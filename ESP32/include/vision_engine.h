#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  vision_engine.h  —  Greta V2  (STUB — not yet implemented)
//
//  Design rationale:
//    The vision engine is isolated behind a clean interface so the control
//    loop is never blocked by frame capture or inference. It runs inference
//    asynchronously (future: on a second core or co-processor) and exposes
//    only detection results, not raw frames.
//
//    The control loop consumes results via vision_get_result() on each tick.
//    No vision result ever directly modifies robot state — only the command
//    processor and state manager do that. Vision is advisory.
//
//  Hardware target: ESP32-CAM or OV2640 over I2C/SPI.
// ══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>

// ─── Detection result ─────────────────────────────────────────────────────────
enum VisionDetection : uint8_t {
    VISION_NONE        = 0,
    VISION_PERSON      = 1,
    VISION_PET         = 2,
    VISION_FACE        = 3,
    VISION_OBSTACLE    = 4,    // Visual obstacle (complements ultrasonic)
    VISION_LINE        = 5,    // For future line-following mode
};

struct VisionResult {
    VisionDetection detection;
    uint8_t         confidence;    // 0–100
    uint32_t        timestamp_ms;
};

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void vision_init();
void vision_update();   // Non-blocking — queues async capture; call every loop()

// ─── Results ─────────────────────────────────────────────────────────────────
bool         vision_result_ready();
VisionResult vision_get_result();   // Returns last detection; clears ready flag
