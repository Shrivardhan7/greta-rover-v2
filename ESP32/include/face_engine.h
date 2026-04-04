#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  face_engine.h  —  Greta V2  (STUB — structure defined, driver TBD)
//
//  Design rationale:
//    The face engine sits on top of a display driver abstraction (face_display.h).
//    The face_engine knows expressions; the driver knows pixels.
//    Swapping OLED for LCD or a colour TFT requires only a new driver
//    implementation — the expression logic above it is unchanged.
//
//    Expressions are driven by state transitions, not by command strings,
//    so the face always reflects actual system state, not last command.
// ══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>

// ─── Expression set ──────────────────────────────────────────────────────────
enum FaceExpression : uint8_t {
    FACE_OFFLINE   = 0,
    FACE_BOOT      = 1,
    FACE_READY     = 2,
    FACE_MOVING    = 3,
    FACE_SAFE      = 4,
    FACE_ERROR     = 5,
    FACE_THINKING  = 6,   // Future: AI processing
    FACE_COUNT     = 7
};

// ─── Eye colour palette ───────────────────────────────────────────────────────
enum EyeColor : uint8_t {
    EYE_WHITE   = 0,
    EYE_CYAN    = 1,
    EYE_BLUE    = 2,
    EYE_GREEN   = 3,
    EYE_PURPLE  = 4,
    EYE_LAVENDER= 5,
    EYE_AMBER   = 6,   // Used for SAFE state
    EYE_RED     = 7,   // Used for ERROR state
};

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void face_init();
void face_update();    // Animation tick — call every loop()

// ─── Control ─────────────────────────────────────────────────────────────────
void face_set_expression(FaceExpression expr);
void face_set_eye_color(EyeColor color);
void face_on_state_change(uint8_t prevState, uint8_t newState);  // Auto-maps state → expression
