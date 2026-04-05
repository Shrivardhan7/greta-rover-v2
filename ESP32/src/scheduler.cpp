/*
 * scheduler.cpp
 * Greta OS — Phase 1 System Backbone
 *
 * Cooperative millis()-based task scheduler.
 *
 * Implementation notes:
 *   - s_task_intervals[] maps TaskID to interval in ms. Edit here to retune.
 *   - Rollover-safe: subtraction of uint32_t values handles millis() wrap at
 *     ~49 days correctly without special casing.
 *   - s_loop_start_ms tracks the beginning of each loop iteration so
 *     health_manager can measure actual loop execution time.
 */

#include "scheduler.h"
#include <Arduino.h>   /* millis() */

/* ── Task Interval Table ────────────────────────────────────────────────── */

/*
 * Index matches TaskID enum. All values in milliseconds.
 * Edit intervals here — do not scatter timing constants across modules.
 */
static const uint32_t s_task_intervals[TASK_COUNT] = {
    [TASK_STATE_GATE]  =    5,
    [TASK_COMMAND]     =   10,
    [TASK_NETWORK]     =   20,
    [TASK_BLUETOOTH]   =   20,
    [TASK_TELEMETRY]   =  500,
    [TASK_HEALTH]      = 1000,
};

/* ── Internal State ─────────────────────────────────────────────────────── */

static uint32_t s_last_run_ms[TASK_COUNT];
static uint32_t s_loop_start_ms  = 0;
static uint32_t s_loop_time_ms   = 0;

/* ── Public Functions ───────────────────────────────────────────────────── */

void scheduler_init(void)
{
    uint32_t now = millis();

    for (uint8_t i = 0; i < TASK_COUNT; i++) {
        s_last_run_ms[i] = now;
    }

    s_loop_start_ms = now;
    s_loop_time_ms  = 0;
}

void scheduler_tick(void)
{
    uint32_t now = millis();

    /* Compute elapsed time for the loop iteration that just completed */
    s_loop_time_ms  = now - s_loop_start_ms;
    s_loop_start_ms = now;
}

bool scheduler_due(TaskID task)
{
    if (task >= TASK_COUNT) return false;

    uint32_t now     = millis();
    uint32_t elapsed = now - s_last_run_ms[task];

    if (elapsed >= s_task_intervals[task]) {
        s_last_run_ms[task] = now;
        return true;
    }

    return false;
}

uint32_t scheduler_get_loop_time_ms(void)
{
    return s_loop_time_ms;
}
