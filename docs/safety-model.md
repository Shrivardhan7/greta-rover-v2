# GRETA OS — Safety Model

**Safety classification:** Functional safety — non-certified, best-effort  
**Philosophy:** Default to the most restrictive safe state on any uncertainty. Panic before undefined behaviour. The kernel does not command actuators.

---

## 1. State Machine

The robot operates in exactly one state at all times. `state_manager` is the single authority for the current state. No module may change state except by calling `state_set()`.

```
              ┌──────────────┐
    boot ────►│  CONNECTING  │
              └──────┬───────┘
                     │ WiFi ok + BT ok
              ┌──────▼───────┐
         ┌───►│    READY     │◄──────────────────┐
         │    └──────┬───────┘                   │
         │           │ link lost                 │ links restored
         │    ┌──────▼───────┐                   │
         │    │     SAFE     │───────────────────►┘
         │    └──────┬───────┘
         │           │ health fault / scheduler fault
         │    ┌──────▼───────┐
         └────│    PANIC     │  (no exit — watchdog reset only)
              └──────────────┘
```

### State definitions

| State | Movement | Comms | Description |
|---|---|---|---|
| `CONNECTING` | Disabled | Partial | Boot phase; links not yet established |
| `READY` | Enabled | Full | All links healthy; normal operation |
| `SAFE` | Disabled | Full | Link degraded but recoverable; awaiting restoration |
| `PANIC` | Disabled | Alert only | Unrecoverable fault detected; safe loop active |

---

## 2. Panic State

### Trigger conditions

`STATE_PANIC` is entered from any state when any of the following occur:

| Trigger | Source | Escalation call |
|---|---|---|
| Health manager fault (module heartbeat timeout) | `health_manager_fault()` | `link_recovery_update()` |
| Scheduler fault (task overrun threshold exceeded) | `scheduler_fault()` | `link_recovery_update()` |
| Memory exhaustion (heap below minimum threshold) | `system_monitor_update()` | `state_set(STATE_PANIC,...)` |
| WiFi timeout critical (extended loss beyond threshold) | `network_manager` via event bus | `state_set(STATE_PANIC,...)` |

### Panic behaviour

1. `state_manager` receives `state_set(STATE_PANIC, reason)`.
2. `state_manager` publishes `EVENT_PANIC` on the event bus.
3. Any registered motion module receives the event and calls `motion_stop()` → HAL sets all PWM outputs to zero.
4. `command_processor` gates all incoming commands — no motion commands are dispatched.
5. `main.cpp` detects `STATE_PANIC` at top of `loop()` and enters `panic_safe_loop()`.
6. `panic_safe_loop()` transmits a telemetry alert every 2 seconds if WiFi is available.
7. No state transition out of `PANIC` is performed in software. Recovery requires a hardware watchdog reset.

### Panic safe-loop constraints

- No blocking calls.
- No motor output.
- No command processing.
- Serial alert printed every 2 seconds.
- Telemetry alert sent every 2 seconds (guarded by `network_wifi_ok()`).
- `yield()` called each iteration to prevent FreeRTOS watchdog from triggering a hard fault.

---

## 3. Safe State

`STATE_SAFE` is a recoverable degraded state, distinct from `STATE_PANIC`.

### Trigger conditions

- WiFi link drops while in `STATE_READY`.
- Bluetooth link drops while in `STATE_READY`.
- Either link is absent for longer than the link-loss detection window (evaluated in `link_recovery_update()` every loop tick).

### Safe state behaviour

- Movement commands are disabled.
- Communication modules remain active.
- System monitor continues to report.
- Health heartbeats continue.
- `link_recovery_update()` polls `network_wifi_ok() && bluetooth_connected()` every loop iteration.

### Recovery from SAFE

When both links are confirmed healthy:

```
state_set(STATE_READY, "links restored")
health_kick("network")
health_kick("bluetooth")
```

No other module takes action. Recovery is exclusively driven by `link_recovery_update()` in `main.cpp`.

---

## 4. Health Monitoring

Every schedulable module registers a named heartbeat at boot:

```c
health_register("network");
health_register("bluetooth");
health_register("command");
health_register("telemetry");
health_register("system_monitor");
```

Each module must call `health_kick("<name>")` on every scheduled execution. If a module fails to kick within its configured timeout window, `health_manager_fault()` returns `true`.

### Fault hysteresis

A single missed kick does not trigger a fault. `health_manager` applies a missed-beat count threshold before declaring a fault. This prevents transient scheduler jitter from causing false panics.

| Parameter | Recommended value |
|---|---|
| Kick timeout per module | 3× the module's scheduled interval |
| Fault threshold | 3 consecutive missed kicks |

### What health monitoring detects

- Task stall (module stops executing entirely).
- Task timeout (module takes too long and misses its own kick window).
- Scheduler fault (no tasks running — all kicks missing simultaneously).

---

## 5. Watchdog Behaviour

Greta OS Phase 1 does not directly configure the ESP32 hardware watchdog. The cooperative scheduler must not stall.

Planned Phase 2 watchdog integration:
- `greta_core_init()` registers the main task with `esp_task_wdt_add()`.
- `system_monitor_update()` calls `esp_task_wdt_reset()` on each 100 ms tick.
- If `system_monitor_update()` does not run within the watchdog timeout, the ESP32 resets.
- Watchdog timeout set to 5 seconds (50× the monitor interval) to absorb scheduler jitter.

---

## 6. Link Recovery

Link recovery logic is centralised in `link_recovery_update()` inside `main.cpp`. No module performs its own link recovery.

```
link_recovery_update() called every loop() iteration:

  wifi = network_wifi_ok()
  bt   = bluetooth_connected()
  ok   = wifi && bt

  if STATE_CONNECTING && ok  → STATE_READY  "links up"
  if STATE_SAFE      && ok  → STATE_READY  "links restored"
  if not STATE_PANIC:
    if health_manager_fault() → STATE_PANIC  "health fault"
    if scheduler_fault()      → STATE_PANIC  "scheduler fault"
```

---

## 7. Motor Disable Logic

Greta OS Phase 1 does not directly control motors. Motor disable is enforced through the state gate in `command_processor`:

- `command_processor` checks `state_get()` before dispatching any motion command.
- If state is not `STATE_READY`, motion commands are dropped and an ACK error is returned.
- In Phase 2, `motion_manager` will additionally subscribe to `EVENT_PANIC` and call `motion_stop()` → HAL → PWM = 0 immediately, independent of `command_processor`.

This two-layer gate ensures motors are disabled even if a command is mid-dispatch when panic occurs.

---

## 8. Fault Escalation Path

```
Module misses heartbeat (×3)
        │
        ▼
health_manager_fault() → true
        │
        ▼
link_recovery_update() detects fault
        │
        ▼
state_set(STATE_PANIC, "health fault")
        │
        ├─► event_bus: EVENT_PANIC published
        │       └─► motion_manager: motion_stop() [future]
        │       └─► safety_manager: e-stop assert [future]
        │
        ├─► main.cpp: panic guard triggers panic_safe_loop()
        │
        └─► telemetry: alert frame sent to dashboard
```

---

## 9. Safety Philosophy

**Fail safe, not fail operational.** In the presence of uncertainty, the robot stops. Resuming operation requires verified link restoration and a clean health state — not a timeout.

**No autonomous recovery from panic.** Recovery from `STATE_PANIC` requires a physical watchdog reset. Software self-recovery introduces the risk of resuming in an undefined state.

**Kernel does not own actuators.** The state machine and scheduler have no knowledge of motors, servos, or actuators. Actuator state is owned exclusively by `motion_manager` (future), which responds to events. This prevents the kernel from ever issuing an unintended motion command.

**Single state authority.** `state_manager` is the only module that records and transitions the robot state. No module infers state from other signals.
