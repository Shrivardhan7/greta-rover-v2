# GRETA OS — Design Rules

**Status:** Mandatory. These rules apply to all code in the Greta V2 repository without exception. A pull request that violates any rule is rejected without review until the violation is corrected.

---

## Rule Index

| ID | Rule | Category |
|---|---|---|
| DR-01 | No `delay()` after boot settle | Timing |
| DR-02 | No direct hardware access outside HAL | Abstraction |
| DR-03 | No module coupling across layers | Architecture |
| DR-04 | All schedulable modules register a health heartbeat | Health |
| DR-05 | All failure conditions transition state | Safety |
| DR-06 | Kernel does not control actuators | Safety |
| DR-07 | No dynamic memory allocation after boot | Memory |
| DR-08 | All `_update()` functions must return within budget | Timing |
| DR-09 | No module reads state and acts without `state_get()` | Safety |
| DR-10 | No global mutable state shared between modules | Architecture |
| DR-11 | All new tasks registered in priority group order | Scheduler |
| DR-12 | All modules expose a consistent init/update API | API |
| DR-13 | No `Serial.print` in `_update()` on the hot path | Performance |
| DR-14 | No magic numbers — all constants in `config.h` | Maintainability |
| DR-15 | Compiler warnings treated as errors | Quality |

---

## DR-01 — No `delay()` after boot settle

**Rule:** The only permitted `delay()` call in the entire firmware is the 100 ms USB CDC settle in `setup()` before `greta_core_init()`. All other waiting must be non-blocking, implemented with `millis()` comparisons.

**Rationale:** `delay()` blocks the cooperative scheduler. A 10 ms `delay()` inside `network_update()` causes every other task to miss its deadline for that tick.

**Detection:** Grep the codebase for `delay(` during CI. Any match outside the single permitted location fails the build.

**Correct pattern:**
```c
// Wrong
delay(500);

// Correct
static uint32_t lastRun = 0;
if (millis() - lastRun >= 500) {
    lastRun = millis();
    // do work
}
```

---

## DR-02 — No direct hardware access outside HAL

**Rule:** Modules at Layer 3 and above must not call `digitalWrite()`, `analogWrite()`, `analogRead()`, `Wire.begin()`, `SPI.begin()`, or any equivalent Arduino or ESP-IDF peripheral function. These calls are made only inside HAL modules (Layer 1).

**Rationale:** Direct hardware access creates a tight coupling between module logic and the hardware platform. Changing the MCU or pin assignment requires modifying every module that contains the call.

**Permitted exceptions:** Layer 0 platform setup code and HAL modules only.

---

## DR-03 — No module coupling across layers

**Rule:** A module must not `#include` the header of another module at the same layer or a higher layer. Cross-module communication at the same layer is done exclusively through the event bus.

**Rationale:** Direct includes create compilation dependencies that cause cascading rebuild failures and obscure the actual data flow.

**Permitted cross-layer dependencies:** As defined in `GRETA_MODULE_MAP.md`. All other includes are forbidden.

**Permitted same-layer communication:** `event_publish()` / `event_subscribe()` only.

---

## DR-04 — All schedulable modules register a health heartbeat

**Rule:** Every module with a scheduler-registered `_update()` function must:
1. Call `health_register("module_name")` in its `_init()` function.
2. Call `health_kick("module_name")` at the beginning of every `_update()` call.

**Rationale:** An `_update()` function that stops running (due to a fault, hang, or scheduler overrun) must be detectable. Without health kicks, a silent module failure cannot be distinguished from normal operation.

---

## DR-05 — All failure conditions transition state

**Rule:** When a module detects a fault condition that affects robot safety or operability, it must express that condition through the FSM by calling `state_set()` or by publishing an event that results in a state transition. A module must not attempt to self-recover silently.

**Rationale:** The FSM is the single observable record of robot health. A module that silently retries a failure produces telemetry that does not match the robot's actual condition.

**Permitted actions on fault:**
- Call `state_set(STATE_SAFE, "reason")` for recoverable faults.
- Publish a fault event to the event bus.
- Log to Serial.

**Not permitted:**
- Blocking retry loops.
- Clearing error flags without state transition.

---

## DR-06 — Kernel does not control actuators

**Rule:** `state_manager`, `scheduler`, `health_manager`, `event_bus`, `greta_core`, and `system_monitor` must not call any function that results in motor, servo, or actuator output. Actuator control is owned exclusively by `motion_manager`.

**Rationale:** Separating state management from actuator control ensures that a kernel bug cannot cause unintended motion. The kernel communicates intent through state and events; motion is the responsibility of a dedicated module.

**How stop is enforced:** `STATE_PANIC` and `STATE_SAFE` are detected by `motion_manager` via event bus subscription. `motion_manager` calls `motion_stop()` → HAL in response. The kernel does not call `motion_stop()` directly.

---

## DR-07 — No dynamic memory allocation after boot

**Rule:** `malloc()`, `new`, `String` concatenation that grows heap, `std::vector::push_back` beyond reserved capacity, and equivalent dynamic allocation operations are forbidden after `setup()` completes.

**Rationale:** Dynamic allocation on embedded systems causes heap fragmentation over time. A robot running for hours may fail due to allocation failure that does not appear on a 10-minute bench test.

**Permitted:** Static allocation, stack allocation, and fixed-size arrays sized at compile time.

**Permitted during boot:** Initialization-time allocation before `setup()` returns is permitted but must be accounted for in the boot diagnostic heap report.

---

## DR-08 — All `_update()` functions must return within budget

**Rule:** Every `_update()` function must return within its timing budget as defined in `TASK_TIMING.md`. The scheduler measures execution time. Functions that consistently exceed their budget are redesigned, not given a larger budget.

**Rationale:** The cooperative scheduler depends on every task yielding promptly. An overrunning task delays all lower-priority tasks for that scheduler tick.

**When a function cannot meet its budget:** Split it into two tasks with separate intervals, or move the compute-heavy portion to a background RTOS task on Core 0.

---

## DR-09 — No module reads state and acts without `state_get()`

**Rule:** A module that gates behaviour on robot state must call `state_get()` and compare to the defined constants (`STATE_READY`, `STATE_SAFE`, etc.). A module must not maintain its own copy of robot state or infer state from other signals.

**Rationale:** Two modules maintaining separate copies of robot state will eventually diverge. `state_manager` is the single source of truth.

---

## DR-10 — No global mutable state shared between modules

**Rule:** A module's internal variables must be declared `static` within the module's `.cpp` file. No module exposes global mutable variables in its header. Modules share data through function calls and the event bus only.

**Rationale:** Shared mutable globals make the execution order of modules load-bearing in unpredictable ways. A race between two modules touching the same global is extremely difficult to reproduce and diagnose on hardware.

---

## DR-11 — All new tasks registered in priority group order

**Rule:** When adding a new task to the scheduler, it must be registered in `setup()` in the correct priority group position as defined by `TASK_TIMING.md`. The registration order determines dispatch priority within a simultaneous-due tick.

**Rationale:** Out-of-order registration causes a new task to execute before higher-priority tasks that happen to be due in the same tick, silently breaking the execution order invariant.

---

## DR-12 — All modules expose a consistent init/update API

**Rule:** Every Greta module must expose exactly:
```c
void <module>_init(void);
void <module>_update(void);
```
Additional functions are permitted. Renaming `init` to `begin`, `setup`, `start`, or similar is not permitted.

**Rationale:** Consistent naming allows `main.cpp` and the scheduler to treat all modules uniformly. It also makes onboarding new contributors straightforward.

---

## DR-13 — No `Serial.print` on the hot path

**Rule:** `Serial.print()` and `Serial.printf()` must not be called on every execution of a 10 ms or 20 ms task. Serial output at high frequency blocks the UART and inflates task execution time.

**Permitted:** Log once on state change. Log at 100 ms cadence or slower. Log on fault.

**Correct pattern:**
```c
void network_update() {
    health_kick("network");
    // ... logic ...
    // Only log on change, not every tick:
    if (state_changed) {
        Serial.println(F("[NET] link state changed"));
    }
}
```

---

## DR-14 — No magic numbers

**Rule:** All numeric constants with physical or configuration meaning must be defined as named constants in `config.h` or the relevant module's configuration section. Inline literals are only permitted for mathematical constants (e.g., `0`, `1`, `100`, divisors) where the meaning is self-evident from context.

**Examples:**
```c
// Wrong
if (heap < 20000) { ... }
scheduler_add_task(network_update, 10);

// Correct
#define GRETA_MIN_HEAP_BYTES    20000
#define TASK_INTERVAL_CRITICAL_MS  10
if (heap < GRETA_MIN_HEAP_BYTES) { ... }
scheduler_add_task(network_update, TASK_INTERVAL_CRITICAL_MS);
```

---

## DR-15 — Compiler warnings treated as errors

**Rule:** The firmware must compile with zero warnings under `-Wall -Wextra`. All warnings are treated as errors in the CI build configuration (`-Werror`).

**Rationale:** Warnings on embedded systems frequently indicate real bugs — unused variables that shadow intended ones, implicit casts that truncate values, signed/unsigned mismatches that cause incorrect comparisons.

**Process:** A PR that introduces new warnings is blocked until the warning is resolved, not suppressed with `#pragma GCC diagnostic`.
