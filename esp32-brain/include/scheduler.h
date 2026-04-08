/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  scheduler.h  —  Greta OS
//
//  Cooperative millis()-based task scheduler.
//  Manages all module update intervals from a single location.
//  Replaces ad-hoc (millis() - lastX > INTERVAL_X) checks scattered in main.
//
//  Design:
//    - Non-blocking. Evaluates intervals; does not call module functions.
//    - main.cpp calls scheduler_due() per task and runs the task if due.
//    - All timing uses uint32_t millis(); rollover at ~49 days is handled
//      correctly by unsigned subtraction — no special casing needed.
//    - No threads. No RTOS dependency. Fully cooperative.
//
//  Task intervals are defined in scheduler.cpp — tune there, not here.
//
//  Rule: the scheduler controls WHEN modules run.
//        Modules control HOW LONG they run.
//        Loop timing drift is detected via scheduler_get_loop_time_ms()
//        and consumed by health_manager.
// ============================================================================

#include <stdint.h>

// ── Task IDs ──────────────────────────────────────────────────────────────────
// Assigned in order of criticality. Intervals are defined in scheduler.cpp.
typedef enum {
    TASK_STATE_GATE  = 0,   // state_manager gate evaluation     —   5 ms
    TASK_COMMAND     = 1,   // command_processor watchdogs       —  10 ms
    TASK_NETWORK     = 2,   // network_manager update            —  20 ms
    TASK_BLUETOOTH   = 3,   // bluetooth_bridge update           —  20 ms
    TASK_TELEMETRY   = 4,   // telemetry broadcast               — 500 ms
    TASK_HEALTH      = 5,   // health_manager update             — 1000 ms

    TASK_COUNT              // Keep last — used for array sizing
} TaskID;

// ── Public API ────────────────────────────────────────────────────────────────

// Initialise all task timers to the current millis() value.
// Call once from main.cpp setup() before the main loop starts.
void scheduler_init(void);

// Record the elapsed time for the loop iteration that just completed.
// Call at the END of every loop() iteration.
// health_manager reads this value via scheduler_get_loop_time_ms().
void scheduler_tick(void);

// Returns true if the task's interval has elapsed since it last ran.
// On true: resets the task timer. Caller must then run the task.
// On false: interval has not elapsed — caller skips the task.
// Usage: if (scheduler_due(TASK_NETWORK)) network_update();
bool scheduler_due(TaskID task);

// Returns the elapsed time in ms for the most recently completed loop iteration.
// Used by health_manager to detect loop timing drift.
uint32_t scheduler_get_loop_time_ms(void);
