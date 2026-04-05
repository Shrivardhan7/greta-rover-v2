// ════════════════════════════════════════════════════════════════════════════
//  GRETA V2 — ESP32 Firmware additions
//  File: include/mode_manager.h + src/mode_manager.cpp
//
//  INTEGRATION — what to add to existing files:
//
//  1. In main.cpp setup():
//       mode_init();          // after command_init()
//
//  2. In main.cpp loop():
//       mode_update();        // after state_update()
//
//  3. In command_processor.cpp, command_receive():
//       Add before the state gate check:
//         if (mode_get() == MODE_SAFE && !_is_stop_cmd(cmd)) return;
//         if (mode_get() == MODE_AUTONOMOUS && _is_move_cmd(cmd)) return;
//
//  4. In network_manager.cpp _ws_event(), WStype_TEXT case:
//       Add handling for "MODE <n>" frames before calling _cmdCb:
//         if (strncmp(p, "MODE ", 5) == 0) {
//             mode_receive(p + 5);
//             return;
//         }
//
//  5. In telemetry.cpp telemetry_build():
//       Add: doc["mode"] = mode_name();
//
//  6. In platformio.ini lib_deps — no new libraries required.
// ════════════════════════════════════════════════════════════════════════════


// ════════════════════════════════════════════════════════════════════════════
//  include/mode_manager.h
// ════════════════════════════════════════════════════════════════════════════
/*
#pragma once
#include <Arduino.h>

enum RoverMode : uint8_t {
    MODE_MANUAL     = 0,
    MODE_AUTONOMOUS = 1,
    MODE_VOICE      = 2,
    MODE_SAFE       = 3,
    MODE_COUNT      = 4
};

void        mode_init();
void        mode_update();          // call every loop() — reserved for future timed transitions

void        mode_receive(const char* modeStr);  // called by network_manager on "MODE <n>" frame
RoverMode   mode_get();
const char* mode_name();
*/


// ════════════════════════════════════════════════════════════════════════════
//  src/mode_manager.cpp
// ════════════════════════════════════════════════════════════════════════════

#include "mode_manager.h"
#include "network_manager.h"
#include "state_manager.h"
#include "command_processor.h"
#include <Arduino.h>

static RoverMode _mode = MODE_MANUAL;

static const char* const MODE_NAMES[MODE_COUNT] = {
    "MANUAL", "AUTONOMOUS", "VOICE", "SAFE"
};

void mode_init() {
    _mode = MODE_MANUAL;
    Serial.println(F("[MODE] init → MANUAL"));
}

void mode_update() {
    // Reserved — e.g. timeout back to MANUAL if autonomous stalls
}

// Called by network_manager when dashboard sends "MODE MANUAL" etc.
void mode_receive(const char* modeStr) {
    if (!modeStr) return;

    RoverMode requested = MODE_COUNT;    // Sentinel

    if      (strcmp(modeStr, "MANUAL")     == 0) requested = MODE_MANUAL;
    else if (strcmp(modeStr, "AUTONOMOUS") == 0) requested = MODE_AUTONOMOUS;
    else if (strcmp(modeStr, "VOICE")      == 0) requested = MODE_VOICE;
    else if (strcmp(modeStr, "SAFE")       == 0) requested = MODE_SAFE;

    if (requested == MODE_COUNT) {
        Serial.printf("[MODE] Unknown mode requested: %s\n", modeStr);
        network_broadcast("{\"event\":\"MODE_REJECTED\"}");
        return;
    }

    // Safety gate: cannot enter AUTONOMOUS or VOICE from STATE_SAFE or STATE_ERROR
    if (requested != MODE_MANUAL && requested != MODE_SAFE) {
        if (state_is_halted()) {
            Serial.printf("[MODE] Mode change to %s rejected — system halted\n", modeStr);
            network_broadcast("{\"event\":\"MODE_REJECTED\"}");
            return;
        }
    }

    if (_mode == requested) return;

    Serial.printf("[MODE] %s → %s\n", MODE_NAMES[_mode], MODE_NAMES[requested]);
    _mode = requested;

    // ACK the mode change so dashboard confirms
    char ack[48];
    snprintf(ack, sizeof(ack), "{\"event\":\"MODE_ACK\",\"mode\":\"%s\"}", MODE_NAMES[_mode]);
    network_broadcast(ack);
}

RoverMode   mode_get()  { return _mode; }
const char* mode_name() { return MODE_NAMES[_mode]; }
