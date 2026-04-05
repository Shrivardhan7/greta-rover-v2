# Greta OS — Phase 1 Engineering Reference
**Scope:** event_bus · scheduler · health_manager integration guide
**Applies to:** Greta V2 → Greta OS Phase 1
**Do not begin Phase 2 until Phase 1 validation is complete.**

---

## 1. File Structure

```
greta-os/
└── firmware/
    ├── main.cpp                         ← EXISTS. Minimal additions only.
    │
    ├── system/                          ← NEW directory for Phase-1 modules
    │   ├── event_bus.h
    │   ├── event_bus.cpp
    │   ├── scheduler.h
    │   ├── scheduler.cpp
    │   ├── health_manager.h
    │   └── health_manager.cpp
    │
    ├── main_integration_patch.cpp       ← Integration reference (not compiled)
    └── telemetry_integration_patch.cpp  ← Integration reference (not compiled)
```

The `system/` directory is new. All three Phase-1 modules live here. This keeps System Core infrastructure separate from the existing transport, safety, and behavior modules. Future System Core modules (`fault_manager` Phase 2, `datacore` Phase 3) will also live here.

The two patch files are engineering references — they are not compiled. Apply their diffs to the existing `main.cpp` and `telemetry.cpp` by hand.

---

## 2. Event Bus — Design Decisions

**Why synchronous dispatch?**
Asynchronous queues require dynamic allocation or fixed ring buffers plus a drain mechanism. On a cooperative single-threaded embedded system, synchronous dispatch is simpler, fully deterministic, and eliminates deferred-execution bugs. Every published event is fully handled before `event_publish()` returns.

**Why static subscription table?**
No heap allocation. No pointer lifetime management. Subscription count is a compile-time constant — if it overflows, it fails visibly at init time (return value check), not silently at runtime.

**Why no FSM transitions through event_bus?**
`state_manager` is the sole FSM authority. If events could trigger state transitions, any subscriber could indirectly modify FSM state by publishing the right event. The prohibition is architectural, not performance-based.

**Handler contract:**
Every event handler must be short (microseconds, not milliseconds), non-blocking, and must not call `event_publish()` itself. Re-entrant dispatch into the same handler table during a dispatch is not supported and must be avoided.

---

## 3. Scheduler — Design Decisions

**Why not a real scheduler or RTOS?**
FreeRTOS task switching on ESP32 introduces stack isolation, priority inversion risk, and synchronisation overhead. For a cooperative embedded system with well-bounded module update functions, cooperative scheduling in a single `loop()` is safer, more auditable, and requires no synchronisation primitives.

**Why centralise timing?**
Ad-hoc `if (millis() - last_X > INTERVAL_X)` repeated across `loop()` scales poorly. Intervals drift when copy-paste errors occur. The scheduler makes all timing visible in one place (`s_task_intervals[]` in `scheduler.cpp`) and ensures consistent rollover handling everywhere.

**Rollover safety:**
`uint32_t elapsed = now - s_last_run_ms[task]` is correct even when `millis()` overflows at 2^32 ms (~49 days). Unsigned subtraction wraps correctly for this use case.

**Loop timing measurement:**
`scheduler_tick()` records the loop start timestamp. `scheduler_get_loop_time_ms()` returns the elapsed time since the previous tick. `health_manager` consumes this to detect loop budget overrun before it becomes a safety issue.

---

## 4. Health Manager — Design Decisions

**Why integer-only scoring?**
ESP32 has hardware floating point, but floating point in embedded firmware introduces subtle precision and ordering issues that are hard to audit. Integer arithmetic with scaled values is explicit, predictable, and easy to trace in a debugger.

**Why edge-trigger for EVENT_HEALTH_WARNING?**
Publishing the event repeatedly while in warning state would flood event handlers every second (the health update interval). Edge triggering — publish once on crossing into warning, clear flag on recovery — produces one event per condition, which is the semantically correct signal.

**Why does health_manager not call state_manager_request()?**
Health manager is System Core infrastructure. Halt decisions belong to `fault_manager` (Phase 2) which evaluates fault severity against policy. Phase 1 health_manager surfaces warnings only. When `fault_manager` is added in Phase 2, it will subscribe to `EVENT_HEALTH_WARNING` and escalate to a halt request if warranted. The chain is: health_manager → event_bus → fault_manager → state_manager. Not health_manager → state_manager directly.

**Why return HealthReport by value from health_get_report()?**
Returning a copy (not a pointer) prevents callers from holding a stale pointer into internal state. On ESP32, a 20-byte struct copy is negligible. Telemetry calls this once per 500ms — not a hot path.

---

## 5. Modules That Must NOT Be Modified

These Greta V2 modules are untouched during Phase 1. No file edits, no interface changes, no logic modifications:

| Module | Reason |
|--------|--------|
| `state_manager` | FSM authority. Any change requires safety review. Phase 1 does not touch the FSM. |
| `command_processor` | Behavior layer. No changes needed. Existing command flow is preserved. |
| `bluetooth_bridge` | Transport layer. No changes needed. |
| `mode_manager` | Behavior layer. No changes needed. |

**Allowed modifications (additive only):**

| Module | Allowed Change |
|--------|---------------|
| `main.cpp` | Add 3 includes. Add 3 init calls. Wrap existing update calls in `scheduler_due()`. |
| `telemetry.h` | Append 7 new fields to `TelemetrySnapshot` struct. |
| `telemetry.cpp` | Add 8 lines inside existing `telemetry_update()` to populate health fields. |
| `network_manager` | Add 1 line: `health_manager_record_rssi(WiFi.RSSI())`. Or add it in telemetry instead. |

All other files: read-only for Phase 1.

---

## 6. Safe Integration Order

Follow this order exactly. Do not skip steps. Do not combine steps.

### Step 1 — Create event_bus (no integration yet)

Create `system/event_bus.h` and `system/event_bus.cpp`. Add `#include "system/event_bus.h"` to `main.cpp` and call `event_bus_init()` in `setup()`. Nothing subscribes yet. Compile. Flash. Verify system boots normally. Verify no regression in WiFi, Bluetooth, command handling.

**Validation gate:** All existing Greta V2 behaviour unchanged. event_bus_init() runs without error.

---

### Step 2 — Integrate scheduler

Create `system/scheduler.h` and `system/scheduler.cpp`. Add `#include "system/scheduler.h"` to `main.cpp`. Call `scheduler_init()` in `setup()`. Replace the `loop()` body with the scheduler-driven pattern from `main_integration_patch.cpp`. Compile. Flash. Measure loop timing. Verify all existing module update intervals are preserved within tolerance.

**Validation gate:** All existing Greta V2 behaviour unchanged. Telemetry arrives at the same cadence. Commands are processed at the same latency. No loop timing regression.

---

### Step 3 — Add health_manager (metrics only, no event publishing)

Create `system/health_manager.h` and `system/health_manager.cpp`. Temporarily comment out the `event_publish()` call in `health_manager_update()` (add a `#define HEALTH_EVENTS_ENABLED 0` guard). Add `#include "system/health_manager.h"` to `main.cpp`. Call `health_manager_init()` in `setup()`. Add `health_manager_update()` to the `TASK_HEALTH` scheduler slot. Add `health_manager_record_rssi()` call in network_manager or telemetry. Compile. Flash. Verify health score is computed (add a `Serial.println(health_get_score())` temporarily).

**Validation gate:** Health score computes correctly (100 under normal conditions, drops under simulated heap or RSSI pressure). No impact on existing module behaviour. No event publishing yet.

---

### Step 4 — Telemetry health integration

Apply `telemetry_integration_patch.cpp` additions to `telemetry.h` and `telemetry.cpp`. Apply the JSON serializer additions. Compile. Flash. Open dashboard. Verify health fields appear in the telemetry payload. Verify existing telemetry fields are unchanged.

**Validation gate:** Dashboard receives and displays health_score, health_status, heap_free, uptime_s correctly. Existing dashboard panels unaffected.

---

### Step 5 — Enable event_bus publishing in health_manager

Remove the `HEALTH_EVENTS_ENABLED` guard. Enable `event_publish(EVENT_HEALTH_WARNING, &evt)` in `health_manager_update()`. Add a test subscriber (Serial.println) to verify events dispatch. Simulate a health warning condition (force low RSSI, or reduce `HEAP_NOMINAL` threshold temporarily). Verify event fires once on threshold crossing, not repeatedly. Verify recovery clears the flag.

**Validation gate:** EVENT_HEALTH_WARNING dispatches exactly once on threshold crossing. Clears on recovery. No timing regression. No spurious events during normal operation.

---

### Step 6 — Phase 1 system validation

Run the full system for an extended session. Check: loop timing under load, health score stability, telemetry cadence, command latency, event dispatch on link loss/restore (Phase 2 wires link events; validate the channel exists and dispatches correctly when tested manually). Document results. Only after successful validation does Phase 2 begin.

---

## 7. Implementation Order Rationale

**event_bus first** because `health_manager_init()` subscribes during its init. `event_bus_init()` must have run before any subscriber registration. If this order is reversed, the subscription table is corrupt from the first call.

**scheduler second** because it wraps existing module calls with no behavioural change. If the scheduler step introduces a regression, the delta is minimal: only the timing wrapper changed. This makes regression root cause obvious.

**health_manager metrics only third** because separating metric collection (harmless) from event publishing (new behaviour) gives an isolated validation step. If the score is wrong, it is a scoring bug. It cannot be confused with an event dispatch bug because publishing is disabled.

**telemetry integration fourth** because it is the first externally observable integration milestone. Dashboard health fields are visible confirmation that the full chain — sampling → scoring → snapshot → serialization → dashboard — is working end to end.

**event publishing last** because it is the only step that adds new dynamic behaviour to the system. Enabling it last means any regression at this step is unambiguously caused by event dispatch.

This order means that at every step, a regression has exactly one possible cause: the module added in that step.

---

## 8. Minimal Code Philosophy

All Phase-1 code is written to these constraints:

- **No templates.** No `std::function`, no `std::vector`, no STL containers.
- **No dynamic allocation.** No `malloc`, no `new`, no `delete`. All state is static.
- **No blocking calls.** No `delay()`, no `while (!done)` in any update function.
- **No float arithmetic** in scoring paths. Integer arithmetic only.
- **Fixed-size arrays** for all collections. Overflow returns a failure code, not undefined behaviour.
- **No abstraction layers** beyond what is needed. One header per module. One implementation per module. No factory functions, no virtual dispatch.
- **C-compatible headers.** The `.h` files use `typedef struct`, `typedef enum`, and function declarations compatible with both C and C++ translation units. This preserves flexibility for future Arduino-side reuse.

These constraints exist because embedded firmware reliability is about predictability, not elegance. Code that does exactly what it appears to do, with no hidden allocation and no dynamic dispatch, is code that behaves the same way on the thousandth boot as on the first.

---

## 9. Phase 2 Preparation Notes

Phase 1 creates the infrastructure that Phase 2 (`fault_manager`, `risk_manager`) will build on. No Phase 2 code is written now — but the Phase 1 design deliberately leaves the following hooks ready:

- `EVENT_HEALTH_WARNING` is already defined and published. `fault_manager` (Phase 2) subscribes to it at init time.
- `health_get_score()` API is already defined. `fault_manager` polls it.
- `TASK_COUNT` in scheduler.h has room for new task slots without changing existing IDs.
- `EVENT_CHANNEL_COUNT` in event_bus.h grows by adding new enum values before the sentinel.
- `TelemetrySnapshot` struct can be extended with fault fields using the same additive pattern as the health fields.

Phase 2 integration follows the same pattern as Phase 1: create new modules, add init calls, add scheduler slots, extend telemetry — no existing logic is modified.

---

*Greta OS Phase 1 Engineering Reference — Rev 1.0*
*All changes governed by system design rules in GRETA_OS_CONTEXT.md Rev 1.2*
