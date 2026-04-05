/*
 * scheduler.h
 * Greta OS — Phase 1 System Backbone
 *
 * Cooperative millis()-based task scheduler.
 * Manages module update intervals from a single location.
 * Replaces ad-hoc (millis() - last_X > INTERVAL_X) checks in main loop.
 *
 * Design:
 *   - Non-blocking. Evaluates intervals; does not call module functions.
 *   - main.cpp calls scheduler_due() per task and decides whether to run it.
 *   - All timing is uint32_t millis()-based; handles rollover correctly.
 *   - No threads. No RTOS dependency. Fully cooperative.
 *
 * RULE: Scheduler controls WHEN modules run. Modules control HOW LONG they run.
 * If a module's update function runs long, the scheduler detects the drift via
 * scheduler_get_loop_time_ms(). health_manager consumes this value.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

/* ── Task IDs ───────────────────────────────────────────────────────────── */

/*
 * Assigned in order of criticality.
 * Intervals defined in scheduler.cpp — tune there, not here.
 */
typedef enum {
    TASK_STATE_GATE   = 0,   /* state_manager gate evaluation     — 5 ms  */
    TASK_COMMAND      = 1,   /* command_processor poll            — 10 ms */
    TASK_NETWORK      = 2,   /* network_manager update            — 20 ms */
    TASK_BLUETOOTH    = 3,   /* bluetooth_bridge update           — 20 ms */
    TASK_TELEMETRY    = 4,   /* telemetry update                  — 500 ms*/
    TASK_HEALTH       = 5,   /* health_manager update             — 1000 ms*/

    TASK_COUNT               /* Keep last */
} TaskID;

/* ── Public API ─────────────────────────────────────────────────────────── */

/*
 * scheduler_init()
 * Initialise all task timers to current millis().
 * Call once from main.cpp before the main loop starts.
 */
void scheduler_init(void);

/*
 * scheduler_tick()
 * Record the start of the current loop iteration.
 * Must be called FIRST in every loop() iteration.
 */
void scheduler_tick(void);

/*
 * scheduler_due()
 * Returns true if the task's interval has elapsed since its last run.
 * On return true, resets the task timer. Caller must then run the task.
 * Returns false if interval has not elapsed — caller skips the task.
 */
bool scheduler_due(TaskID task);

/*
 * scheduler_get_loop_time_ms()
 * Returns the elapsed time in ms for the most recently completed loop.
 * Used by health_manager to detect loop timing drift.
 */
uint32_t scheduler_get_loop_time_ms(void);

#endif /* SCHEDULER_H */
