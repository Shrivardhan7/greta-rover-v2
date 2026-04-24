/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

// ============================================================================
//  main.cpp — Greta OS Entry Point
//  Robot  : Greta V2
//  Target : ESP32-S3 (Xtensa LX6, dual-core, 240 MHz)
//
//  Architecture:
//    Safety first    — state machine guards all movement
//    Modular         — modules communicate via callbacks, not direct coupling
//    Cooperative     — no RTOS tasks; millis()-based scheduler
//    Readable        — timing budget documented per layer
//
//  Execution timing budget (cooperative, non-preemptive):
//     5 ms  — STATE GATE   : FSM guard check
//    10 ms  — CONTROL      : command watchdogs
//    20 ms  — COMMS        : network + bluetooth ingress
//   500 ms  — TELEMETRY    : outbound dashboard data
//  1000 ms  — HEALTH       : system health scoring
// ============================================================================

// ── Platform ─────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <string.h>

// ── Greta OS Core ─────────────────────────────────────────────────────────────
#include "scheduler.h"        // Cooperative task scheduler
#include "event_bus.h"        // Decoupled inter-module event bus
#include "health_manager.h"   // System health scoring (WiFi RSSI, heap, loop time)

// ── Greta V2 Modules ─────────────────────────────────────────────────────────
#include "config.h"
#include "state_manager.h"    // FSM: BOOT → CONNECTING → READY → MOVING / SAFE / ERROR
#include "network_manager.h"  // WiFi + WebSocket link
#include "command_processor.h"// Command validation, forwarding, ACK handling
#include "bluetooth_bridge.h" // UART bridge to Arduino motor controller
#include "telemetry.h"        // JSON telemetry broadcast to dashboard
#include "mode_manager.h"     // Rover operating mode (MANUAL / AUTONOMOUS / SAFE)
#include "task_manager.h"     // Lightweight task lifecycle registry
#include "behavior_manager.h" // Deterministic arbitration and safety policy
#include "room_identity_manager.h"

// ── Future Module Hooks (not yet implemented) ─────────────────────────────────
// Uncomment each block when the module is ready.
// Keep these gated so the build stays clean with no stub object files.
//
// #include "motion_manager.h"  // Drive / servo / actuator abstraction
// #include "vision_manager.h"  // Camera pipeline, object detection
// #include "ota_manager.h"     // Over-the-air firmware update


// ============================================================================
//  INTERNAL HELPERS
//  All functions below are file-scoped (static). Modules communicate through
//  the registered callbacks — not by calling each other directly.
// ============================================================================

// ── Command ingress callback ──────────────────────────────────────────────────
// network_manager calls this on each validated dashboard frame.
// Keeping this adapter here means network_manager does not need to know
// that command_processor exists.
static void on_dashboard_command(const char* cmd) {
    if (cmd && strncmp(cmd, "MODE ", 5) == 0) {
        behavior_handle_mode_request(cmd + 5);
        return;
    }
    if (cmd && strncmp(cmd, "ROOM ", 5) == 0) {
        room_identity_handle_command(cmd);
        return;
    }
    command_receive(cmd);
}

// ── Bluetooth ACK drain ───────────────────────────────────────────────────────
// Must run after bluetooth_update() in the same loop tick.
// If this were a separate scheduled task, bluetooth_update() might not have run
// yet and we would read stale data. Inline placement here is intentional.
static inline void bluetooth_ack_drain() {
    if (bluetooth_available()) {
        command_receive_ack(bluetooth_read());
    }
}

// ── Link recovery ─────────────────────────────────────────────────────────────
// Evaluates WiFi + BT link health every loop() iteration and drives
// CONNECTING → READY and SAFE → READY transitions.
// Runs ungated (not inside the scheduler) so that link loss is detected
// within one loop iteration, not within one scheduler task interval.
static void link_recovery_update() {
    const RobotState cur  = state_get();
    const bool       wifi = network_wifi_ok();
    const bool       bt   = bluetooth_connected();
    const bool       ok   = wifi && bt;

    // Both links up → leave CONNECTING or recover from SAFE
    if ((cur == STATE_CONNECTING || cur == STATE_SAFE) && ok) {
        Serial.println(F("[MAIN] Links up → READY"));
        state_set(STATE_READY, "links up");
        return;
    }

    // Either link dropped while operational → go to SAFE
    // (bluetooth_bridge and network_manager also call state_set(STATE_SAFE)
    //  on their own timeouts — this is a belt-and-suspenders fallback.)
    if ((cur == STATE_READY || cur == STATE_MOVING) && !ok) {
        Serial.println(F("[MAIN] Link lost → SAFE"));
        state_set(STATE_SAFE, "link lost");
    }
}


// ============================================================================
//  BOOT SEQUENCE
// ============================================================================
void setup() {

    // ── Serial console ────────────────────────────────────────────────────────
    Serial.begin(115200);
    delay(100);   // Wait for USB-CDC to settle — the only blocking call at boot

    Serial.println(F("\n[GRETA] ============================================"));
    Serial.println(F("[GRETA]  Greta Rover OS — Booting"));
    Serial.println(F("[GRETA] ============================================"));
    Serial.printf( "[GRETA]  Free heap : %u bytes\n", ESP.getFreeHeap());
    Serial.printf( "[GRETA]  CPU freq  : %u MHz\n",   ESP.getCpuFreqMHz());
    Serial.printf( "[GRETA]  SDK ver   : %s\n",       ESP.getSdkVersion());
    Serial.println(F("[GRETA] ============================================"));

    // ── Module init — order is load-bearing ──────────────────────────────────
    //
    //   state_init()   — FSM must be valid before any other module initialises,
    //                    because network_init() fires WiFi callbacks immediately.
    //   bluetooth_init — hardware UART; no soft dependencies.
    //   command_init   — depends on state (reads STATE_* for gate checks).
    //   mode_init      — depends on state (rejects mode changes when halted).
    //   telemetry_init — depends on network being pre-configured (not started).
    //   network_init   — starts WiFi last; command callback must be set first.
    //   event_bus_init — no dependencies; must be ready before scheduler.
    //   scheduler_init — times all subsequent tasks.
    //   health_manager — runs on top of scheduler timing.

    event_bus_init();

    state_init();
    state_set(STATE_CONNECTING, "boot");

    bluetooth_init();
    command_init();
    mode_init();
    task_manager_init();
    behavior_manager_init();
    room_identity_manager_init();
    telemetry_init();

    network_set_command_callback(on_dashboard_command);
    network_init();

    scheduler_init();
    health_manager_init();

    Serial.println(F("[MAIN] All modules initialised — entering runtime loop"));
}


// ============================================================================
//  RUNTIME LOOP — cooperative scheduler active
//
//  Each loop() iteration:
//    1. SCHEDULER      — run any tasks whose interval has elapsed
//    2. BT ACK DRAIN   — inline, must follow bluetooth_update()
//    3. LINK RECOVERY  — FSM link transitions, runs every tick
// ============================================================================
void loop() {

    // 1. Scheduler — runs registered tasks at their configured intervals.
    //    Tasks that are not due this tick return immediately via scheduler_due().
    if (scheduler_due(TASK_HEALTH))     health_manager_update();
    if (scheduler_due(TASK_STATE_GATE)) state_update();
    if (scheduler_due(TASK_COMMAND)) {
        mode_update();
        behavior_manager_update();
        command_update();
        task_manager_update();
        room_identity_manager_update();
    }
    if (scheduler_due(TASK_NETWORK))    network_update();

    // bluetooth_update must run before bluetooth_ack_drain in the same tick.
    if (scheduler_due(TASK_BLUETOOTH))  bluetooth_update();

    if (scheduler_due(TASK_TELEMETRY))  telemetry_update();

    // 2. BT ACK drain — inline after bluetooth_update() for this tick.
    bluetooth_ack_drain();

    // 3. Link recovery — runs every loop() iteration, not scheduler-gated.

    // Scheduler timing measurement — call at the end of each loop iteration.
    scheduler_tick();
}
