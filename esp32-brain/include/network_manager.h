/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  network_manager.h  —  Greta V2
//
//  Owns the WiFi physical link and the WebSocket application layer.
//  Responsible for:
//    - Multi-SSID connection with priority fallback (SSID_0 → SSID_1 → SSID_2)
//    - WiFi watchdog → STATE_SAFE on link loss
//    - mDNS registration (greta.local)
//    - WebSocket server lifecycle
//    - PING heartbeat to detect stale browser sessions
//    - Routing inbound WebSocket text frames to the registered CommandCallback
//
//  This module is deliberately thin — it does not interpret commands or make
//  safety decisions. It delivers raw strings to command_processor and
//  broadcasts raw strings from the system to the dashboard.
// ============================================================================

#include <Arduino.h>

// ── Callback type ─────────────────────────────────────────────────────────────
// Called by network_manager on each validated inbound WebSocket text frame.
// Register via network_set_command_callback() before network_init().
typedef void (*CommandCallback)(const char* cmd);

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void network_init();
void network_update();    // WiFi state machine + WebSocket pump — call every loop()

// ── Control ───────────────────────────────────────────────────────────────────
void network_set_command_callback(CommandCallback cb);

// ── TX ────────────────────────────────────────────────────────────────────────
void network_broadcast(const char* msg);   // Broadcast JSON string to all WS clients

// ── Heartbeat ─────────────────────────────────────────────────────────────────
// Called by command_processor when a PONG frame arrives from the dashboard.
void network_on_pong();

// ── Status ────────────────────────────────────────────────────────────────────
bool        network_wifi_ok();               // True if WiFi is currently connected
bool        network_ws_client_connected();   // True if at least one WS client is active
int8_t      network_rssi();                  // RSSI in dBm; returns 0 if disconnected
const char* network_ssid();                  // Active SSID; empty string if disconnected
