#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  telemetry.h  —  Greta V2
//
//  Design rationale:
//    Telemetry is a read-only snapshot of system state for the dashboard.
//    It deliberately aggregates from multiple modules rather than having
//    modules push into a central struct — this keeps modules independent
//    and the telemetry frame always consistent.
//
//    telemetry_build() writes into a caller-provided buffer to avoid
//    returning heap-allocated Strings across module boundaries.
// ══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>

void telemetry_init();
void telemetry_update();    // Rate-limited broadcast — call every loop()

// Write current telemetry JSON into buf (max bufLen bytes, null-terminated).
// Safe to call at any time; does not broadcast.
void telemetry_build(char* buf, size_t bufLen);
