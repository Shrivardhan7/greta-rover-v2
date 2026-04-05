# GRETA OS — Task Timing Model

**Scheduler type:** Cooperative, non-preemptive  
**Tick resolution:** 1 ms (based on `millis()`)  
**Task dispatch:** First-registered, first-dispatched within a simultaneous-due group

---

## 1. Scheduler Philosophy

Greta OS uses a cooperative scheduler rather than a preemptive RTOS scheduler for Phase 1. This is a deliberate design decision:

- **Determinism over throughput.** A cooperative model makes task interleaving explicit and auditable. There are no hidden context switches.
- **No stack overhead per task.** Every task shares the single `loop()` stack. No per-task stack allocation required.
- **Simple fault detection.** A task that blocks will stall the entire loop, making stalls immediately visible to the health manager rather than silently degrading one task.
- **ESP32 compatibility.** The Arduino `loop()` model on ESP32 is inherently single-threaded on Core 1. A cooperative scheduler is a natural fit.

**Consequence:** Every `_update()` function must complete within its timing budget and return. A function that calls `delay()`, polls a blocking socket, or spins on a hardware flag violates this contract and stalls all lower-priority tasks.

---

## 2. Task Priority Groups

Tasks are grouped by criticality. The group determines the interval and the consequence of missing a deadline.

### Group 1 — CRITICAL (10 ms)

Tasks in this group process inbound data. A missed deadline here means commands are not ingested in time and the link watchdog may incorrectly declare a loss.

| Task | Interval | Reason |
|---|---|---|
| `network_update` | 10 ms | WiFi ingress; commands must enter pipeline before watchdogs fire |
| `bluetooth_update` | 10 ms | BT ACK processing; paired with network cadence |

10 ms rationale: Round-trip human reaction time over WiFi is 20–50 ms. Processing inbound data every 10 ms ensures commands are never delayed more than one extra scheduler quantum beyond network arrival.

### Group 2 — CONTROL (20 ms)

Tasks that act on data ingested by Group 1. Must run after Group 1 has processed the current tick.

| Task | Interval | Reason |
|---|---|---|
| `command_update` | 20 ms | Watchdogs and command timeouts evaluated after ingress |
| `safety_update` *(future)* | 20 ms | Hardware safety checks require high cadence |

20 ms rationale: Control loop frequency of 50 Hz is consistent with hobby servo update rates and provides adequate responsiveness for teleoperated motion.

### Group 3 — STATE (50 ms)

Tasks that maintain the robot's operational state. These run less frequently because state transitions are not time-critical at millisecond resolution.

| Task | Interval | Reason |
|---|---|---|
| `state_update` | 50 ms | FSM bookkeeping; depends on fresh command pipeline |
| `motion_update` *(future)* | 50 ms | Motion profile updates at 20 Hz |

50 ms rationale: The FSM does not need to poll faster than human-scale decision-making. 20 Hz is adequate for smooth state transitions.

### Group 4 — TELEMETRY / MONITOR (100 ms)

Outbound-only tasks. These tasks read system state and transmit it. They have no effect on robot behaviour and are safe to run at lower priority.

| Task | Interval | Reason |
|---|---|---|
| `telemetry_update` | 100 ms | Dashboard refresh rate; saturating WiFi faster provides no benefit |
| `system_monitor_update` | 100 ms | Heap and RSSI change slowly; 10 Hz sampling is sufficient |

100 ms rationale: Dashboard WebSocket update at 10 Hz is visually smooth for human operators. Higher rates would waste WiFi bandwidth without improving situational awareness.

### Group 5 — SLOW (500 ms) *(future)*

Low-frequency background tasks. These tasks must not cause jitter in higher-priority groups.

| Task | Interval | Reason |
|---|---|---|
| `vision_update` | 500 ms | Camera inference is compute-heavy; 2 Hz is the target frame rate for Phase 3 |
| `brain_update` | 500 ms | Planning tick; high-level decisions update at 2 Hz |
| `ota_update` | 500 ms | OTA polling; low urgency, must not block network tasks |

---

## 3. Task Execution Time Budgets

Each task must complete within its budget. The scheduler measures execution time for each task. Exceeding the budget triggers a log warning. Repeated overruns (configurable threshold) trigger `scheduler_fault()`.

| Group | Interval | Maximum execution budget |
|---|---|---|
| CRITICAL | 10 ms | 2 ms |
| CONTROL | 20 ms | 5 ms |
| STATE | 50 ms | 10 ms |
| TELEMETRY | 100 ms | 20 ms |
| SLOW | 500 ms | 50 ms |

Budget rationale: The budget is set to 20% of the task interval for CRITICAL tasks and up to 40% for SLOW tasks. This leaves margin for other tasks that may also be due in the same scheduler tick.

---

## 4. Inline Loop Tasks (Not Scheduler-Managed)

Two operations run inline in `loop()` on every iteration, outside the scheduler:

| Operation | Why inline |
|---|---|
| `bluetooth_ack_drain()` | Must execute immediately after `bluetooth_update()` in the same tick to avoid a read-before-update race. Scheduling independently would decouple the pair. |
| `link_recovery_update()` | Link loss must be detected within one scheduler quantum regardless of which tasks were due. Running every tick provides the minimum possible detection latency. |

---

## 5. Scheduler Fault Detection

The scheduler is responsible for detecting its own failure modes:

| Fault | Detection method | Response |
|---|---|---|
| Task overrun (single) | Execution time > budget | Log warning, health kick suppressed |
| Task overrun (repeated) | Consecutive overruns > threshold | `scheduler_fault()` → `STATE_PANIC` |
| Null task registered | `scheduler_add_task(NULL, ...)` | Rejected at registration; logged |
| Duplicate task registered | Same function pointer registered twice | Rejected at registration; logged |
| Task count exceeded | `scheduler_add_task()` when at `MAX_TASKS` | Rejected; boot halted with error |

---

## 6. MAX_TASKS Allocation

`MAX_TASKS` is defined at compile time in `scheduler.h`. The budget covers current tasks plus future module slots.

| Category | Task slots |
|---|---|
| Current Phase 1 tasks | 6 |
| Future control tasks (motion, safety) | 2 |
| Future brain tasks (vision, brain, OTA) | 3 |
| Reserve | 1 |
| **Total MAX_TASKS** | **12** |

---

## 7. Timing Expansion for Future Phases

When new modules are added, their tasks must be registered in the correct priority group during `setup()`. The scheduler's task registration order determines dispatch priority within a simultaneous-due tick.

Rules for adding tasks:
1. Determine the correct priority group from the table above.
2. Add `scheduler_add_task(module_update, interval_ms)` in group order within `setup()`.
3. Register a health heartbeat: `health_register("module_name")`.
4. Call `health_kick("module_name")` at the start of each `module_update()`.
5. Verify `scheduler_task_count()` does not exceed `MAX_TASKS - 1` (one slot kept in reserve).
