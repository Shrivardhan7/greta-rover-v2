/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  health_manager.h  —  Greta OS
//
//  Aggregates per-subsystem health indicators into a composite score (0–100).
//  Monitors WiFi RSSI, free heap, and main loop execution time.
//  Publishes EVENT_HEALTH_WARNING on the event bus when the score drops
//  below HEALTH_THRESHOLD_WARNING (edge-trigger — fires once per episode).
//
//  Consumed by:
//    - telemetry    → health_get_score(), health_get_report() (read-only)
//
//  Rules:
//    - health_manager does NOT call state_set(). Ever.
//    - health_manager does NOT modify any module state.
//    - health_manager does NOT take corrective action autonomously.
//    - All scoring uses integer arithmetic only. No floating point.
// ============================================================================

#include <stdint.h>

// ── Health Status ─────────────────────────────────────────────────────────────
typedef enum {
    HEALTH_STABLE   = 0,   // Score >= HEALTH_THRESHOLD_WARNING
    HEALTH_WARNING  = 1,   // Score >= HEALTH_THRESHOLD_CRITICAL, < WARNING
    HEALTH_CRITICAL = 2,   // Score <  HEALTH_THRESHOLD_CRITICAL
} HealthStatus;

// ── Score Thresholds ──────────────────────────────────────────────────────────
#define HEALTH_THRESHOLD_WARNING   70   // Below this → HEALTH_WARNING, event published
#define HEALTH_THRESHOLD_CRITICAL  40   // Below this → HEALTH_CRITICAL

// ── Health Report ─────────────────────────────────────────────────────────────
// Snapshot of all health metrics. Returned by value from health_get_report().
// Caller must treat the returned struct as read-only.
typedef struct {
    uint8_t      score_composite;   // Weighted composite score, 0–100
    uint8_t      score_wifi;        // WiFi RSSI sub-score,  0–100
    uint8_t      score_heap;        // Free heap sub-score,  0–100
    uint8_t      score_loop;        // Loop time sub-score,  0–100
    HealthStatus status;            // Derived from composite score
    uint32_t     uptime_s;          // System uptime in seconds
    int32_t      last_rssi_dbm;     // Last recorded WiFi RSSI (dBm, negative)
    uint32_t     heap_free_bytes;   // Last recorded free heap
    uint32_t     loop_time_ms;      // Last recorded main loop execution time
} HealthReport;

// ── Public API ────────────────────────────────────────────────────────────────

// Initialise internal state. Call from main.cpp after event_bus_init().
void health_manager_init(void);

// Sample all health metrics and recompute composite score.
// Call via scheduler at TASK_HEALTH interval (1000 ms).
// Non-blocking. Publishes EVENT_HEALTH_WARNING if threshold is crossed.
void health_manager_update(void);

// Feed the latest WiFi RSSI from network_manager.
// Call from network_manager_update() or from the telemetry integration point.
// Non-blocking — updates internal RSSI sample only.
void health_manager_record_rssi(int32_t rssi_dbm);

// Returns the last computed composite health score (0–100).
uint8_t health_get_score(void);

// Returns HEALTH_STABLE, HEALTH_WARNING, or HEALTH_CRITICAL.
HealthStatus health_get_status(void);

// Returns a copy of the full HealthReport struct.
// Used by telemetry to populate the dashboard payload.
HealthReport health_get_report(void);
