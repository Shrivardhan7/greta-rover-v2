// ═══════════════════════════════════════════════════════════════════════════════
//  GRETA OS — KERNEL ENTRY POINT
//  File    : main.cpp
//  Robot   : Greta V2
//  Target  : ESP32 (Xtensa LX6, dual-core, 240 MHz)
//  Kernel  : Greta OS v2.0 — Cooperative Real-Time Kernel
// ───────────────────────────────────────────────────────────────────────────────
//  Architecture philosophy:
//    Safety first     — panic before undefined behaviour
//    Modular brain    — zero inter-module coupling through this file
//    Hardware abstrac — platform details hidden behind HAL headers
//    AI ready         — task slots pre-allocated for perception / planning
//    Industrial grade — every timing budget documented and enforced
// ───────────────────────────────────────────────────────────────────────────────
//  Execution timing budget (cooperative, non-preemptive):
//    10 ms  — CRITICAL  : network ingress, bluetooth ACK, link watchdogs
//    20 ms  — CONTROL   : command watchdogs, motor safety
//    50 ms  — STATE     : FSM transitions, link recovery
//   100 ms  — TELEMETRY : outbound data, system monitor, health report
//   500 ms  — SLOW      : OTA check, vision heartbeat, brain tick (future)
// ═══════════════════════════════════════════════════════════════════════════════

// ── LAYER 0 : Platform ────────────────────────────────────────────────────────
#include <Arduino.h>

// ── LAYER 1 : Greta OS Core ───────────────────────────────────────────────────
#include "greta_core.h"       // System identity, boot ID, uptime, capability flags
#include "scheduler.h"        // Cooperative task scheduler (MAX_TASKS enforced)
#include "event_bus.h"        // Decoupled inter-module event bus
#include "health_manager.h"   // Heartbeat registry, stall detection, fault reporting
#include "system_monitor.h"   // Heap / stack / WiFi RSSI / CPU load observer

// ── LAYER 2 : Greta V2 Module HAL ─────────────────────────────────────────────
#include "config.h"
#include "state_manager.h"    // FSM: CONNECTING → READY → SAFE → PANIC
#include "network_manager.h"
#include "command_processor.h"
#include "bluetooth_bridge.h"
#include "telemetry.h"

// ── LAYER 3 : Future Module Hooks (not yet implemented) ───────────────────────
// Headers included only when the corresponding capability flag is set in
// greta_core.h so that the build remains clean with no stub object files.
#if GRETA_CAP_MOTION
#  include "motion_manager.h"  // Drive / servo / actuator abstraction
#endif
#if GRETA_CAP_VISION
#  include "vision_manager.h"  // Camera pipeline, object detection feed
#endif
#if GRETA_CAP_BRAIN
#  include "brain_manager.h"   // Autonomy / planning / ML inference
#endif
#if GRETA_CAP_SAFETY
#  include "safety_manager.h"  // Hardware e-stop, current sensing, tilt
#endif
#if GRETA_CAP_OTA
#  include "ota_manager.h"     // Over-the-air firmware update service
#endif


// ═══════════════════════════════════════════════════════════════════════════════
//  INTERNAL LINKAGE — callbacks and recovery helpers
//  All functions below are file-scoped (static). Nothing outside main.cpp
//  may call them directly; modules communicate through the event bus only.
// ═══════════════════════════════════════════════════════════════════════════════

// ── Command ingress callback ──────────────────────────────────────────────────
// network_manager raises this on each validated dashboard frame.
// Kept here (not inside network_manager) so network_manager remains ignorant
// of command_processor — the only coupling point is this thin adapter.
static void on_dashboard_command(const char* cmd) {
    command_receive(cmd);
}

// ── Bluetooth ACK drain ───────────────────────────────────────────────────────
// Must execute inline in loop() after bluetooth_update() has already run for
// this tick. Scheduling it as a separate task would create a read-before-update
// race. This is the one permissible inline call that bypasses the scheduler.
static inline void bluetooth_ack_drain() {
    if (bluetooth_available()) {
        command_receive_ack(bluetooth_read());
    }
}

// ── Link recovery FSM ─────────────────────────────────────────────────────────
// Evaluates WiFi + BT link health and drives CONNECTING → READY / SAFE → READY
// transitions. Runs every loop() iteration (not scheduler-gated) because link
// loss must be detected within one scheduler quantum, not one task interval.
static void link_recovery_update() {
    const greta_state_t cur  = state_get();
    const bool          wifi = network_wifi_ok();
    const bool          bt   = bluetooth_connected();
    const bool          ok   = wifi && bt;

    if (cur == STATE_CONNECTING && ok) {
        state_set(STATE_READY, "links up");
        health_kick("network");
        health_kick("bluetooth");
        return;
    }

    if (cur == STATE_SAFE && ok) {
        Serial.println(F("[MAIN] Links restored → READY"));
        state_set(STATE_READY, "links restored");
        return;
    }

    // ── Panic escalation ─────────────────────────────────────────────────────
    // health_manager_fault() returns true when any registered module has missed
    // its heartbeat beyond the configured timeout. A single missed beat is not
    // sufficient — health_manager applies its own hysteresis window.
    if (cur != STATE_PANIC) {
        if (health_manager_fault()) {
            Serial.println(F("[MAIN] PANIC — health fault detected"));
            state_set(STATE_PANIC, "health fault");
            // Downstream safety response is handled by state_manager and any
            // registered STATE_PANIC listeners on the event bus. This file
            // does not directly command motors or actuators.
            return;
        }
        if (scheduler_fault()) {
            Serial.println(F("[MAIN] PANIC — scheduler fault"));
            state_set(STATE_PANIC, "scheduler fault");
            return;
        }
    }
}

// ── Panic safe-loop ───────────────────────────────────────────────────────────
// Entered when STATE_PANIC is active. Transmits a minimal telemetry alert at a
// reduced cadence and does nothing else. Movement is inhibited by state_manager.
// This function never returns; the watchdog will reset the MCU if recovery
// is desired (not implemented here — recovery policy belongs to state_manager).
static void panic_safe_loop() {
    static uint32_t lastAlert = 0;
    const  uint32_t now       = millis();

    if (now - lastAlert >= 2000UL) {
        lastAlert = now;
        telemetry_send_alert("PANIC: system fault — movement disabled");
        Serial.println(F("[MAIN] !! PANIC LOOP — awaiting watchdog reset !!"));
    }
    // Yield to RTOS idle task; do not spin-lock the core.
    yield();
}


// ═══════════════════════════════════════════════════════════════════════════════
//  BOOT SEQUENCE
// ═══════════════════════════════════════════════════════════════════════════════
void setup() {

    // ── BOOT LAYER ────────────────────────────────────────────────────────────
    Serial.begin(115200);
    delay(100);   // USB CDC settle — the only permissible blocking call at boot

    // Greta core must be first: it stamps the boot ID and boot timestamp used
    // by every subsequent log line and by the health manager's uptime tracker.
    greta_core_init();

    Serial.printf("\n");
    Serial.println(F("╔══════════════════════════════════════════════╗"));
    Serial.println(F("║         GRETA OS — KERNEL STARTING           ║"));
    Serial.println(F("╚══════════════════════════════════════════════╝"));
    Serial.printf("[GRETA] Robot     : %s\n",          greta_robot_name());
    Serial.printf("[GRETA] Firmware  : %s  (%s)\n",    GRETA_VERSION, GRETA_BUILD_DATE);
    Serial.printf("[GRETA] Boot ID   : %s\n",          greta_boot_id());
    Serial.printf("[GRETA] HW profile: %s\n",          greta_hw_profile());

    // ── Boot diagnostics ──────────────────────────────────────────────────────
    Serial.println(F("[GRETA] ── Boot diagnostics ──────────────────────"));
    Serial.printf("[GRETA] Free heap : %u bytes\n",    ESP.getFreeHeap());
    Serial.printf("[GRETA] Flash size: %u bytes\n",    ESP.getFlashChipSize());
    Serial.printf("[GRETA] CPU freq  : %u MHz\n",      ESP.getCpuFreqMHz());
    Serial.printf("[GRETA] SDK ver   : %s\n",          ESP.getSdkVersion());
    Serial.println(F("[GRETA] Boot diagnostics ready"));
    Serial.println(F("[GRETA] ──────────────────────────────────────────"));

    // ── MODULE INIT LAYER ─────────────────────────────────────────────────────
    // Order is load-bearing. Do not reorder without updating the comment block
    // at the top of this file and the scheduler task registration below.
    //
    //   state_init()   — must precede everything; FSM must be valid before any
    //                    callback could fire (including network_init's WiFi).
    //   bluetooth_init — hardware peripheral; no soft dependencies.
    //   command_init   — depends on state (reads STATE_* for gate checks).
    //   telemetry_init — depends on network being pre-configured (not started).
    //   network_init   — fires WiFi.begin() last; callback registered first.

    state_init();
    state_set(STATE_CONNECTING, "boot");

    bluetooth_init();
    command_init();
    telemetry_init();

    network_set_command_callback(on_dashboard_command);
    network_init();

    // ── OS INIT LAYER ─────────────────────────────────────────────────────────
    // Greta OS services initialised after all hardware modules are ready.
    // event_bus has no dependencies → first. scheduler needs event_bus for fault
    // events → second. health_manager needs scheduler task count → third.
    // system_monitor needs health_manager and greta_core uptime → last.

    event_bus_init();
    scheduler_init();
    health_manager_init();
    system_monitor_init();

    // ── Health heartbeat registration ─────────────────────────────────────────
    // Every schedulable module registers here. health_manager will raise a fault
    // if health_kick("<name>") is not called within the module's timeout window.
    health_register("network");
    health_register("bluetooth");
    health_register("command");
    health_register("telemetry");
    health_register("system_monitor");

    // ── TASK REGISTRATION LAYER ───────────────────────────────────────────────
    // Registration order defines execution priority within a single scheduler
    // tick when multiple tasks fall due simultaneously.
    //
    // CRITICAL (10 ms) — ingress path; must run before any watchdog fires.
    scheduler_add_task(network_update,         10);
    scheduler_add_task(bluetooth_update,       10);

    // CONTROL (20 ms) — command watchdogs; ingress must have been processed.
    scheduler_add_task(command_update,         20);

    // STATE (50 ms) — FSM bookkeeping; depends on command pipeline being fresh.
    scheduler_add_task(state_update,           50);

    // TELEMETRY / MONITOR (100 ms) — outbound only; lowest priority.
    scheduler_add_task(telemetry_update,      100);
    scheduler_add_task(system_monitor_update, 100);

    // ── Future module task slots (capability-gated, not yet implemented) ──────
    // These slots are pre-allocated in the scheduler's MAX_TASKS budget.
    // Uncomment each line when the corresponding module is implemented and
    // its capability flag is enabled in greta_core.h.
    //
    // CONTROL layer (20 ms):
    //   scheduler_add_task(safety_update,  20);   // GRETA_CAP_SAFETY
    //
    // STATE layer (50 ms):
    //   scheduler_add_task(motion_update,  50);   // GRETA_CAP_MOTION
    //
    // SLOW layer (500 ms):
    //   scheduler_add_task(vision_update, 500);   // GRETA_CAP_VISION
    //   scheduler_add_task(brain_update,  500);   // GRETA_CAP_BRAIN
    //   scheduler_add_task(ota_update,    500);   // GRETA_CAP_OTA

    Serial.printf("[MAIN] Scheduler tasks registered: %d\n", scheduler_task_count());
    Serial.println(F("[MAIN] Greta OS kernel boot complete — entering runtime loop"));
}


// ═══════════════════════════════════════════════════════════════════════════════
//  RUNTIME LOOP — GRETA OS PHASE-1 SCHEDULER ACTIVE
// ═══════════════════════════════════════════════════════════════════════════════
//  Structure of each loop() iteration:
//
//   1. PANIC CHECK    — abort to safe-loop immediately if system is faulted.
//   2. SCHEDULER      — run all tasks whose interval has elapsed (in reg. order).
//   3. BT ACK DRAIN   — inline, post-bluetooth_update; not schedulable safely.
//   4. LINK RECOVERY  — FSM transitions + panic escalation; runs every tick.
//
//  Nothing else belongs here. Cross-cutting concerns go into their own modules.
// ═══════════════════════════════════════════════════════════════════════════════
void loop() {

    // ── RUNTIME LOOP LAYER ────────────────────────────────────────────────────

    // 1. Panic guard — if the system is in STATE_PANIC, nothing else runs.
    //    panic_safe_loop() yields internally; it does not spin-lock the core.
    if (state_get() == STATE_PANIC) {
        panic_safe_loop();
        return;
    }

    // 2. Scheduler tick — executes all tasks registered above whose interval
    //    has elapsed since their last execution. Order within a tick is the
    //    registration order, preserving the original execution priority.
    scheduler_run_due_tasks();

    // 3. Bluetooth ACK drain — must follow bluetooth_update() in the same tick.
    //    Placed here rather than as a scheduler task to eliminate the
    //    read-before-update race (see bluetooth_ack_drain() comment above).
    bluetooth_ack_drain();

    // 4. Link recovery + panic escalation — runs every loop() iteration so that
    //    link loss is detected within one scheduler quantum regardless of which
    //    tasks were due this tick.
    link_recovery_update();
}
