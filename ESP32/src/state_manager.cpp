#include "state_manager.h"
#include <Arduino.h>

// ─── Private state ────────────────────────────────────────────────────────────
static RobotState _state         = STATE_BOOT;
static char       _last_reason[48] = "";      // Static buffer — no heap
static uint32_t   _entered_ms    = 0;

// ─── Name table (ROM, no heap) ────────────────────────────────────────────────
static const char* const STATE_NAMES[STATE_COUNT] = {
    "BOOT", "CONNECTING", "READY", "MOVING", "SAFE", "ERROR"
};

static const char* _name_of(RobotState s) {
    if (s < STATE_COUNT) return STATE_NAMES[s];
    return "UNKNOWN";
}

// ─── Transition guard table ──────────────────────────────────────────────────
// [from][to] = allowed?
// Encoding as bitmask per source state: bit N set → transition to state N allowed.
// STOP / safety events must always reach SAFE; ESTOP can always go to READY.
static const uint8_t ALLOWED_FROM[STATE_COUNT] = {
    //  BOOT  CONN  READY MOV  SAFE  ERR  ← destination bit index
    0b00000010,  // BOOT       → CONNECTING only
    0b00100100,  // CONNECTING → READY | ERROR
    0b00111100,  // READY      → MOVING | SAFE | ERROR | (back to CONNECTING on reset)
    0b00010100,  // MOVING     → READY | SAFE | ERROR
    0b00000100,  // SAFE       → READY only (automatic recovery)
    0b00000010,  // ERROR      → CONNECTING (after reset)
};

static bool _transition_allowed(RobotState from, RobotState to) {
    if (from >= STATE_COUNT || to >= STATE_COUNT) return false;
    return (ALLOWED_FROM[from] >> to) & 0x01;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────
void state_init() {
    _state      = STATE_BOOT;
    _entered_ms = millis();
    _last_reason[0] = '\0';
    Serial.println(F("[STATE] init → BOOT"));
}

void state_update() {
    // Reserved for future timed transitions (e.g. ERROR auto-recovery timer).
}

// ─── Mutator ──────────────────────────────────────────────────────────────────
void state_set(RobotState next, const char* reason) {
    if (_state == next) return;

    // Enforce transition guard — block illegal transitions
    if (!_transition_allowed(_state, next)) {
        Serial.printf(F("[STATE] BLOCKED %s → %s (%s)\n"),
                      _name_of(_state), _name_of(next), reason);
        return;
    }

    Serial.printf("[STATE] %s → %s  reason='%s'\n",
                  _name_of(_state), _name_of(next), reason ? reason : "");

    // Store reason in static buffer — truncate safely
    if (reason && reason[0]) {
        strncpy(_last_reason, reason, sizeof(_last_reason) - 1);
        _last_reason[sizeof(_last_reason) - 1] = '\0';
    }

    _state      = next;
    _entered_ms = millis();
}

// ─── Accessors ────────────────────────────────────────────────────────────────
RobotState  state_get()          { return _state; }
const char* state_name()         { return _name_of(_state); }
const char* state_last_reason()  { return _last_reason; }
uint32_t    state_entered_ms()   { return _entered_ms; }
