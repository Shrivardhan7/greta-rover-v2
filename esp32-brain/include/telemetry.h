/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  telemetry.h  —  Greta V2
//
//  Telemetry is a read-only snapshot of system state sent to the dashboard.
//  It aggregates from multiple modules (state, mode, network, bluetooth,
//  command processor) rather than having modules push into a shared struct.
//  This keeps modules independent and the JSON frame always consistent.
//
//  JSON field keys are defined in config.h (TEL_KEY_* constants).
//  Buffer sizing: 512 bytes. Verify with the ArduinoJson Assistant if you
//  add new fields: https://arduinojson.org/v6/assistant/
//
//  Memory:
//    telemetry_build() writes into a caller-supplied buffer using a
//    stack-allocated StaticJsonDocument — no heap allocation.
// ============================================================================

#include <Arduino.h>

// Initialise telemetry module. Call from main.cpp setup().
void telemetry_init();

// Rate-limited broadcast — builds JSON and calls network_broadcast().
// Interval is TELEMETRY_INTERVAL_MS from config.h. Call every loop().
void telemetry_update();

// Write current telemetry JSON into buf (max bufLen bytes, null-terminated).
// Does not broadcast. Safe to call at any time for debugging.
void telemetry_build(char* buf, size_t bufLen);
