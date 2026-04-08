/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
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
#include "event_bus.h"
#include "network_manager.h"
#include "state_manager.h"
#include <Arduino.h>
#include <string.h>

#define MODE_VOICE MODE_ERROR
#define mode_init mode_init_legacy
#define mode_update mode_update_legacy
#define mode_receive mode_receive_legacy
#define mode_get mode_get_legacy
#define mode_name mode_name_legacy

// ── Private state ─────────────────────────────────────────────────────────────
static RoverMode _mode = MODE_IDLE;
static char _lastReason[32] = "";

static const char* const MODE_NAMES[MODE_COUNT] = {
    "IDLE", "MANUAL", "AUTONOMOUS", "SAFE", "ERROR"
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

#undef MODE_VOICE
#undef mode_init
#undef mode_update
#undef mode_receive
#undef mode_get
#undef mode_name

static RoverMode s_mode = MODE_IDLE;
static char s_last_reason[32] = "";

static const char* const S_MODE_NAMES[MODE_COUNT] = {
    "IDLE",
    "MANUAL",
    "AUTONOMOUS",
    "SAFE",
    "ERROR"
};

static void mode_store_reason(const char* reason) {
    if (!reason) {
        s_last_reason[0] = '\0';
        return;
    }

    strncpy(s_last_reason, reason, sizeof(s_last_reason) - 1);
    s_last_reason[sizeof(s_last_reason) - 1] = '\0';
}

static void mode_publish(RoverMode mode) {
    EventPayload payload = {};
    payload.channel = EVENT_MODE_CHANGED;
    payload.timestamp_ms = millis();
    payload.data[0] = (uint8_t)mode;
    event_publish(EVENT_MODE_CHANGED, &payload);
}

static bool mode_apply(RoverMode next, const char* reason, bool announce) {
    if (next >= MODE_COUNT) return false;
    if (s_mode == next) return true;

    Serial.printf("[MODE] %s -> %s (%s)\n",
                  S_MODE_NAMES[s_mode],
                  S_MODE_NAMES[next],
                  reason ? reason : "");

    s_mode = next;
    mode_store_reason(reason);
    mode_publish(next);

    if (announce) {
        char ack[64];
        snprintf(ack, sizeof(ack), "{\"event\":\"MODE_ACK\",\"mode\":\"%s\"}", S_MODE_NAMES[s_mode]);
        network_broadcast(ack);
    }

    return true;
}

void mode_init(void) {
    s_mode = MODE_IDLE;
    mode_store_reason("boot");
}

void mode_update(void) {
    if (state_get() == STATE_ERROR && s_mode != MODE_ERROR) {
        mode_apply(MODE_ERROR, "state error", false);
        return;
    }

    if (state_get() == STATE_SAFE && s_mode != MODE_SAFE) {
        mode_apply(MODE_SAFE, "state safe", false);
        return;
    }

    if (state_get() == STATE_READY && (s_mode == MODE_SAFE || s_mode == MODE_ERROR)) {
        mode_apply(MODE_IDLE, "state recovered", true);
    }
}

bool mode_request(RoverMode requested, const char* reason) {
    if (requested >= MODE_COUNT) return false;

    if (requested == MODE_SAFE || requested == MODE_ERROR) {
        return mode_apply(requested, reason, true);
    }

    if (!state_is_online() || state_is_halted()) {
        network_broadcast("{\"event\":\"MODE_REJECTED\"}");
        return false;
    }

    return mode_apply(requested, reason, true);
}

bool mode_request_from_string(const char* mode_str, const char* reason) {
    if (!mode_str) return false;

    RoverMode requested = MODE_COUNT;
    if (strcmp(mode_str, "IDLE") == 0) requested = MODE_IDLE;
    else if (strcmp(mode_str, "MANUAL") == 0) requested = MODE_MANUAL;
    else if (strcmp(mode_str, "AUTONOMOUS") == 0) requested = MODE_AUTONOMOUS;
    else if (strcmp(mode_str, "SAFE") == 0) requested = MODE_SAFE;
    else if (strcmp(mode_str, "ERROR") == 0) requested = MODE_ERROR;

    if (requested == MODE_COUNT) {
        network_broadcast("{\"event\":\"MODE_REJECTED\"}");
        return false;
    }

    return mode_request(requested, reason);
}

void mode_force(RoverMode next, const char* reason) {
    mode_apply(next, reason, true);
}

RoverMode mode_get(void) {
    return s_mode;
}

const char* mode_name(void) {
    return S_MODE_NAMES[s_mode];
}

const char* mode_last_reason(void) {
    return s_last_reason;
}

bool mode_is_motion_permitted(void) {
    return s_mode == MODE_MANUAL || s_mode == MODE_AUTONOMOUS;
}
