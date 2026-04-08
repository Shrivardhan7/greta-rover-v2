/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

// ============================================================================
//  scheduler.cpp — Cooperative Millis-Based Task Scheduler
//
//  How it works:
//    Each task is assigned a TaskID (defined in scheduler.h) and a fixed
//    interval in milliseconds (defined in s_task_intervals[] below).
//
//    In loop(), caller checks: if (scheduler_due(TASK_X)) do_task_x();
//
//    scheduler_due() returns true once per interval and resets the timer.
//    It never blocks and never allocates memory.
//
//  Timing accuracy:
//    Based on millis(). Rollover-safe: uint32_t subtraction wraps correctly
//    at ~49 days without any special handling.
//
//  Loop time measurement:
//    Call scheduler_tick() at the end of each loop() iteration.
//    health_manager reads the result via scheduler_get_loop_time_ms() to
//    score loop timing as part of the system health report.
//
//  To retune task intervals:
//    Edit s_task_intervals[] below. Do not scatter timing constants across
//    other modules — all task timing lives here.
// ============================================================================

#include "scheduler.h"
#include <Arduino.h>

// ── Task Interval Table ───────────────────────────────────────────────────────
// Index matches the TaskID enum in scheduler.h.
// All values are in milliseconds.
static const uint32_t s_task_intervals[TASK_COUNT] = {
    [TASK_STATE_GATE]  =    5,   // FSM guard — fast enough to catch link drops
    [TASK_COMMAND]     =   10,   // Command + ACK watchdogs
    [TASK_NETWORK]     =   20,   // WiFi + WebSocket ingress
    [TASK_BLUETOOTH]   =   20,   // UART RX from Arduino + silence watchdog
    [TASK_TELEMETRY]   =  500,   // JSON telemetry broadcast to dashboard
    [TASK_HEALTH]      = 1000,   // Health score update (RSSI, heap, loop time)
};

// ── Internal State ────────────────────────────────────────────────────────────
static uint32_t s_last_run_ms[TASK_COUNT];
static uint32_t s_loop_start_ms = 0;
static uint32_t s_loop_time_ms  = 0;

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void scheduler_init(void) {
    uint32_t now = millis();

    for (uint8_t i = 0; i < TASK_COUNT; i++) {
        s_last_run_ms[i] = now;
    }

    s_loop_start_ms = now;
    s_loop_time_ms  = 0;
}

// ── Tick ──────────────────────────────────────────────────────────────────────
// Call at the end of each loop() iteration.
// Computes elapsed time for the iteration that just completed.
// health_manager reads this via scheduler_get_loop_time_ms().
void scheduler_tick(void) {
    uint32_t now    = millis();
    s_loop_time_ms  = now - s_loop_start_ms;
    s_loop_start_ms = now;
}

// ── Due check ────────────────────────────────────────────────────────────────
// Returns true if the task's interval has elapsed since it last ran.
// Resets the task timer on each true return.
// Usage: if (scheduler_due(TASK_NETWORK)) network_update();
bool scheduler_due(TaskID task) {
    if (task >= TASK_COUNT) return false;

    uint32_t now     = millis();
    uint32_t elapsed = now - s_last_run_ms[task];

    if (elapsed >= s_task_intervals[task]) {
        s_last_run_ms[task] = now;
        return true;
    }

    return false;
}

// ── Loop time accessor ────────────────────────────────────────────────────────
uint32_t scheduler_get_loop_time_ms(void) {
    return s_loop_time_ms;
}
