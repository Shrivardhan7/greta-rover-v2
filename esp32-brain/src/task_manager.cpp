/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#include "task_manager.h"
#include "bluetooth_bridge.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>

typedef struct {
    const char*    name;
    uint8_t        priority;
    TaskLifecycle  lifecycle;
    uint32_t       entered_ms;
    char           note[32];
} TaskSlot;

static TaskSlot s_tasks[GRETA_TASK_COUNT];

static const char* const TASK_NAMES[GRETA_TASK_COUNT] = {
    "IDLE",
    "MANUAL_DRIVE",
    "AUTONOMY",
    "SAFETY_HOLD"
};

static const uint8_t TASK_PRIORITIES[GRETA_TASK_COUNT] = {
    1,
    40,
    60,
    255
};

static void task_write_note(TaskSlot* slot, const char* note) {
    if (!slot) return;
    if (!note) {
        slot->note[0] = '\0';
        return;
    }

    strncpy(slot->note, note, sizeof(slot->note) - 1);
    slot->note[sizeof(slot->note) - 1] = '\0';
}

static void task_reset_slot(GretaTaskId task) {
    s_tasks[task].name      = TASK_NAMES[task];
    s_tasks[task].priority  = TASK_PRIORITIES[task];
    s_tasks[task].lifecycle = (task == GRETA_TASK_IDLE)
        ? TASK_LIFECYCLE_ACTIVE
        : TASK_LIFECYCLE_INACTIVE;
    s_tasks[task].entered_ms = millis();
    s_tasks[task].note[0] = '\0';
}

void task_manager_init(void) {
    for (uint8_t i = 0; i < GRETA_TASK_COUNT; ++i) {
        task_reset_slot((GretaTaskId)i);
    }
    task_write_note(&s_tasks[GRETA_TASK_IDLE], "boot");
}

void task_manager_update(void) {
    if (task_manager_active() == GRETA_TASK_SAFETY_HOLD) {
        s_tasks[GRETA_TASK_IDLE].lifecycle = TASK_LIFECYCLE_INTERRUPTED;
    }
}

bool task_manager_activate(GretaTaskId task, const char* note) {
    if (task >= GRETA_TASK_COUNT) return false;

    GretaTaskId active = task_manager_active();
    if (active < GRETA_TASK_COUNT &&
        active != task &&
        TASK_PRIORITIES[task] < TASK_PRIORITIES[active]) {
        return false;
    }

    if (active < GRETA_TASK_COUNT && active != task) {
        s_tasks[active].lifecycle = TASK_LIFECYCLE_INTERRUPTED;
        task_write_note(&s_tasks[active], note);
    }

    s_tasks[task].lifecycle = TASK_LIFECYCLE_ACTIVE;
    s_tasks[task].entered_ms = millis();
    task_write_note(&s_tasks[task], note);
    return true;
}

bool task_manager_interrupt(GretaTaskId task, const char* note) {
    if (task >= GRETA_TASK_COUNT) return false;
    if (s_tasks[task].lifecycle != TASK_LIFECYCLE_ACTIVE) return false;

    s_tasks[task].lifecycle = TASK_LIFECYCLE_INTERRUPTED;
    task_write_note(&s_tasks[task], note);

    if (task_manager_active() == GRETA_TASK_COUNT) {
        task_manager_reset_to_idle(note);
    }

    return true;
}

void task_manager_clear(GretaTaskId task, const char* note) {
    if (task >= GRETA_TASK_COUNT) return;

    s_tasks[task].lifecycle = TASK_LIFECYCLE_INACTIVE;
    s_tasks[task].entered_ms = millis();
    task_write_note(&s_tasks[task], note);

    if (task != GRETA_TASK_IDLE && task_manager_active() == GRETA_TASK_COUNT) {
        task_manager_reset_to_idle(note);
    }
}

void task_manager_reset_to_idle(const char* note) {
    for (uint8_t i = 1; i < GRETA_TASK_COUNT; ++i) {
        if (s_tasks[i].lifecycle == TASK_LIFECYCLE_ACTIVE) {
            s_tasks[i].lifecycle = TASK_LIFECYCLE_INTERRUPTED;
        }
    }

    s_tasks[GRETA_TASK_IDLE].lifecycle = TASK_LIFECYCLE_ACTIVE;
    s_tasks[GRETA_TASK_IDLE].entered_ms = millis();
    task_write_note(&s_tasks[GRETA_TASK_IDLE], note);
}

GretaTaskId task_manager_active(void) {
    GretaTaskId active = GRETA_TASK_COUNT;
    uint8_t active_priority = 0;

    for (uint8_t i = 0; i < GRETA_TASK_COUNT; ++i) {
        if (s_tasks[i].lifecycle != TASK_LIFECYCLE_ACTIVE) continue;
        if (active == GRETA_TASK_COUNT || s_tasks[i].priority >= active_priority) {
            active = (GretaTaskId)i;
            active_priority = s_tasks[i].priority;
        }
    }

    return active;
}

uint8_t task_manager_active_priority(void) {
    GretaTaskId active = task_manager_active();
    return (active < GRETA_TASK_COUNT) ? s_tasks[active].priority : 0;
}

const char* task_manager_active_name(void) {
    GretaTaskId active = task_manager_active();
    return (active < GRETA_TASK_COUNT) ? s_tasks[active].name : "NONE";
}

TaskStatus task_manager_status(GretaTaskId task) {
    TaskStatus status = { GRETA_TASK_IDLE, TASK_LIFECYCLE_INACTIVE, 0, 0, "" };

    if (task >= GRETA_TASK_COUNT) return status;

    status.id = task;
    status.lifecycle = s_tasks[task].lifecycle;
    status.priority = s_tasks[task].priority;
    status.entered_ms = s_tasks[task].entered_ms;
    strncpy(status.note, s_tasks[task].note, sizeof(status.note) - 1);
    status.note[sizeof(status.note) - 1] = '\0';
    return status;
}

bool task_manager_dispatch_motion(GretaTaskId task, const char* cmd, const char* note) {
    if (!cmd || cmd[0] == '\0') return false;
    if (task >= GRETA_TASK_COUNT || task == GRETA_TASK_IDLE || task == GRETA_TASK_SAFETY_HOLD) {
        return false;
    }

    if (!task_manager_activate(task, note)) {
        return false;
    }

    bluetooth_send(cmd);
    return true;
}

bool task_manager_dispatch_stop(const char* note) {
    bluetooth_send(CMD_STOP);
    task_manager_activate(GRETA_TASK_SAFETY_HOLD, note);
    return true;
}
