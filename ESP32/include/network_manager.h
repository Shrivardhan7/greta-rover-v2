#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  network_manager.h  —  Greta V2
//
//  Design rationale:
//    Owns the WiFi physical link and the WebSocket application layer.
//    Responsible for:
//      • Multi-SSID connection with priority fallback
//      • WiFi watchdog → STATE_SAFE on link loss
//      • mDNS registration (greta.local)
//      • WebSocket server lifecycle
//      • PING heartbeat to detect stale browser connections
//      • Routing inbound WS text frames to command_processor
//
//    The network layer is deliberately thin — it does not interpret commands
//    or make safety decisions. It delivers raw strings to the command processor
//    and broadcasts strings from the system to the dashboard.
// ══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>

// ─── Callback type ────────────────────────────────────────────────────────────
typedef void (*CommandCallback)(const char* cmd);

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void network_init();
void network_update();    // WiFi watchdog + WS pump — call every loop()

// ─── Control ─────────────────────────────────────────────────────────────────
void network_set_command_callback(CommandCallback cb);

// ─── TX ──────────────────────────────────────────────────────────────────────
void network_broadcast(const char* msg);

// ─── Heartbeat ───────────────────────────────────────────────────────────────
// Called by command_processor when dashboard replies PONG
void network_on_pong();

// ─── Status ──────────────────────────────────────────────────────────────────
bool        network_wifi_ok();
bool        network_ws_client_connected();
int8_t      network_rssi();        // Current RSSI in dBm; 0 if disconnected
const char* network_ssid();        // Active SSID, empty string if disconnected
