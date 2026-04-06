/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  face_engine.h  —  Greta V2  (STUB — interface defined, driver TBD)
//
//  The face engine sits on top of a display driver abstraction (face_display.h).
//  The face engine knows expressions; the driver knows pixels.
//  Swapping OLED for LCD or a colour TFT only requires a new driver —
//  the expression logic above it is unchanged.
//
//  Expressions are driven by FSM state transitions, not by command strings,
//  so the face always reflects actual system state, not last received command.
//
//  Hardware target: OLED (SSD1306) or colour TFT (ST7735 / ILI9341).
//  Implementation: pending display driver selection.
// ============================================================================

#include <Arduino.h>
#include "state_manager.h"   // For RobotState — ensures type consistency

// ── Expression set ────────────────────────────────────────────────────────────
enum FaceExpression : uint8_t {
    FACE_OFFLINE  = 0,
    FACE_BOOT     = 1,
    FACE_READY    = 2,
    FACE_MOVING   = 3,
    FACE_SAFE     = 4,
    FACE_ERROR    = 5,
    FACE_THINKING = 6,    // Future: AI processing indicator
    FACE_COUNT    = 7
};

// ── Eye colour palette ────────────────────────────────────────────────────────
enum EyeColor : uint8_t {
    EYE_WHITE    = 0,
    EYE_CYAN     = 1,
    EYE_BLUE     = 2,
    EYE_GREEN    = 3,
    EYE_PURPLE   = 4,
    EYE_LAVENDER = 5,
    EYE_AMBER    = 6,   // Used for SAFE state
    EYE_RED      = 7,   // Used for ERROR state
};

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void face_init();
void face_update();    // Animation tick — call every loop()

// ── Control ───────────────────────────────────────────────────────────────────
void face_set_expression(FaceExpression expr);
void face_set_eye_color(EyeColor color);

// Called on FSM state transitions — auto-maps RobotState → FaceExpression.
// Register this as a callback from main.cpp after face_init().
void face_on_state_change(RobotState prevState, RobotState newState);
