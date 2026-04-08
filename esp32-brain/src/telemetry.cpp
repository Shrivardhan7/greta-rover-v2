/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

// ============================================================================
//  telemetry.cpp — JSON Telemetry Broadcast to Dashboard
//
//  What it does:
//    Builds a JSON status frame every TELEMETRY_INTERVAL_MS and broadcasts
//    it to all connected WebSocket clients via network_broadcast().
//
//  Frame fields (always present):
//    state    — current FSM state name (e.g. "READY", "MOVING", "SAFE")
//    mode     — current rover mode (e.g. "MANUAL", "AUTONOMOUS")
//    wifi     — "OK" or "LOST"
//    bt       — "OK" or "LOST"
//    uptime   — seconds since boot
//    last_cmd — last command forwarded to Arduino
//
//  Optional fields (compile-time flags in config.h):
//    rssi     — WiFi signal strength in dBm  (FEATURE_TELEMETRY_RSSI)
//    latency  — last BT round-trip time in ms (FEATURE_TELEMETRY_LATENCY)
//
//  Memory:
//    StaticJsonDocument lives on the stack — no heap allocation.
//    _jsonBuf is a static char array — reused each tick.
//    Buffer size is 256 bytes; verify with ArduinoJson Assistant if you
//    add new fields: https://arduinojson.org/v6/assistant/
// ============================================================================

#include "telemetry.h"
#include "config.h"
#include "state_manager.h"
#include "network_manager.h"
#include "bluetooth_bridge.h"
#include "command_processor.h"
#include "mode_manager.h"
#include <ArduinoJson.h>
#include <Arduino.h>

// ── Private ───────────────────────────────────────────────────────────────────
static uint32_t _lastBroadcastMs = 0;
static char     _jsonBuf[256];      // Static output buffer — no heap allocation

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void telemetry_init() {
    _lastBroadcastMs = millis();
    Serial.println(F("[TEL] init"));
}

void telemetry_update() {
    const uint32_t now = millis();
    if ((now - _lastBroadcastMs) < TELEMETRY_INTERVAL_MS) return;
    _lastBroadcastMs = now;

    telemetry_build(_jsonBuf, sizeof(_jsonBuf));
    network_broadcast(_jsonBuf);
}

// ── Build ──────────────────────────────────────────────────────────────────────
// Populates buf with a JSON telemetry frame. Does not allocate from heap.
// buf must be at least 256 bytes. Caller owns the buffer.
void telemetry_build(char* buf, size_t bufLen) {
    StaticJsonDocument<256> doc;   // Stack-allocated; freed when function returns

    // Core fields — always present
    doc[TEL_KEY_STATE]    = state_name();
    doc[TEL_KEY_MODE]     = mode_name();
    doc[TEL_KEY_WIFI]     = network_wifi_ok()     ? "OK" : "LOST";
    doc[TEL_KEY_BT]       = bluetooth_connected() ? "OK" : "LOST";
    doc[TEL_KEY_UPTIME]   = millis() / 1000;
    doc[TEL_KEY_LAST_CMD] = command_last();

    // Optional fields — enabled in config.h
#ifdef FEATURE_TELEMETRY_RSSI
    doc[TEL_KEY_RSSI]     = network_rssi();
#endif

#ifdef FEATURE_TELEMETRY_LATENCY
    const uint32_t lat = command_last_latency_ms();
    if (lat > 0) doc[TEL_KEY_LATENCY] = lat;
#endif

    serializeJson(doc, buf, bufLen);
}
