/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

// ============================================================================
//  mode_manager.cpp — Rover Operating Mode
//
//  Modes:
//    MODE_MANUAL     — Dashboard sends movement commands directly (default)
//    MODE_AUTONOMOUS — Onboard logic controls movement; dashboard commands blocked
//    MODE_VOICE      — Voice commands control movement (future)
//    MODE_SAFE       — All movement blocked; only STOP commands accepted
//
//  Mode changes arrive from the dashboard as "MODE MANUAL", "MODE AUTONOMOUS"
//  etc. via network_manager. See integration notes below for wiring.
//
//  Safety:
//    MODE_AUTONOMOUS and MODE_VOICE are rejected when the system is halted
//    (STATE_SAFE or STATE_ERROR). This prevents autonomous mode from being
//    activated during a fault condition.
//
//  Integration — add these calls to existing files:
//
//    main.cpp setup():
//      mode_init();              // after command_init()
//
//    main.cpp loop() scheduler block:
//      if (scheduler_due(TASK_MODE)) mode_update();  // if a TASK_MODE is added
//
//    command_processor.cpp command_receive(), before the state gate check:
//      if (mode_get() == MODE_SAFE && !_is_stop_cmd(cmd)) return;
//      if (mode_get() == MODE_AUTONOMOUS && _is_move_cmd(cmd)) return;
//
//    network_manager.cpp _ws_event() WStype_TEXT case, before _cmdCb:
//      if (strncmp(p, "MODE ", 5) == 0) {
//          mode_receive(p + 5);
//          return;
//      }
//
//    telemetry.cpp telemetry_build():
//      doc["mode"] = mode_name();
// ============================================================================

#include "mode_manager.h"
#include "network_manager.h"
#include "state_manager.h"
#include <Arduino.h>

// ── Private state ─────────────────────────────────────────────────────────────
static RoverMode _mode = MODE_MANUAL;

static const char* const MODE_NAMES[MODE_COUNT] = {
    "MANUAL", "AUTONOMOUS", "VOICE", "SAFE"
};

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void mode_init() {
    _mode = MODE_MANUAL;
    Serial.println(F("[MODE] init → MANUAL"));
}

// Reserved for future use — e.g. auto-timeout back to MANUAL if autonomous stalls.
void mode_update() {}

// ── Mode change — from dashboard ("MODE MANUAL" etc.) ────────────────────────
void mode_receive(const char* modeStr) {
    if (!modeStr) return;

    // Parse the mode string
    RoverMode requested = MODE_COUNT;   // MODE_COUNT is used as a sentinel (invalid)

    if      (strcmp(modeStr, "MANUAL")     == 0) requested = MODE_MANUAL;
    else if (strcmp(modeStr, "AUTONOMOUS") == 0) requested = MODE_AUTONOMOUS;
    else if (strcmp(modeStr, "VOICE")      == 0) requested = MODE_VOICE;
    else if (strcmp(modeStr, "SAFE")       == 0) requested = MODE_SAFE;

    if (requested == MODE_COUNT) {
        Serial.printf("[MODE] Unknown mode: %s\n", modeStr);
        network_broadcast("{\"event\":\"MODE_REJECTED\"}");
        return;
    }

    // Safety gate: AUTONOMOUS and VOICE cannot be entered while the system is halted.
    if (requested != MODE_MANUAL && requested != MODE_SAFE) {
        if (state_is_halted()) {
            Serial.printf("[MODE] Rejected %s — system is halted\n", modeStr);
            network_broadcast("{\"event\":\"MODE_REJECTED\"}");
            return;
        }
    }

    // No-op if mode is already active
    if (_mode == requested) return;

    Serial.printf("[MODE] %s → %s\n", MODE_NAMES[_mode], MODE_NAMES[requested]);
    _mode = requested;

    // Acknowledge the mode change to the dashboard
    char ack[48];
    snprintf(ack, sizeof(ack), "{\"event\":\"MODE_ACK\",\"mode\":\"%s\"}", MODE_NAMES[_mode]);
    network_broadcast(ack);
}

// ── Accessors ────────────────────────────────────────────────────────────────
RoverMode   mode_get()  { return _mode; }
const char* mode_name() { return MODE_NAMES[_mode]; }
