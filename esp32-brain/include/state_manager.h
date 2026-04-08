/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  state_manager.h  —  Greta V2
//
//  The FSM is the system's single source of truth for operational safety.
//  No module sets motor state directly. No module decides "safe or not"
//  independently. All decisions flow through state_set().
//
//  All transitions are logged with a reason string so that post-mortem
//  analysis of safety events is possible from the serial monitor.
//
//  Valid state transitions:
//    BOOT       → CONNECTING
//    CONNECTING → READY | ERROR
//    READY      → MOVING | SAFE | ERROR
//    MOVING     → READY | SAFE | ERROR
//    SAFE       → READY  (automatic, when links are restored)
//    ERROR      → CONNECTING  (manual reset / reboot only)
//
//  Illegal transitions are blocked and logged — they never silently succeed.
// ============================================================================

#include <Arduino.h>

// ── State enumeration ─────────────────────────────────────────────────────────
enum RobotState : uint8_t {
    STATE_BOOT       = 0,
    STATE_CONNECTING = 1,
    STATE_READY      = 2,
    STATE_MOVING     = 3,
    STATE_SAFE       = 4,   // All movement blocked; rover is stopped
    STATE_ERROR      = 5,   // Unrecoverable fault; requires reset
    STATE_COUNT      = 6    // Sentinel — keep last
};

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void state_init();
void state_update();    // Reserved for timed transitions — call every loop()

// ── Mutator ───────────────────────────────────────────────────────────────────
// Request a state transition. reason is stored in a static buffer (no heap).
// Illegal transitions are blocked and logged; they never silently succeed.
void state_set(RobotState next, const char* reason = "");

// ── Accessors ────────────────────────────────────────────────────────────────
RobotState  state_get();            // Current state
const char* state_name();           // Current state as a human-readable string
const char* state_last_reason();    // Reason string for the last transition
uint32_t    state_entered_ms();     // millis() when the current state was entered

// ── Guards ────────────────────────────────────────────────────────────────────
// Use these instead of scattered (state == X || state == Y) checks.

// True if the rover is allowed to accept movement commands.
inline bool state_can_move()  { return state_get() == STATE_READY ||
                                       state_get() == STATE_MOVING; }

// True if the rover is in a halted safety state (SAFE or ERROR).
inline bool state_is_halted() { return state_get() == STATE_SAFE  ||
                                       state_get() == STATE_ERROR; }

// True if the rover has completed boot and is operational (READY or beyond).
inline bool state_is_online() { return state_get() >= STATE_READY; }
