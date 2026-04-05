/*
 * health_manager.cpp
 * Greta OS — Phase 1 System Backbone
 *
 * Implementation notes:
 *   - All scoring: integer arithmetic only. No floats.
 *   - Sub-scores computed with piecewise linear map: clamp(value, min, max)
 *     scaled to 0–100 using integer multiply + divide.
 *   - Composite score = weighted average of sub-scores.
 *   - EVENT_HEALTH_WARNING published only on threshold crossing (edge trigger),
 *     not repeatedly while in warning state.
 *   - heap_free_bytes sampled via ESP.getFreeHeap() (ESP32 Arduino API).
 *   - Loop timing sampled from scheduler_get_loop_time_ms().
 */

#include "health_manager.h"
#include "event_bus.h"
#include "scheduler.h"
#include <Arduino.h>   /* millis() */
#include <esp_system.h>  /* esp_get_free_heap_size() */

/* ── Scoring Thresholds ─────────────────────────────────────────────────── */

/* WiFi RSSI thresholds (dBm). Scores 0 at or below RSSI_MIN. */
#define RSSI_NOMINAL    -60     /* 100 points   */
#define RSSI_WARNING    -75     /* ~50 points   */
#define RSSI_MIN        -90     /* 0 points     */

/* Heap free thresholds (bytes). Scores 0 at or below HEAP_MIN. */
#define HEAP_NOMINAL    80000   /* 100 points   */
#define HEAP_WARNING    50000   /* ~50 points   */
#define HEAP_MIN        30000   /* 0 points     */

/* Loop time thresholds (ms). Scores 0 at or above LOOP_MAX. */
#define LOOP_NOMINAL        2   /* 100 points   */
#define LOOP_WARNING        8   /* ~50 points   */
#define LOOP_MAX           20   /* 0 points     */

/* ── Composite Score Weights ────────────────────────────────────────────── */

/*
 * Integer weights. Final score = sum(weight_i * score_i) / sum(weights).
 * Heap and loop timing weighted highest as they signal firmware stress.
 */
#define WEIGHT_WIFI     2
#define WEIGHT_HEAP     4
#define WEIGHT_LOOP     4
#define WEIGHT_TOTAL    (WEIGHT_WIFI + WEIGHT_HEAP + WEIGHT_LOOP)

/* ── Internal State ─────────────────────────────────────────────────────── */

static HealthReport s_report;
static bool         s_warning_published = false;  /* edge-trigger guard */

/* ── Internal Helpers ───────────────────────────────────────────────────── */

/*
 * score_range()
 * Maps a value in [val_min, val_max] to a score in [0, 100].
 * Returns 0 if value <= val_min, 100 if value >= val_max.
 * Uses integer arithmetic only.
 *
 * For metrics where lower is worse (RSSI, heap): val_min=bad, val_max=good.
 * For metrics where higher is worse (loop time): pass negated values or
 * use score_range_inverse() below.
 */
static uint8_t score_range(int32_t value, int32_t val_min, int32_t val_max)
{
    if (value <= val_min) return 0;
    if (value >= val_max) return 100;

    /* Scale to 0–100 integer */
    int32_t range = val_max - val_min;
    int32_t delta = value  - val_min;

    return (uint8_t)((delta * 100) / range);
}

/* Loop time: higher value is worse → invert by scoring against LOOP_MAX */
static uint8_t score_loop_time(uint32_t loop_ms)
{
    if (loop_ms >= LOOP_MAX)     return 0;
    if (loop_ms <= LOOP_NOMINAL) return 100;

    uint32_t range = LOOP_MAX - LOOP_NOMINAL;
    uint32_t delta = LOOP_MAX - loop_ms;

    return (uint8_t)((delta * 100) / range);
}

static HealthStatus derive_status(uint8_t score)
{
    if (score >= HEALTH_THRESHOLD_WARNING)  return HEALTH_STABLE;
    if (score >= HEALTH_THRESHOLD_CRITICAL) return HEALTH_WARNING;
    return HEALTH_CRITICAL;
}

/* ── Public Functions ───────────────────────────────────────────────────── */

void health_manager_init(void)
{
    s_report.score_composite = 100;
    s_report.score_wifi      = 100;
    s_report.score_heap      = 100;
    s_report.score_loop      = 100;
    s_report.status          = HEALTH_STABLE;
    s_report.uptime_s        = 0;
    s_report.last_rssi_dbm   = RSSI_NOMINAL;
    s_report.heap_free_bytes = HEAP_NOMINAL;
    s_report.loop_time_ms    = 0;

    s_warning_published = false;
}

void health_manager_update(void)
{
    /* ── Sample raw metrics ─────────────────────────────────────────────── */

    s_report.uptime_s        = millis() / 1000;
    s_report.heap_free_bytes = esp_get_free_heap_size();
    s_report.loop_time_ms    = scheduler_get_loop_time_ms();

    /* s_report.last_rssi_dbm is updated externally via health_manager_record_rssi() */

    /* ── Compute sub-scores ─────────────────────────────────────────────── */

    s_report.score_wifi = score_range(
        s_report.last_rssi_dbm, RSSI_MIN, RSSI_NOMINAL
    );

    s_report.score_heap = score_range(
        (int32_t)s_report.heap_free_bytes, HEAP_MIN, HEAP_NOMINAL
    );

    s_report.score_loop = score_loop_time(s_report.loop_time_ms);

    /* ── Compute weighted composite ─────────────────────────────────────── */

    uint32_t weighted_sum =
        (uint32_t)s_report.score_wifi * WEIGHT_WIFI +
        (uint32_t)s_report.score_heap * WEIGHT_HEAP +
        (uint32_t)s_report.score_loop * WEIGHT_LOOP;

    s_report.score_composite = (uint8_t)(weighted_sum / WEIGHT_TOTAL);

    /* ── Derive status ──────────────────────────────────────────────────── */

    s_report.status = derive_status(s_report.score_composite);

    /* ── Publish event on crossing into warning (edge trigger) ──────────── */

    if (s_report.status != HEALTH_STABLE && !s_warning_published) {
        EventPayload evt;
        evt.channel      = EVENT_HEALTH_WARNING;
        evt.timestamp_ms = millis();
        evt.data[0]      = s_report.score_composite;
        evt.data[1]      = (uint8_t)s_report.status;

        event_publish(EVENT_HEALTH_WARNING, &evt);
        s_warning_published = true;
    }

    /* Clear flag when health recovers to STABLE */
    if (s_report.status == HEALTH_STABLE) {
        s_warning_published = false;
    }
}

void health_manager_record_rssi(int32_t rssi_dbm)
{
    s_report.last_rssi_dbm = rssi_dbm;
}

uint8_t health_get_score(void)
{
    return s_report.score_composite;
}

HealthStatus health_get_status(void)
{
    return s_report.status;
}

HealthReport health_get_report(void)
{
    return s_report;  /* Returns a value copy — caller cannot mutate internal state */
}
