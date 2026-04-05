#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  state_manager.h  —  Greta V2
//
//  Design rationale:
//    The FSM is the system's single source of truth. No module sets motor
//    state, no module decides "safe or not" — the state machine does.
//    All transitions are logged with a reason string so post-mortem analysis
//    of safety events is straightforward.
//
//  Valid transitions:
//    BOOT       → CONNECTING
//    CONNECTING → READY | ERROR
//    READY      → MOVING | SAFE | ERROR
//    MOVING     → READY | SAFE | ERROR
//    SAFE       → READY (auto, when links restored)
//    ERROR      → CONNECTING (manual reset only)
// ══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>

// ─── State enumeration ────────────────────────────────────────────────────────
enum RobotState : uint8_t {
    STATE_BOOT       = 0,
    STATE_CONNECTING = 1,
    STATE_READY      = 2,
    STATE_MOVING     = 3,
    STATE_SAFE       = 4,
    STATE_ERROR      = 5,
    STATE_COUNT      = 6    // Sentinel — keep last
};

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void state_init();
void state_update();     // Call every loop() — reserved for timed transitions

// ─── Accessors ────────────────────────────────────────────────────────────────
RobotState  state_get();
const char* state_name();
const char* state_last_reason();     // Why did the last transition occur?
uint32_t    state_entered_ms();      // millis() when current state was entered

// ─── Mutator ─────────────────────────────────────────────────────────────────
// reason: short, human-readable, ASCII only — stored in static buffer (no heap)
void state_set(RobotState next, const char* reason = "");

// ─── Guards ──────────────────────────────────────────────────────────────────
// These replace scattered (state == X || state == Y) checks across modules.
inline bool state_can_move()   { return state_get() == STATE_READY || state_get() == STATE_MOVING; }
inline bool state_is_halted()  { return state_get() == STATE_SAFE  || state_get() == STATE_ERROR; }
inline bool state_is_online()  { return state_get() >= STATE_READY; }
