# GRETA OS - Interfaces Reference

**Project:** GRETA OS - Companion Robotics Platform  
**Author:** Shrivardhan Jadhav  
**Year:** 2026  
**Status:** Public interface reference

---

## 1. Purpose

This document defines the current GRETA OS module interfaces, allowed dependencies, and architectural boundaries.

It is a reference for:

- firmware integration
- module ownership
- safe dependency decisions
- future extension without breaking control boundaries

This document describes the existing structure. It does not redefine architecture.

See also:

- [greta-module-map.md](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/docs/greta-module-map.md)
- [control_flow.md](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/docs/control_flow.md)
- [design-rules.md](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/docs/design-rules.md)

---

## 2. Interface Model

GRETA OS uses a small, explicit module contract style.

Common conventions:

- modules expose `*_init()` and `*_update()` where appropriate
- public headers define behavior contracts, not implementation details
- timing is owned by `scheduler`
- system truth is owned by `state_manager`
- decision authority is owned by `behavior_manager`
- execution ownership is owned by `task_manager`
- safety must not depend on personality or presentation modules

Interface rule:

- if a function is not declared in a module's public header, it is not part of the supported external interface

---

## 3. Layer Summary

| Layer | Role | Current modules |
|---|---|---|
| 2 - Kernel | timing, state truth, events, health | `scheduler`, `event_bus`, `state_manager`, `health_manager` |
| 3 - Communication | ingress, egress, transport status | `network_manager`, `bluetooth_bridge`, `telemetry` |
| 4 - Control | command gating, mode, task ownership, arbitration | `command_processor`, `mode_manager`, `task_manager`, `behavior_manager` |
| 5 - Personality | deterministic read-only expression/personality observers | `face_engine`, `voice_engine` |
| 6 - Brain | future advisory intelligence modules | `vision_engine` |

Notes:

- `config.h` is a shared configuration contract, not a behavior-owning module.
- `voice_engine` and `vision_engine` are currently interface stubs.
- `face_engine` is the GRETA Expression System interface and remains non-rendering.

---

## 4. Public Interfaces

### `config.h`

Responsibility:

- compile-time system constants
- protocol strings
- timing thresholds
- telemetry field keys
- feature flags

Boundary:

- modules may read configuration values
- modules must not hardcode duplicate timing or protocol constants when `config.h` already defines them

### `scheduler`

Header:

- [scheduler.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/scheduler.h)

Public API:

- `scheduler_init(void)`
- `scheduler_tick(void)`
- `scheduler_due(TaskID task)`
- `scheduler_get_loop_time_ms(void)`

Owns:

- periodic task cadence
- loop timing measurement

Boundary:

- scheduler decides when work runs
- scheduler does not call application modules directly
- scheduler does not decide safety or policy

### `event_bus`

Header:

- [event_bus.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/event_bus.h)

Public API:

- `event_bus_init(void)`
- `event_subscribe(EventChannel channel, EventHandler handler)`
- `event_publish(EventChannel channel, const EventPayload* payload)`

Owns:

- synchronous publish-subscribe notifications

Boundary:

- event handlers must be non-blocking
- subscriptions are registered at init time
- event bus is not a motion or safety authority

### `state_manager`

Header:

- [state_manager.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/state_manager.h)

Public API:

- `state_init()`
- `state_update()`
- `state_set(RobotState next, const char* reason = "")`
- `state_get()`
- `state_name()`
- `state_last_reason()`
- `state_entered_ms()`
- `state_can_move()`
- `state_is_halted()`
- `state_is_online()`

Owns:

- authoritative robot operational state

Boundary:

- `state_manager` is the system source of truth
- other modules may read state and request state transitions
- `state_manager` must not command motion directly

### `health_manager`

Header:

- [health_manager.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/health_manager.h)

Public API:

- `health_manager_init(void)`
- `health_manager_update(void)`
- `health_manager_record_rssi(int32_t rssi_dbm)`
- `health_get_score(void)`
- `health_get_status(void)`
- `health_get_report(void)`

Owns:

- composite health scoring
- health status publication

Boundary:

- health monitors conditions
- health does not directly command safety state or motion
- corrective action belongs to behavior and state layers

### `network_manager`

Header:

- [network_manager.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/network_manager.h)

Public API:

- `network_init()`
- `network_update()`
- `network_set_command_callback(CommandCallback cb)`
- `network_broadcast(const char* msg)`
- `network_on_pong()`
- `network_wifi_ok()`
- `network_ws_client_connected()`
- `network_rssi()`
- `network_ssid()`

Owns:

- WiFi connectivity
- WebSocket lifecycle
- dashboard ingress callback routing

Boundary:

- network layer moves strings and link status
- network layer must not interpret movement policy
- network layer must not own motion authority

### `bluetooth_bridge`

Header:

- [bluetooth_bridge.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/bluetooth_bridge.h)

Public API:

- `bluetooth_init()`
- `bluetooth_update()`
- `bluetooth_available()`
- `bluetooth_read()`
- `bluetooth_connected()`
- `bluetooth_last_rx_ms()`

Owns:

- raw line-oriented UART link state

Boundary:

- bridge owns bytes and lines, not command meaning
- public interface exposes receive/status only
- motion transmit is intentionally not part of the public header contract

### `telemetry`

Header:

- [telemetry.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/telemetry.h)

Public API:

- `telemetry_init()`
- `telemetry_update()`
- `telemetry_build(char* buf, size_t bufLen)`

Owns:

- read-only dashboard status snapshots

Boundary:

- telemetry aggregates existing module state
- telemetry must not become a shared control-state store
- telemetry must not influence behavior or motion decisions

### `mode_manager`

Header:

- [mode_manager.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/mode_manager.h)

Public API:

- `mode_init(void)`
- `mode_update(void)`
- `mode_request(RoverMode requested, const char* reason)`
- `mode_request_from_string(const char* mode_str, const char* reason)`
- `mode_force(RoverMode next, const char* reason)`
- `mode_get(void)`
- `mode_name(void)`
- `mode_last_reason(void)`
- `mode_is_motion_permitted(void)`

Owns:

- operating mode selection and mode-level permission gates

Boundary:

- modes define permission categories
- mode selection is not the same as task ownership
- mode logic must remain consistent with `state_manager`

### `behavior_manager`

Header:

- [behavior_manager.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/behavior_manager.h)

Public API:

- `behavior_manager_init(void)`
- `behavior_manager_update(void)`
- `behavior_handle_mode_request(const char* mode_str)`
- `behavior_evaluate_command(const char* cmd, CommandSource source)`
- `behavior_dispatch_command(const char* cmd, CommandSource source, const char** reason_out)`
- `behavior_note_motion_command(CommandSource source, const char* cmd)`
- `behavior_note_stop_command(const char* reason)`
- `behavior_note_command_rejected(const char* cmd, const char* reason)`
- `behavior_force_safe(const char* reason)`
- `behavior_is_safety_latched(void)`
- `behavior_last_safety_reason(void)`

Owns:

- command admission decisions
- safety latching
- arbitration between state, mode, health, and task ownership

Boundary:

- behavior layer decides what the robot may do
- behavior layer does not replace `state_manager` as truth authority
- behavior layer is the policy gateway before motion execution

### `task_manager`

Header:

- [task_manager.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/task_manager.h)

Public API:

- `task_manager_init(void)`
- `task_manager_update(void)`
- `task_manager_activate(GretaTaskId task, const char* note)`
- `task_manager_interrupt(GretaTaskId task, const char* note)`
- `task_manager_clear(GretaTaskId task, const char* note)`
- `task_manager_reset_to_idle(const char* note)`
- `task_manager_active(void)`
- `task_manager_active_priority(void)`
- `task_manager_active_name(void)`
- `task_manager_status(GretaTaskId task)`

Owns:

- execution ownership
- task lifecycle
- task priority ordering

Boundary:

- public interface exposes task state, not raw motion dispatch
- task manager determines who currently owns execution
- motion dispatch internals are not part of the public module contract

### `command_processor`

Header:

- [command_processor.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/command_processor.h)

Public API:

- `command_init()`
- `command_update()`
- `command_receive(const char* cmd)`
- `command_receive_ack(const char* ack)`
- `command_last()`
- `command_last_latency_ms()`
- `command_waiting_ack()`

Owns:

- whitelist validation
- watchdog handling
- ACK correlation
- ingress normalization between network input and behavior policy

Boundary:

- command processor validates and requests
- command processor does not own final motion authority
- command processor must route motion through behavior policy

### `face_engine`

Header:

- [face_engine.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/face_engine.h)

Public API:

- `face_init(void)`
- `face_update(void)`
- `face_on_state_change(RobotState prev_state, RobotState new_state)`
- `face_on_behavior_update(void)`
- `face_sync_from_system(void)`
- `face_current_expression(void)`
- `face_status(void)`

Owns:

- GRETA expression state selection

Boundary:

- `face_engine` is a personality observer
- it derives expression from state and behavior signals
- it does not render pixels in this interface
- it must remain deterministic and must not influence control timing, safety, mode, task, or motion

### `voice_engine` *(stub interface)*

Header:

- [voice_engine.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/voice_engine.h)

Public API:

- `voice_init()`
- `voice_update()`
- `voice_set_language(VoiceLanguage lang)`
- `voice_speak(VoicePhrase phrase)`
- `voice_set_volume(uint8_t vol)`
- `voice_on_state_change(RobotState prevState, RobotState newState)`

Owns:

- future voice/personality output contract

Boundary:

- voice remains asynchronous and non-blocking
- voice is presentation/personality, not control authority

### `vision_engine` *(stub interface)*

Header:

- [vision_engine.h](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/esp32-brain/include/vision_engine.h)

Public API:

- `vision_init()`
- `vision_update()`
- `vision_result_ready()`
- `vision_get_result()`

Owns:

- future advisory perception result contract

Boundary:

- vision results are advisory
- vision must not directly modify robot state or motion

---

## 5. Allowed Dependencies

The dependency rule is simple:

- a module may depend on lower layers
- same-layer coupling should be avoided and must not create control ambiguity
- state, behavior, and task authority must not be bypassed

Current dependency guidance for the active structure:

| Module | Allowed dependencies |
|---|---|
| `scheduler` | platform timing only |
| `event_bus` | platform types only |
| `state_manager` | platform types, optional event publication |
| `health_manager` | `event_bus`, `scheduler` |
| `network_manager` | `behavior_manager`, `health_manager`, `state_manager`, platform WiFi/WebSocket APIs |
| `bluetooth_bridge` | `behavior_manager`, `config.h`, platform UART APIs |
| `telemetry` | read-only status modules such as `state_manager`, `mode_manager`, `network_manager`, `bluetooth_bridge`, `command_processor`, `health_manager` |
| `mode_manager` | `state_manager`, `event_bus`, `network_manager` |
| `behavior_manager` | `state_manager`, `mode_manager`, `task_manager`, `health_manager`, `event_bus`, selected link-status readers |
| `task_manager` | `config.h`, internal motion-link implementation |
| `command_processor` | `behavior_manager`, `state_manager`, `network_manager`, `bluetooth_bridge`, `config.h` |
| `face_engine` | `state_manager`, `behavior_manager`, read-only observer signals |
| `voice_engine` | `state_manager` |
| `vision_engine` | future camera/perception back-end only |

Dependency note:

- [greta-module-map.md](C:/Users/shriv/OneDrive/Desktop/greta-rover-v2/docs/greta-module-map.md) remains the repository authority for dependency policy

---

## 6. Architectural Boundaries

The following boundaries define GRETA OS module behavior.

### Timing boundary

- only `scheduler` owns execution cadence
- modules must not create private timing loops that redefine system cadence

### Truth boundary

- only `state_manager` owns the authoritative robot state
- no other module may maintain a competing operational truth source

### Decision boundary

- `behavior_manager` decides whether requested actions are allowed
- safety escalation converges through behavior policy

### Execution boundary

- `task_manager` owns active execution role and priority
- task ownership must be resolved before motion execution proceeds

### Motion boundary

- public control flow is:
  - `command_processor -> behavior_manager -> task_manager -> motion link`
- modules must not bypass this path through public interfaces

### Communication boundary

- `network_manager` and `bluetooth_bridge` move data and link status
- they must stay thin and must not become alternate policy layers

### Personality boundary

- personality modules are deterministic read-only observers
- they must not influence safety, motion, task, or mode decisions
- they must not introduce timing dependencies into the control loop

---

## 7. Interface Rules For New Modules

New modules should follow the existing GRETA OS interface style.

Required traits:

- a small public header
- clear single ownership
- non-blocking `*_update()` if scheduled
- no hidden safety authority
- no duplicate state truth
- no direct public motion bypass

Recommended header shape:

```c
void module_init(void);
void module_update(void);
```

Add read-only accessors only when another module actually needs them.

---

## 8. Stability Notes

Current interface maturity:

- stable active interfaces: `scheduler`, `event_bus`, `state_manager`, `health_manager`, `network_manager`, `bluetooth_bridge`, `command_processor`, `mode_manager`, `behavior_manager`, `task_manager`, `telemetry`
- controlled personality interface: `face_engine`
- stub or future-facing interfaces: `voice_engine`, `vision_engine`

Repository rule:

- implementation details may change
- public interface intent should remain stable unless the architecture intentionally changes and the documentation is updated in the same change set

---

## 9. SDK View

GRETA OS should be read as a modular firmware SDK with these core contracts:

```text
scheduler        -> when work runs
state_manager    -> what the robot is
behavior_manager -> what the robot may do
task_manager     -> who owns execution
command_processor-> how commands enter policy
network_manager  -> how external control connects
bluetooth_bridge -> how the motion link is monitored
telemetry        -> how status is exported
face_engine      -> how GRETA expresses system condition
```

This interface model keeps GRETA OS modular, deterministic, and ready for staged robotics integration.

