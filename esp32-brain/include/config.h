/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  config.h  —  Greta V2 System Configuration
//
//  Single source of truth for all compile-time constants.
//  No module may use bare numeric literals for timing, pins, or protocol
//  strings. All magic numbers live here so tuning and porting are safe and fast.
//
//  WiFi credentials:
//    Copy wifi_secrets_template.h → wifi_secrets.h (listed in .gitignore).
//    Never commit wifi_secrets.h.
// ============================================================================

// ── Version ───────────────────────────────────────────────────────────────────
#define GRETA_VERSION     "2.0.0-dev"
#define GRETA_BUILD_DATE  __DATE__

// ── WiFi credentials ─────────────────────────────────────────────────────────
#include "wifi_secrets.h"

// ── WiFi connection ───────────────────────────────────────────────────────────
// Multi-SSID fallback: SSID_0 is tried first, then SSID_1, then SSID_2.
// SSID_1 and SSID_2 are enabled by defining them in wifi_secrets.h.
// Each attempt runs for WIFI_CONNECT_TIMEOUT_MS before the next is tried.
#define WIFI_CONNECT_TIMEOUT_MS     8000    // ms per SSID attempt before trying next

// ── mDNS ──────────────────────────────────────────────────────────────────────
#define MDNS_HOSTNAME     "greta"           // Rover reachable at greta.local

// ── WebSocket ─────────────────────────────────────────────────────────────────
#define WS_PORT           81

// ── Bluetooth UART (HC-05 on UART2) ──────────────────────────────────────────
#define BT_BAUD           9600
#define BT_UART_NUM       2
#define BT_TX_PIN         17               // ESP32 TX2 → HC-05 RX
#define BT_RX_PIN         18               // ESP32 RX2 → HC-05 TX
#define BT_RX_BUF_SIZE    64               // Static RX line buffer (bytes)

// ── Safety & Timing ───────────────────────────────────────────────────────────
#define CMD_TIMEOUT_MS              2000   // Auto-STOP if no move cmd in 2 s (MOVING state)
#define BT_ACK_TIMEOUT_MS           1500   // Auto-STOP if no Arduino ACK in 1.5 s
#define BT_SILENCE_TIMEOUT_MS       6000   // STATE_SAFE if Arduino silent for 6 s
#define TELEMETRY_INTERVAL_MS       1000   // Dashboard JSON broadcast interval
#define HEARTBEAT_INTERVAL_MS       5000   // PING → dashboard interval
#define HEARTBEAT_PONG_TIMEOUT_MS  12000   // STATE_SAFE if no PONG within 12 s

// ── Command strings (Dashboard → ESP32 → Arduino) ────────────────────────────
#define CMD_FORWARD   "MOVE F"
#define CMD_BACKWARD  "MOVE B"
#define CMD_LEFT      "MOVE L"
#define CMD_RIGHT     "MOVE R"
#define CMD_STOP      "STOP"
#define CMD_PING      "PING"
#define CMD_PONG      "PONG"
#define CMD_ESTOP     "ESTOP"              // Emergency stop — same priority as STOP

// ── ACK strings (Arduino → ESP32) ────────────────────────────────────────────
#define ACK_FORWARD   "ACK F"
#define ACK_BACKWARD  "ACK B"
#define ACK_LEFT      "ACK L"
#define ACK_RIGHT     "ACK R"
#define ACK_STOP      "ACK STOP"
#define ACK_OBSTACLE  "OBSTACLE"
#define ACK_BOOT      "ACK BOOT"          // Arduino sends on setup() completion

// ── Telemetry JSON field keys ─────────────────────────────────────────────────
// Used in telemetry_build(). Defined here so the dashboard contract is auditable
// without opening telemetry.cpp.
#define TEL_KEY_STATE     "state"
#define TEL_KEY_MODE      "mode"           // Current rover mode (MANUAL / AUTONOMOUS)
#define TEL_KEY_WIFI      "wifi"
#define TEL_KEY_BT        "bt"
#define TEL_KEY_UPTIME    "uptime"
#define TEL_KEY_LAST_CMD  "lastCmd"
#define TEL_KEY_RSSI      "rssi"           // Optional: WiFi signal strength (dBm)
#define TEL_KEY_LATENCY   "latencyMs"      // Optional: last cmd→ack round-trip (ms)
#define TEL_KEY_OBS_TIME  "lastObsTs"      // Optional: millis() of last obstacle event

// ── Feature flags ─────────────────────────────────────────────────────────────
// Comment out a flag to disable that feature at compile time.
#define FEATURE_HEARTBEAT              // PING/PONG watchdog with dashboard
#define FEATURE_TELEMETRY_RSSI         // Include WiFi RSSI in telemetry frame
// #define FEATURE_TELEMETRY_LATENCY   // Include cmd latency in telemetry frame
