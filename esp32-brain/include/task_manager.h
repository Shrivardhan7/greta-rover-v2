/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once

#include <stdint.h>

typedef enum {
    GRETA_TASK_IDLE = 0,
    GRETA_TASK_MANUAL_DRIVE,
    GRETA_TASK_AUTONOMY,
    GRETA_TASK_SAFETY_HOLD,
    GRETA_TASK_COUNT
} GretaTaskId;

typedef enum {
    TASK_LIFECYCLE_INACTIVE = 0,
    TASK_LIFECYCLE_ACTIVE,
    TASK_LIFECYCLE_INTERRUPTED
} TaskLifecycle;

typedef struct {
    GretaTaskId   id;
    TaskLifecycle lifecycle;
    uint8_t       priority;
    uint32_t      entered_ms;
    char          note[32];
} TaskStatus;

void        task_manager_init(void);
void        task_manager_update(void);
bool        task_manager_activate(GretaTaskId task, const char* note);
bool        task_manager_interrupt(GretaTaskId task, const char* note);
void        task_manager_clear(GretaTaskId task, const char* note);
void        task_manager_reset_to_idle(const char* note);
GretaTaskId task_manager_active(void);
uint8_t     task_manager_active_priority(void);
const char* task_manager_active_name(void);
TaskStatus  task_manager_status(GretaTaskId task);

// Internal execution entrypoints used by behavior_manager to reach the motion layer.
// These are module contracts, not general command APIs.
bool        task_manager_dispatch_motion(GretaTaskId task, const char* cmd, const char* note);
bool        task_manager_dispatch_stop(const char* note);
