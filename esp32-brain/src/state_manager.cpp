/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

// ============================================================================
//  state_manager.cpp — Robot State Machine (FSM)
//
//  States: BOOT → CONNECTING → READY ↔ MOVING
//                                    ↘ SAFE → READY
//                                    ↘ ERROR
//
//  All state transitions go through state_set(). Illegal transitions are
//  logged and blocked — no silent state corruption is possible.
//
//  Safety note: STATE_SAFE is the fallback for all link-loss and timeout
//  events. STATE_ERROR is reserved for unrecoverable faults. In neither
//  state will command_processor forward movement commands to the Arduino.
// ============================================================================

#include "state_manager.h"
#include <Arduino.h>

// ── Private state ─────────────────────────────────────────────────────────────
static RobotState _state            = STATE_BOOT;
static char       _last_reason[48]  = "";    // Static buffer — no heap allocation
static uint32_t   _entered_ms       = 0;

// ── Name table ────────────────────────────────────────────────────────────────
// Stored in ROM via const. Index matches the RobotState enum.
static const char* const STATE_NAMES[STATE_COUNT] = {
    "BOOT", "CONNECTING", "READY", "MOVING", "SAFE", "ERROR"
};

static const char* _name_of(RobotState s) {
    if (s < STATE_COUNT) return STATE_NAMES[s];
    return "UNKNOWN";
}

// ── Transition guard table ────────────────────────────────────────────────────
// ALLOWED_FROM[from] is a bitmask: bit N set means transition to state N is allowed.
//
// Reading the table:
//   BOOT       → CONNECTING only               (system must connect before anything else)
//   CONNECTING → READY | ERROR                 (WiFi+BT up, or a fault occurred)
//   READY      → MOVING | SAFE | ERROR         (normal operation paths)
//   MOVING     → READY | SAFE | ERROR          (stop, link loss, or fault)
//   SAFE       → READY only                    (recovery when links restore)
//   ERROR      → CONNECTING                    (manual reset / reboot recovery)
//
// Bit index:      BOOT CONN READY MOV SAFE ERROR
//                   0    1    2    3    4    5
static const uint8_t ALLOWED_FROM[STATE_COUNT] = {
    0b00000010,   // BOOT       → CONNECTING
    0b00100100,   // CONNECTING → READY(2) | ERROR(5)
    0b00111100,   // READY      → MOVING(3) | SAFE(4) | ERROR(5)
    0b00110100,   // MOVING     → READY(2) | SAFE(4) | ERROR(5)
    0b00000100,   // SAFE       → READY(2) only
    0b00000010,   // ERROR      → CONNECTING(1) — after reset
};

static bool _transition_allowed(RobotState from, RobotState to) {
    if (from >= STATE_COUNT || to >= STATE_COUNT) return false;
    return (ALLOWED_FROM[from] >> to) & 0x01;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void state_init() {
    _state          = STATE_BOOT;
    _entered_ms     = millis();
    _last_reason[0] = '\0';
    Serial.println(F("[STATE] init → BOOT"));
}

// Reserved for future timed transitions (e.g. ERROR auto-recovery timer).
void state_update() {}

// ── Mutator ───────────────────────────────────────────────────────────────────
void state_set(RobotState next, const char* reason) {
    if (_state == next) return;

    if (!_transition_allowed(_state, next)) {
        // Log the blocked transition but do not assert — keep the rover running.
        Serial.printf("[STATE] BLOCKED %s → %s (reason: %s)\n",
                      _name_of(_state), _name_of(next), reason ? reason : "");
        return;
    }

    Serial.printf("[STATE] %s → %s  reason='%s'\n",
                  _name_of(_state), _name_of(next), reason ? reason : "");

    if (reason && reason[0]) {
        strncpy(_last_reason, reason, sizeof(_last_reason) - 1);
        _last_reason[sizeof(_last_reason) - 1] = '\0';
    }

    _state      = next;
    _entered_ms = millis();
}

// ── Accessors ────────────────────────────────────────────────────────────────
RobotState  state_get()          { return _state; }
const char* state_name()         { return _name_of(_state); }
const char* state_last_reason()  { return _last_reason; }
uint32_t    state_entered_ms()   { return _entered_ms; }
