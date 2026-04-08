/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

// ============================================================================
//  health_manager.cpp — System Health Scoring
//
//  What it does:
//    Computes a composite health score (0–100) from three sub-scores:
//      - WiFi signal strength (RSSI in dBm)
//      - Free heap memory (bytes)
//      - Main loop execution time (ms)
//
//    If the composite score drops below HEALTH_THRESHOLD_WARNING, a
//    EVENT_HEALTH_WARNING event is published on the event bus. The event
//    fires only once per degradation episode (edge-trigger, not level-trigger)
//    and re-arms when health recovers to STABLE.
//
//  Scoring:
//    Each sub-score uses piecewise linear mapping: 0 at the "bad" threshold,
//    100 at the "good" threshold, linear in between. Integer arithmetic only.
//
//  Composite:
//    Weighted average: heap and loop time carry more weight than RSSI because
//    they are better indicators of firmware stress.
//
//  External updates:
//    RSSI is sampled externally by network_manager via health_manager_record_rssi().
//    Heap and loop time are sampled inside health_manager_update().
// ============================================================================

#include "health_manager.h"
#include "event_bus.h"
#include "scheduler.h"
#include <Arduino.h>
#include <esp_system.h>    // esp_get_free_heap_size()

// ── Scoring Thresholds ────────────────────────────────────────────────────────
// For metrics where higher is better (RSSI, heap): val_min=bad, val_max=good.
// For loop time (lower is better), see score_loop_time() below.

#define RSSI_NOMINAL    -60     // 100 points (strong signal)
#define RSSI_WARNING    -75     // ~50 points
#define RSSI_MIN        -90     // 0 points   (very weak signal)

#define HEAP_NOMINAL    80000   // 100 points (~80 KB free — healthy)
#define HEAP_WARNING    50000   // ~50 points
#define HEAP_MIN        30000   // 0 points   (dangerously low)

#define LOOP_NOMINAL        2   // 100 points (2 ms — fast loop)
#define LOOP_WARNING        8   // ~50 points
#define LOOP_MAX           20   // 0 points   (20 ms — loop is overloaded)

// ── Composite Score Weights ───────────────────────────────────────────────────
// Heap and loop time weighted higher — they signal firmware stress directly.
#define WEIGHT_WIFI     2
#define WEIGHT_HEAP     4
#define WEIGHT_LOOP     4
#define WEIGHT_TOTAL    (WEIGHT_WIFI + WEIGHT_HEAP + WEIGHT_LOOP)

// ── Internal State ────────────────────────────────────────────────────────────
static HealthReport s_report;
static bool         s_warning_published = false;   // Edge-trigger guard

// ── Scoring Helpers ───────────────────────────────────────────────────────────

// Maps value in [val_min, val_max] → score in [0, 100].
// Returns 0 if value <= val_min, 100 if value >= val_max.
// Uses integer arithmetic only (no floats).
static uint8_t score_range(int32_t value, int32_t val_min, int32_t val_max) {
    if (value <= val_min) return 0;
    if (value >= val_max) return 100;

    int32_t range = val_max - val_min;
    int32_t delta = value  - val_min;

    return (uint8_t)((delta * 100) / range);
}

// Loop time: higher is worse, so we invert the direction compared to score_range().
static uint8_t score_loop_time(uint32_t loop_ms) {
    if (loop_ms >= LOOP_MAX)     return 0;
    if (loop_ms <= LOOP_NOMINAL) return 100;

    uint32_t range = LOOP_MAX - LOOP_NOMINAL;
    uint32_t delta = LOOP_MAX - loop_ms;

    return (uint8_t)((delta * 100) / range);
}

// Converts a composite score to a HealthStatus label.
static HealthStatus derive_status(uint8_t score) {
    if (score >= HEALTH_THRESHOLD_WARNING)  return HEALTH_STABLE;
    if (score >= HEALTH_THRESHOLD_CRITICAL) return HEALTH_WARNING;
    return HEALTH_CRITICAL;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void health_manager_init(void) {
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

void health_manager_update(void) {

    // ── Sample raw metrics ────────────────────────────────────────────────────
    s_report.uptime_s        = millis() / 1000;
    s_report.heap_free_bytes = esp_get_free_heap_size();
    s_report.loop_time_ms    = scheduler_get_loop_time_ms();
    // RSSI is updated externally via health_manager_record_rssi()

    // ── Compute sub-scores ────────────────────────────────────────────────────
    s_report.score_wifi = score_range(
        s_report.last_rssi_dbm, RSSI_MIN, RSSI_NOMINAL
    );
    s_report.score_heap = score_range(
        (int32_t)s_report.heap_free_bytes, HEAP_MIN, HEAP_NOMINAL
    );
    s_report.score_loop = score_loop_time(s_report.loop_time_ms);

    // ── Compute weighted composite ────────────────────────────────────────────
    uint32_t weighted_sum =
        (uint32_t)s_report.score_wifi * WEIGHT_WIFI +
        (uint32_t)s_report.score_heap * WEIGHT_HEAP +
        (uint32_t)s_report.score_loop * WEIGHT_LOOP;

    s_report.score_composite = (uint8_t)(weighted_sum / WEIGHT_TOTAL);

    // ── Derive status ─────────────────────────────────────────────────────────
    s_report.status = derive_status(s_report.score_composite);

    // ── Publish event on first crossing into warning (edge trigger) ───────────
    // We only publish once per degradation episode, not on every update tick.
    if (s_report.status != HEALTH_STABLE && !s_warning_published) {
        EventPayload evt;
        evt.channel      = EVENT_HEALTH_WARNING;
        evt.timestamp_ms = millis();
        evt.data[0]      = s_report.score_composite;
        evt.data[1]      = (uint8_t)s_report.status;

        event_publish(EVENT_HEALTH_WARNING, &evt);
        s_warning_published = true;
    }

    // Re-arm once health recovers
    if (s_report.status == HEALTH_STABLE) {
        s_warning_published = false;
    }
}

// Called by network_manager each time a WiFi RSSI reading is available.
void health_manager_record_rssi(int32_t rssi_dbm) {
    s_report.last_rssi_dbm = rssi_dbm;
}

// ── Accessors ────────────────────────────────────────────────────────────────
uint8_t health_get_score(void) {
    return s_report.score_composite;
}

HealthStatus health_get_status(void) {
    return s_report.status;
}

// Returns a value copy of the report — caller cannot mutate internal state.
HealthReport health_get_report(void) {
    return s_report;
}
