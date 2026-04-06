/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  vision_engine.h  —  Greta V2  (STUB — interface defined, driver TBD)
//
//  The vision engine is isolated behind a clean interface so the control
//  loop is never blocked by frame capture or inference. It runs inference
//  asynchronously (future: on a second core or co-processor) and exposes
//  only detection results — not raw frames.
//
//  The control loop reads results via vision_get_result() on each tick.
//  Vision results are advisory only — they never directly modify robot state.
//  Only command_processor and state_manager change robot state.
//
//  Hardware target: ESP32-CAM module or OV2640 sensor over SCCB/I2C.
//  Implementation: pending camera driver integration.
// ============================================================================

#include <Arduino.h>

// ── Detection types ───────────────────────────────────────────────────────────
enum VisionDetection : uint8_t {
    VISION_NONE      = 0,
    VISION_PERSON    = 1,
    VISION_PET       = 2,
    VISION_FACE      = 3,
    VISION_OBSTACLE  = 4,   // Visual obstacle (complements ultrasonic sensor)
    VISION_LINE      = 5,   // Future: line-following mode
};

// ── Detection result ─────────────────────────────────────────────────────────
struct VisionResult {
    VisionDetection detection;
    uint8_t         confidence;     // 0–100
    uint32_t        timestamp_ms;
};

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void vision_init();
void vision_update();   // Non-blocking — queues async frame capture; call every loop()

// ── Results ───────────────────────────────────────────────────────────────────
bool         vision_result_ready();
VisionResult vision_get_result();   // Returns last detection; clears the ready flag
