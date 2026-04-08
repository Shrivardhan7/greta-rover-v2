/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

// ============================================================================
//  network_manager.cpp — WiFi Connection + WebSocket Server
//
//  Responsibilities:
//    - Connects to WiFi using a prioritised list of networks (from wifi_secrets.h)
//    - Starts a WebSocket server on WS_PORT once WiFi is up
//    - Registers mDNS so the dashboard can reach the rover at <hostname>.local
//    - Calls the registered CommandCallback on each incoming WebSocket frame
//    - Broadcasts JSON strings to all connected dashboard clients
//
//  Multi-SSID fallback:
//    Networks are tried in order: SSID_0 (primary), SSID_1, SSID_2 (hotspot).
//    If a connection attempt exceeds WIFI_CONNECT_TIMEOUT_MS, the next network
//    in the list is tried. The list wraps around indefinitely.
//
//  Safety:
//    - WiFi loss triggers STATE_SAFE immediately.
//    - WebSocket client disconnect triggers STATE_SAFE.
//    - Heartbeat (PING/PONG) monitors dashboard presence while connected.
//      If no PONG is received within HEARTBEAT_PONG_TIMEOUT_MS, STATE_SAFE is set.
//    - All safety transitions go through state_set() — network_manager does
//      not command motors directly.
//
//  Heartbeat feature flag:
//    Guarded by #ifdef FEATURE_HEARTBEAT (defined in config.h).
//    Disable during development to reduce serial noise.
// ============================================================================

#include "network_manager.h"
#include "behavior_manager.h"
#include "config.h"
#include "health_manager.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebSocketsServer.h>
#include <Arduino.h>

// ── Multi-SSID credential table ───────────────────────────────────────────────
// Populated from wifi_secrets.h — define WIFI_SSID_n / WIFI_PASS_n there.
// This file is in .gitignore and must never be committed.
struct WifiCredential {
    const char* ssid;
    const char* pass;
};

static const WifiCredential WIFI_NETWORKS[] = {
    { WIFI_SSID_0, WIFI_PASS_0 },
#ifdef WIFI_SSID_1
    { WIFI_SSID_1, WIFI_PASS_1 },
#endif
#ifdef WIFI_SSID_2
    { WIFI_SSID_2, WIFI_PASS_2 },
#endif
};
static const uint8_t WIFI_NETWORK_COUNT =
    sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// ── Private state ─────────────────────────────────────────────────────────────
static WebSocketsServer _ws(WS_PORT);
static CommandCallback  _cmdCb          = nullptr;
static bool             _wifiOk         = false;
static bool             _wsActive       = false;   // At least one client connected
static uint8_t          _ssidIndex      = 0;
static uint32_t         _connectStartMs = 0;
static uint32_t         _lastPingMs     = 0;
static uint32_t         _lastPongMs     = 0;
static bool             _heartbeatArmed = false;

// ── Forward declarations ──────────────────────────────────────────────────────
static void _start_connect(uint8_t idx);
static void _on_wifi_up();
static void _ws_event(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void network_init() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);    // We manage reconnection explicitly
    _ssidIndex      = 0;
    _connectStartMs = millis();
    _start_connect(_ssidIndex);
}

void network_update() {
    const uint32_t now = millis();

    // ── WiFi state machine ────────────────────────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
        if (_wifiOk) {
            // Link was up — just dropped
            _wifiOk   = false;
            _wsActive = false;
            Serial.println(F("[NET] WiFi lost → SAFE"));
            behavior_force_safe("WiFi lost");
        }

        // Move to the next SSID if this attempt has timed out
        if ((now - _connectStartMs) >= WIFI_CONNECT_TIMEOUT_MS) {
            _ssidIndex = (_ssidIndex + 1) % WIFI_NETWORK_COUNT;
            Serial.printf("[NET] Timeout — trying SSID[%d]: %s\n",
                          _ssidIndex, WIFI_NETWORKS[_ssidIndex].ssid);
            _start_connect(_ssidIndex);
        }
        return;
    }

    // WiFi connected — run WebSocket loop
    if (!_wifiOk) {
        _wifiOk = true;
        _on_wifi_up();
    }

    health_manager_record_rssi(WiFi.RSSI());

    _ws.loop();

    // ── Heartbeat PING / PONG ────────────────────────────────────────────────
#ifdef FEATURE_HEARTBEAT
    if (_wsActive && _heartbeatArmed) {
        if ((now - _lastPingMs) >= HEARTBEAT_INTERVAL_MS) {
            _ws.broadcastTXT("{\"event\":\"PING\"}");
            _lastPingMs = now;
        }
        // PONG watchdog — declares dashboard offline if it stops responding
        if ((now - _lastPongMs) >= HEARTBEAT_PONG_TIMEOUT_MS) {
            Serial.println(F("[NET] Heartbeat timeout → SAFE"));
            behavior_force_safe("heartbeat timeout");
            _heartbeatArmed = false;
        }
    }
#endif
}

// ── TX ────────────────────────────────────────────────────────────────────────
void network_broadcast(const char* msg) {
    if (_wifiOk && msg) {
        _ws.broadcastTXT(msg);
    }
}

// ── Heartbeat ─────────────────────────────────────────────────────────────────
// Called by command_processor when a PONG frame arrives from the dashboard.
void network_on_pong() {
    _lastPongMs = millis();
    Serial.println(F("[NET] PONG received"));
}

// ── Status ────────────────────────────────────────────────────────────────────
bool        network_wifi_ok()             { return _wifiOk; }
bool        network_ws_client_connected() { return _wsActive; }
int8_t      network_rssi()                { return _wifiOk ? (int8_t)WiFi.RSSI() : 0; }
const char* network_ssid()               { return _wifiOk ? WiFi.SSID().c_str() : ""; }

void network_set_command_callback(CommandCallback cb) { _cmdCb = cb; }

// ── Private ───────────────────────────────────────────────────────────────────
static void _start_connect(uint8_t idx) {
    WiFi.disconnect(true);
    Serial.printf("[NET] Connecting to '%s'...\n", WIFI_NETWORKS[idx].ssid);
    WiFi.begin(WIFI_NETWORKS[idx].ssid, WIFI_NETWORKS[idx].pass);
    _connectStartMs = millis();
}

static void _on_wifi_up() {
    Serial.printf("[NET] WiFi UP  SSID='%s'  IP=%s  RSSI=%d dBm\n",
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());

    if (MDNS.begin(MDNS_HOSTNAME)) {
        Serial.printf("[NET] mDNS → %s.local\n", MDNS_HOSTNAME);
    } else {
        Serial.println(F("[NET] mDNS init failed"));
    }

    _ws.begin();
    _ws.onEvent(_ws_event);
    Serial.printf("[NET] WebSocket listening on port %d\n", WS_PORT);

    _lastPongMs     = millis();   // Arm heartbeat from first connection
    _heartbeatArmed = false;      // Don't ping until a client connects
}

static void _ws_event(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {

        case WStype_CONNECTED:
            _wsActive = true;
            Serial.printf("[WS] Client #%u connected\n", num);
            _lastPongMs     = millis();
            _heartbeatArmed = true;
            break;

        case WStype_DISCONNECTED:
            Serial.printf("[WS] Client #%u disconnected\n", num);
            // Conservative: declare inactive and go to SAFE.
            // WebSocketsServer doesn't expose a live client count cleanly,
            // so we go safe and let the browser auto-reconnect to recover.
            _wsActive       = false;
            _heartbeatArmed = false;
            behavior_force_safe("WS client disconnected");
            break;

        case WStype_TEXT: {
            if (!payload || length == 0) break;

            // Null-terminate in place (payload is library-managed buffer)
            payload[length] = '\0';

            // Trim trailing whitespace
            char* p = (char*)payload;
            while (length > 0 &&
                   (p[length-1] == ' ' || p[length-1] == '\r' || p[length-1] == '\n')) {
                p[--length] = '\0';
            }

            Serial.printf("[WS] RX #%u: %s\n", num, p);
            if (_cmdCb) _cmdCb(p);
            break;
        }

        default:
            break;
    }
}
