/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#include "behavior_manager.h"
#include "config.h"
#include "event_bus.h"
#include "health_manager.h"
#include "mode_manager.h"
#include "network_manager.h"
#include "state_manager.h"
#include "task_manager.h"
#include <Arduino.h>
#include <string.h>

static bool s_safety_latched = false;
static bool s_health_warning_latched = false;
static char s_last_safety_reason[48] = "";

static bool is_move_cmd(const char* cmd) {
    if (!cmd) return false;
    return strcmp(cmd, CMD_FORWARD) == 0 ||
           strcmp(cmd, CMD_BACKWARD) == 0 ||
           strcmp(cmd, CMD_LEFT) == 0 ||
           strcmp(cmd, CMD_RIGHT) == 0;
}

static bool is_stop_cmd(const char* cmd) {
    if (!cmd) return false;
    return strcmp(cmd, CMD_STOP) == 0 || strcmp(cmd, CMD_ESTOP) == 0;
}

static void record_safety_reason(const char* reason) {
    if (!reason) {
        s_last_safety_reason[0] = '\0';
        return;
    }

    strncpy(s_last_safety_reason, reason, sizeof(s_last_safety_reason) - 1);
    s_last_safety_reason[sizeof(s_last_safety_reason) - 1] = '\0';
}

static void on_health_warning(const EventPayload* payload) {
    (void)payload;
    s_health_warning_latched = true;
}

void behavior_manager_init(void) {
    s_safety_latched = false;
    s_health_warning_latched = false;
    record_safety_reason("");
    event_subscribe(EVENT_HEALTH_WARNING, on_health_warning);
}

void behavior_manager_update(void) {
    const bool links_ok = network_wifi_ok() && bluetooth_connected();
    const RobotState state = state_get();

    if ((state == STATE_READY || state == STATE_MOVING) && !links_ok) {
        behavior_force_safe("link lost");
    }

    if ((state == STATE_CONNECTING || state == STATE_SAFE) &&
        links_ok &&
        health_get_status() != HEALTH_CRITICAL) {
        mode_force(MODE_IDLE, "links restored");
        state_set(STATE_READY, "links restored");
        task_manager_reset_to_idle("links restored");
        s_safety_latched = false;
    }

    if (s_health_warning_latched && health_get_status() == HEALTH_CRITICAL) {
        behavior_force_safe("health critical");
        s_health_warning_latched = false;
    }

    if (state == STATE_ERROR) {
        s_safety_latched = true;
        mode_force(MODE_ERROR, "state error");
        task_manager_activate(GRETA_TASK_SAFETY_HOLD, "state error");
    }
}

bool behavior_handle_mode_request(const char* mode_str) {
    return mode_request_from_string(mode_str, "dashboard");
}

BehaviorCommandDecision behavior_evaluate_command(const char* cmd, CommandSource source) {
    BehaviorCommandDecision decision = { false, false, false, "rejected" };

    if (!cmd || cmd[0] == '\0') {
        decision.reason = "empty command";
        return decision;
    }

    if (is_stop_cmd(cmd)) {
        decision.accepted = true;
        decision.is_stop = true;
        decision.reason = "stop override";
        return decision;
    }

    decision.is_motion = is_move_cmd(cmd);

    if (!decision.is_motion) {
        decision.accepted = true;
        decision.reason = "non-motion command";
        return decision;
    }

    if (state_is_halted()) {
        decision.reason = "state halted";
        return decision;
    }

    if (!mode_is_motion_permitted()) {
        decision.reason = "mode blocks motion";
        return decision;
    }

    if (health_get_status() != HEALTH_STABLE) {
        decision.reason = "health not stable";
        return decision;
    }

    if (source == COMMAND_SOURCE_DASHBOARD && mode_get() == MODE_AUTONOMOUS) {
        decision.reason = "autonomous mode owns motion";
        return decision;
    }

    if (source == COMMAND_SOURCE_AUTONOMY && mode_get() != MODE_AUTONOMOUS) {
        decision.reason = "autonomy not active";
        return decision;
    }

    decision.accepted = state_can_move();
    decision.reason = decision.accepted ? "accepted" : "state blocks motion";
    return decision;
}

bool behavior_dispatch_command(const char* cmd, CommandSource source, const char** reason_out) {
    const BehaviorCommandDecision decision = behavior_evaluate_command(cmd, source);
    GretaTaskId owner = (source == COMMAND_SOURCE_AUTONOMY)
        ? GRETA_TASK_AUTONOMY
        : GRETA_TASK_MANUAL_DRIVE;

    if (!decision.accepted) {
        if (reason_out) *reason_out = decision.reason;
        behavior_note_command_rejected(cmd, decision.reason);
        return false;
    }

    if (decision.is_stop) {
        task_manager_dispatch_stop("stop command");
        behavior_note_stop_command("stop command");
        if (reason_out) *reason_out = decision.reason;
        return true;
    }

    if (decision.is_motion) {
        if (!task_manager_dispatch_motion(owner, cmd, "motion")) {
            if (reason_out) *reason_out = "task ownership rejected";
            return false;
        }

        behavior_note_motion_command(source, cmd);
        if (reason_out) *reason_out = decision.reason;
        return true;
    }

    if (reason_out) *reason_out = decision.reason;
    return true;
}

void behavior_note_motion_command(CommandSource source, const char* cmd) {
    (void)cmd;
    if (source == COMMAND_SOURCE_AUTONOMY) {
        task_manager_activate(GRETA_TASK_AUTONOMY, "motion");
    } else {
        task_manager_activate(GRETA_TASK_MANUAL_DRIVE, "motion");
        if (mode_get() == MODE_IDLE) {
            mode_request(MODE_MANUAL, "motion");
        }
    }
}

void behavior_note_stop_command(const char* reason) {
    if (mode_get() == MODE_SAFE && state_get() == STATE_READY) {
        mode_force(MODE_IDLE, reason);
    }
}

void behavior_note_command_rejected(const char* cmd, const char* reason) {
    Serial.printf("[BEHAVIOR] rejected '%s' (%s)\n", cmd ? cmd : "", reason ? reason : "");
}

void behavior_force_safe(const char* reason) {
    record_safety_reason(reason);
    s_safety_latched = true;
    task_manager_dispatch_stop(reason ? reason : "safe");
    mode_force((state_get() == STATE_ERROR) ? MODE_ERROR : MODE_SAFE, reason);
    state_set((state_get() == STATE_ERROR) ? STATE_ERROR : STATE_SAFE, reason);
}

bool behavior_is_safety_latched(void) {
    return s_safety_latched;
}

const char* behavior_last_safety_reason(void) {
    return s_last_safety_reason;
}
