# GRETA OS - Control Flow

**Project:** GRETA OS - Companion Robotics Platform  
**Author:** Shrivardhan Jadhav  
**Year:** 2026  
**Status:** Engineering reference

See also: [First Motor Bring-Up Procedure](first_motor_bringup.md)

---

## 1. System Execution Overview

GRETA OS uses a cooperative execution model. No module owns its own thread. All runtime work is advanced from the main loop under scheduler timing.

Core execution philosophy:

- `scheduler` controls when work is allowed to run.
- `state_manager` holds the authoritative system state.
- `behavior_manager` decides what actions are permitted.
- `task_manager` tracks which execution role currently owns control.
- `health_manager` monitors health and fault indicators.
- `command_processor` validates and routes input commands.
- `network_manager` manages communication links and dashboard ingress.

This separation is intentional:

- timing is not mixed with decision logic
- safety decisions are not mixed with communication handling
- command acceptance is not the same as command authority

High-level relationship in the current firmware:

```text
Scheduler -> State Manager -> Behavior Manager -> Task Manager -> Command Processor
Scheduler -> Network Manager -> command callback -> Behavior / Command path
Scheduler -> Health Manager -> event_bus -> Behavior Manager
Bluetooth Bridge -> ACK path -> Command Processor
```

Module interaction hierarchy:

```text
+------------------+
|    Scheduler     |
+---------+--------+
          |
          +--------------------->+------------------+
          |                      |   Health Manager |
          |                      +---------+--------+
          |                                |
          |                                v
          |                      +---------+--------+
          +--------------------->+   State Manager  |
          |                      +---------+--------+
          |                                |
          |                                v
          |                      +---------+--------+
          +--------------------->+ Behavior Manager |
          |                      +---------+--------+
          |                                |
          |                                v
          |                      +---------+--------+
          +--------------------->+   Task Manager   |
          |                      +---------+--------+
          |                                |
          |                                v
          |                      +---------+--------+
          +--------------------->+ Command Processor|
          |                      +---------+--------+
          |                                ^
          |                                |
          |                      +---------+--------+
          +--------------------->+  Network Manager |
          |                      +------------------+
          |
          +--------------------->+------------------+
                                 | Bluetooth Bridge |
                                 +---------+--------+
                                           |
                                           v
                                 +---------+--------+
                                 |   ACK handling   |
                                 +------------------+
```

Design rule:

- The scheduler decides when modules run.
- The state manager defines what the system is.
- The behavior manager defines what the system may do.
- The task manager defines who currently owns execution.

---

## 2. Boot Sequence

Current initialization order in `main.cpp`:

```text
1. config load
2. event_bus init
3. state_manager init
4. bluetooth init
5. command_processor init
6. mode_manager init
7. task_manager init
8. behavior_manager init
9. network_manager init
10. scheduler start
11. health_manager init
```

Reason for this order:

- Configuration must be available before any module reads timing, thresholds, or protocol constants.
- `event_bus` initializes first so later modules can subscribe safely.
- `state_manager` initializes before communication and command handling so the system has valid truth before external inputs appear.
- `bluetooth_bridge` and `command_processor` initialize before network ingress because command forwarding depends on both.
- `mode_manager`, `task_manager`, and `behavior_manager` initialize before the dashboard can influence behavior.
- `network_manager` initializes only after the command callback path is ready.
- `scheduler` starts after all runtime modules are initialized.
- `health_manager` initializes after scheduler startup because it consumes scheduler timing data.

Boot principle:

- Do not allow live communication or action processing before state, behavior, and task ownership are defined.

---

## 3. Main Loop Execution Order

The main loop should preserve a fixed priority order.

Current loop structure in `main.cpp`:

```text
loop()
  1. scheduler due check: state update
  2. scheduler due check: mode update
  3. scheduler due check: behavior arbitration
  4. scheduler due check: command watchdog / command processing support
  5. scheduler due check: task update
  6. scheduler due check: network handling
  7. scheduler due check: bluetooth update
  8. scheduler due check: telemetry update
  9. scheduler due check: health update
 10. inline bluetooth ACK drain
 11. scheduler loop timing tick
```

Operational interpretation:

- `scheduler` gates execution frequency.
- `state_manager` updates before action decisions so arbitration reads current truth.
- `behavior_manager` runs before command forwarding so unsafe or unauthorized actions are blocked early.
- `task_manager` resolves ownership before multiple subsystems can compete for control.
- `command_processor` watchdog logic runs inside the control block, after mode and behavior updates.
- `network_manager` handles communication after the control block in the current loop.
- `bluetooth_ack_drain()` runs inline after `bluetooth_update()` so ACK parsing always sees fresh serial data.
- `health_manager` runs on a slower interval than the control path and publishes warning events for behavior arbitration.

Safety priority rule:

- Any detected safety condition must be handled before motion-related execution continues.

---

## 4. Decision Hierarchy

Control authority is not equal across modules.

Decision hierarchy in the current firmware:

```text
Behavior Manager    -> decides allowed actions and arbitration outcome
Task Manager        -> determines active execution ownership
Command Processor   -> requests / forwards actions through policy gates
Health Manager      -> publishes health warnings consumed by behavior logic
Network Manager     -> may directly force state SAFE on link faults
Scheduler           -> controls timing only
```

Detailed meaning:

- `health_manager` observes fault indicators and publishes warning events. `behavior_manager` escalates to `SAFE` when health is critical.
- `behavior_manager` is the policy layer. It decides whether motion, autonomy, or task activation is allowed.
- `task_manager` determines which active role currently owns control, such as manual drive, autonomy, or safety hold.
- `command_processor` does not own authority. It processes requests and forwards only approved actions.
- `network_manager` is not meant to be a policy owner, but in the current firmware it still sets `STATE_SAFE` directly on WiFi, heartbeat, and WebSocket faults.
- `scheduler` never decides correctness or safety. It only determines execution timing.

System rule:

- Safety overrides all other authority.

---

## 5. Mode and Task Interaction

Modes define permission boundaries. Tasks define active ownership.

Mode effect:

- `IDLE` restricts active motion authority.
- `MANUAL` allows operator-driven motion.
- `AUTONOMOUS` allows onboard control logic to own motion.
- `SAFE` blocks motion except required stop behavior.
- `ERROR` blocks normal execution and requires recovery handling.

Task interaction:

- `task_manager` resolves which task currently owns control.
- If two request paths compete, task priority resolves the conflict deterministically.
- A safety-hold task must preempt all other tasks.

Expected relationship:

```text
Mode -> defines what categories of action are allowed
Task -> defines which active owner may use that permission
Behavior -> resolves the final decision
```

---

## 6. Safety Flow

### Health fault occurs

Typical flow:

```text
health_manager detects warning / critical condition
-> event_bus publishes EVENT_HEALTH_WARNING
-> behavior_manager observes health status on its next update
-> if health is critical, behavior_manager requests SAFE response
-> task_manager activates SAFETY_HOLD
-> state_manager records SAFE transition and reason
-> command_processor blocks further motion commands
```

### Communication drops

Typical flow:

```text
network_manager or link watchdog detects loss
-> current firmware may set STATE_SAFE directly
-> behavior_manager sees unhealthy links on its update
-> SAFE mode / safety hold is enforced
-> active motion ownership is interrupted
-> recovery is allowed only after links are healthy again and health is not critical
```

### Invalid command received

Typical flow:

```text
command_processor receives request
-> command whitelist / protocol check
-> behavior_manager permission check
-> reject command if invalid or unauthorized
-> state remains unchanged unless safety action is required
```

SAFE transition rule:

- `SAFE` is the default response to uncertainty, link loss, or a health condition that invalidates continued motion.

---

## 7. Debugging Value

This structure reduces integration ambiguity during hardware testing.

Debugging benefits:

- timing issues can be isolated to scheduler configuration
- incorrect state transitions can be traced to `state_manager`
- unsafe action acceptance can be traced to `behavior_manager`
- ownership conflicts can be traced to `task_manager`
- malformed or unexpected operator inputs can be traced to `command_processor`
- communication failures can be traced to `network_manager`

For hardware integration, this means:

- a motor event can be traced back to a decision path
- a stop condition can be traced back to a fault source
- a rejected command can be traced back to policy, state, mode, or task ownership

This makes field debugging more predictable and reduces guesswork when multiple modules interact.

---

## 8. Future Extension Notes

New modules should connect through existing control layers rather than creating parallel control paths.

Integration guidance:

- New decision-making modules should connect through the `behavior_manager`.
- New action ownership models should register through the `task_manager`.
- New periodic runtime work should be added through scheduler timing, not ad hoc loop checks.
- New notifications should use the `event_bus` for decoupled observation where appropriate.
- New communication fault paths should prefer routing through `behavior_manager` instead of setting state directly.

Examples:

- Autonomy modules should request control through the behavior layer and claim ownership through the task layer.
- Sensor fault modules should report into health or safety handling, not directly command motion.
- Diagnostic modules should observe state and events, not become new control authorities.

Extension rule:

- Do not bypass state, behavior, or task ownership when introducing new hardware-facing logic.

---

## Control Summary

```text
Scheduler      = when work runs
State Manager  = what the system is
Behavior Layer = what the system may do
Task Layer     = who currently owns execution
Health Manager = whether operation is still safe
Command Path   = how requests enter the system
Network Layer  = how communication enters and leaves the system
```

This control flow keeps GRETA OS predictable, testable, and suitable for staged hardware integration.

