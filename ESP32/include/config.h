#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  config.h  —  Greta V2 System Configuration
//  Single source of truth for all compile-time constants.
//
//  Design rationale:
//    All magic numbers live here. No module may use bare numeric literals
//    for timing, pin assignments, or protocol strings. This makes tuning,
//    porting, and auditing safe and fast.
//
//  WiFi credentials: copy wifi_secrets_template.h → wifi_secrets.h (git-ignored)
// ══════════════════════════════════════════════════════════════════════════════

// ─── Version ──────────────────────────────────────────────────────────────────
#define GRETA_VERSION     "2.0.0"
#define GRETA_BUILD_DATE  __DATE__

// ─── WiFi credentials ────────────────────────────────────────────────────────
#include "wifi_secrets.h"

// ─── WiFi — multi-network fallback ───────────────────────────────────────────
// Priority order: SSID_0 → SSID_1 → SSID_2 (hotspot).
// Leave SSID_1 / SSID_2 empty strings to disable those slots.
// Credentials for each slot come from wifi_secrets.h.
#define WIFI_SSID_COUNT       3
#define WIFI_CONNECT_TIMEOUT_MS  8000    // Per-SSID attempt window
#define WIFI_RECONNECT_INTERVAL_MS 5000  // Retry interval after total failure

// ─── mDNS ────────────────────────────────────────────────────────────────────
#define MDNS_HOSTNAME     "greta"        // Resolves as greta.local

// ─── WebSocket ───────────────────────────────────────────────────────────────
#define WS_PORT           81

// ─── Bluetooth (HC-05 on UART2) ──────────────────────────────────────────────
#define BT_BAUD           9600
#define BT_UART_NUM       2
#define BT_TX_PIN         17             // ESP32 TX2 → HC-05 RX
#define BT_RX_PIN         18             // ESP32 RX2 → HC-05 TX
#define BT_RX_BUF_SIZE    64             // Static line buffer — no heap String growth

// ─── Safety & Timing ─────────────────────────────────────────────────────────
#define CMD_TIMEOUT_MS          2000     // STOP if no movement cmd in 2 s (MOVING state)
#define BT_ACK_TIMEOUT_MS       1500     // STOP if no ACK in 1.5 s
#define BT_SILENCE_TIMEOUT_MS   6000     // SAFE if no bytes from Arduino in 6 s
#define TELEMETRY_INTERVAL_MS   1000     // JSON telemetry broadcast interval
#define HEARTBEAT_INTERVAL_MS   5000     // PING → dashboard interval
#define HEARTBEAT_PONG_TIMEOUT_MS 12000  // Max silence from dashboard before SAFE

// ─── Command strings (Dashboard → ESP32 → Arduino) ───────────────────────────
#define CMD_FORWARD   "MOVE F"
#define CMD_BACKWARD  "MOVE B"
#define CMD_LEFT      "MOVE L"
#define CMD_RIGHT     "MOVE R"
#define CMD_STOP      "STOP"
#define CMD_PING      "PING"
#define CMD_PONG      "PONG"
#define CMD_ESTOP     "ESTOP"           // Emergency stop — same priority as STOP

// ─── ACK strings (Arduino → ESP32) ───────────────────────────────────────────
#define ACK_FORWARD   "ACK F"
#define ACK_BACKWARD  "ACK B"
#define ACK_LEFT      "ACK L"
#define ACK_RIGHT     "ACK R"
#define ACK_STOP      "ACK STOP"
#define ACK_OBSTACLE  "OBSTACLE"
#define ACK_BOOT      "ACK BOOT"       // Arduino sends this on setup() completion

// ─── Telemetry field keys ────────────────────────────────────────────────────
// Using literals here so telemetry_build() stays auditable without jumping files.
#define TEL_KEY_STATE     "state"
#define TEL_KEY_WIFI      "wifi"
#define TEL_KEY_BT        "bt"
#define TEL_KEY_UPTIME    "uptime"
#define TEL_KEY_LAST_CMD  "lastCmd"
#define TEL_KEY_RSSI      "rssi"        // Optional — WiFi signal strength (dBm)
#define TEL_KEY_LATENCY   "latencyMs"   // Optional — last cmd→ack round-trip ms
#define TEL_KEY_OBS_TIME  "lastObsTs"   // Optional — millis() of last obstacle

// ─── Feature flags (comment to disable) ─────────────────────────────────────
#define FEATURE_HEARTBEAT               // Enable PING/PONG watchdog with dashboard
#define FEATURE_TELEMETRY_RSSI          // Include WiFi RSSI in telemetry
// #define FEATURE_TELEMETRY_LATENCY    // Include cmd latency (requires tracking)
// #define FEATURE_MULTI_SSID           // Enable multi-SSID fallback (needs secrets)
