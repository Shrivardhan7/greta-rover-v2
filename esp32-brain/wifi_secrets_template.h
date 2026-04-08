/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once

#error "GRETA OS build blocked: copy wifi_secrets_template.h to esp32-brain/wifi_secrets.h and include that local file instead."

// ============================================================================
//  wifi_secrets_template.h  —  Greta V2 / ESP32-S3 WiFi Credentials Template
//
//  SETUP INSTRUCTIONS:
//    1. Copy this file locally to:  esp32-brain/wifi_secrets.h
//    2. Fill in your network credentials below.
//    3. config.h expects wifi_secrets.h to exist for local builds.
//    4. wifi_secrets.h is listed in .gitignore — it will never be committed.
//
//  NETWORK PRIORITY:
//    SSID_0  →  Primary network (home WiFi) — tried first
//    SSID_1  →  Backup network (optional)   — tried if SSID_0 fails
//    SSID_2  →  Mobile hotspot (optional)   — last resort fallback
//
//  To disable SSID_1 or SSID_2: leave them commented out.
// ============================================================================

// ----------------------------------------------------------------------------
// Primary Network — Home WiFi
// ----------------------------------------------------------------------------
#define WIFI_SSID_0  "YOUR_HOME_NETWORK"
#define WIFI_PASS_0  "YOUR_HOME_PASSWORD"

// ----------------------------------------------------------------------------
// Backup Network — Optional (uncomment to enable)
// ----------------------------------------------------------------------------
// #define WIFI_SSID_1  "YOUR_BACKUP_NETWORK"
// #define WIFI_PASS_1  "YOUR_BACKUP_PASSWORD"

// ----------------------------------------------------------------------------
// Mobile Hotspot — Optional, last resort (uncomment to enable)
// ----------------------------------------------------------------------------
// #define WIFI_SSID_2  "YOUR_HOTSPOT_NAME"
// #define WIFI_PASS_2  "YOUR_HOTSPOT_PASSWORD"

