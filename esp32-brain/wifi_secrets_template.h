#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  wifi_secrets_template.h  —  Greta V2
//
//  INSTRUCTIONS:
//    1. Copy this file to ESP32_Firmware/config/wifi_secrets.h
//    2. Fill in your credentials
//    3. wifi_secrets.h is in .gitignore — it will NEVER be committed
//
//  Priority: SSID_0 is tried first. SSID_1 and SSID_2 are fallbacks.
//  Leave SSID_1 / SSID_2 undefined or empty to disable those slots.
//  SSID_2 is intended for a mobile hotspot (last resort).
// ══════════════════════════════════════════════════════════════════════════════

// Primary network (home WiFi)
#define WIFI_SSID_0  "YOUR_HOME_NETWORK"
#define WIFI_PASS_0  "YOUR_HOME_PASSWORD"

// Backup network (optional — comment out to disable)
// #define WIFI_SSID_1  "YOUR_BACKUP_NETWORK"
// #define WIFI_PASS_1  "YOUR_BACKUP_PASSWORD"

// Mobile hotspot (optional — last resort fallback)
// #define WIFI_SSID_2  "YOUR_HOTSPOT_NAME"
// #define WIFI_PASS_2  "YOUR_HOTSPOT_PASSWORD"
