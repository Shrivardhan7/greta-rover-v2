/*
 * health_manager.h
 * Greta OS — Phase 1 System Backbone
 *
 * Aggregates per-subsystem health indicators into a composite health score.
 * Monitors WiFi RSSI, Bluetooth latency, heap usage, loop timing, and uptime.
 * Publishes EVENT_HEALTH_WARNING on event_bus when score drops below threshold.
 *
 * Consumed by:
 *   - telemetry    → health_get_score(), health_get_report() (read-only)
 *   - fault_manager → health_get_score() (Phase 2 — stub prepared here)
 *
 * RULES:
 *   - health_manager does NOT call state_manager_request(). Ever.
 *   - health_manager does NOT modify any module state.
 *   - health_manager does NOT take corrective action autonomously.
 *   - All scoring uses integer arithmetic only. No floating point.
 */

#ifndef HEALTH_MANAGER_H
#define HEALTH_MANAGER_H

#include <stdint.h>

/* ── Health States ──────────────────────────────────────────────────────── */

typedef enum {
    HEALTH_STABLE   = 0,   /* Score >= HEALTH_THRESHOLD_WARNING  */
    HEALTH_WARNING  = 1,   /* Score >= HEALTH_THRESHOLD_CRITICAL, < WARNING */
    HEALTH_CRITICAL = 2,   /* Score <  HEALTH_THRESHOLD_CRITICAL  */
} HealthStatus;

/* ── Score Thresholds ───────────────────────────────────────────────────── */

#define HEALTH_THRESHOLD_WARNING   70   /* Below this → HEALTH_WARNING, publish event */
#define HEALTH_THRESHOLD_CRITICAL  40   /* Below this → HEALTH_CRITICAL               */

/* ── Health Report ──────────────────────────────────────────────────────── */

/*
 * Per-metric sub-scores (0–100 each) and derived composite.
 * Exposed read-only to telemetry via health_get_report().
 */
typedef struct {
    uint8_t  score_composite;    /* Weighted composite score, 0–100         */
    uint8_t  score_wifi;         /* WiFi RSSI sub-score                     */
    uint8_t  score_heap;         /* Free heap sub-score                     */
    uint8_t  score_loop;         /* Loop timing sub-score                   */
    HealthStatus status;         /* Derived state from composite score      */
    uint32_t uptime_s;           /* System uptime in seconds                */
    int32_t  last_rssi_dbm;      /* Last recorded WiFi RSSI (raw dBm)       */
    uint32_t heap_free_bytes;    /* Last recorded free heap                 */
    uint32_t loop_time_ms;       /* Last recorded main loop execution time  */
} HealthReport;

/* ── Public API ─────────────────────────────────────────────────────────── */

/*
 * health_manager_init()
 * Initialise internal state. Call from main.cpp after event_bus_init().
 */
void health_manager_init(void);

/*
 * health_manager_update()
 * Sample all health metrics and recompute composite score.
 * Call from main loop via scheduler (TASK_HEALTH, 1000 ms interval).
 * Non-blocking. Publishes EVENT_HEALTH_WARNING if threshold crossed.
 */
void health_manager_update(void);

/*
 * health_manager_record_rssi()
 * Feed the latest WiFi RSSI value from network_manager.
 * Call from network_manager_update() — or from telemetry integration point.
 * Non-blocking. Updates internal RSSI sample only.
 */
void health_manager_record_rssi(int32_t rssi_dbm);

/*
 * health_get_score()
 * Returns the last computed composite health score (0–100).
 * Safe to call from any module context.
 */
uint8_t health_get_score(void);

/*
 * health_get_status()
 * Returns HEALTH_STABLE, HEALTH_WARNING, or HEALTH_CRITICAL.
 */
HealthStatus health_get_status(void);

/*
 * health_get_report()
 * Returns a copy of the full HealthReport struct.
 * Telemetry calls this to populate the dashboard payload.
 * Caller must treat the returned struct as read-only snapshot.
 */
HealthReport health_get_report(void);

#endif /* HEALTH_MANAGER_H */
