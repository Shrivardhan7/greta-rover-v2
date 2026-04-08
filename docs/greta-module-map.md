# GRETA OS — Module Map

**Purpose:** Defines ownership, allowed dependencies, and forbidden dependencies for every module in the Greta V2 platform. This document is the authority for import decisions. If a dependency is not listed as allowed, it is forbidden.

---

## 1. Module Registry

Each module lists its layer, its single responsibility, and its public API surface.

| Module | Layer | Owns | Public API |
|---|---|---|---|
| `greta_core` | 2 — Kernel | System identity, boot ID, uptime | `greta_core_init()`, `greta_boot_id()`, `greta_uptime_ms()`, `greta_robot_name()`, `greta_hw_profile()` |
| `scheduler` | 2 — Kernel | Task dispatch, interval tracking | `scheduler_init()`, `scheduler_add_task()`, `scheduler_run_due_tasks()`, `scheduler_task_count()`, `scheduler_fault()` |
| `event_bus` | 2 — Kernel | Publish / subscribe messaging | `event_bus_init()`, `event_publish()`, `event_subscribe()` |
| `health_manager` | 2 — Kernel | Heartbeat registry, fault detection | `health_manager_init()`, `health_register()`, `health_kick()`, `health_manager_fault()` |
| `state_manager` | 2 — Kernel | Robot FSM | `state_init()`, `state_set()`, `state_get()` |
| `system_monitor` | 2 — Kernel | Heap, RSSI, CPU load | `system_monitor_init()`, `system_monitor_update()` |
| `network_manager` | 3 — Comms | WiFi, WebSocket/HTTP | `network_init()`, `network_update()`, `network_wifi_ok()`, `network_set_command_callback()` |
| `bluetooth_bridge` | 3 — Comms | BT/BLE link | `bluetooth_init()`, `bluetooth_update()`, `bluetooth_connected()`, `bluetooth_available()`, `bluetooth_read()` |
| `telemetry` | 3 — Comms | Outbound status frames | `telemetry_init()`, `telemetry_update()`, `telemetry_send_alert()` |
| `command_processor` | 4 — Control | Command validation and dispatch | `command_init()`, `command_receive()`, `command_receive_ack()`, `command_update()` |
| `motion_manager` | 4 — Control | Actuator output via HAL *(future)* | `motion_init()`, `motion_update()`, `motion_stop()` |
| `safety_manager` | 4 — Control | E-stop, tilt, current limits *(future)* | `safety_init()`, `safety_update()`, `safety_is_clear()` |
| `face_engine` | 5 — Personality | GRETA Expression System; deterministic expression state derived from `state_manager` and `behavior_manager` | `face_init()`, `face_update()`, `face_on_state_change()`, `face_on_behavior_update()`, `face_sync_from_system()`, `face_current_expression()`, `face_status()` |
| `vision_manager` | 6 — Brain | Camera pipeline *(future)* | `vision_init()`, `vision_update()` |
| `brain_manager` | 6 — Brain | Planning and inference *(future)* | `brain_init()`, `brain_update()` |

---

## 2. Allowed Dependencies

Dependencies flow downward only. A module may depend on modules in lower-numbered layers and on the HAL. Same-layer dependencies are permitted only through the event bus.

### Core Modules (Layer 2)

| Module | May depend on |
|---|---|
| `greta_core` | Layer 0 (Arduino/ESP-IDF) only |
| `scheduler` | `greta_core`, Layer 0 |
| `event_bus` | Layer 0 only |
| `health_manager` | `scheduler`, `event_bus`, `greta_core` |
| `state_manager` | `event_bus`, `greta_core` |
| `system_monitor` | `health_manager`, `greta_core`, Layer 0 (ESP heap API) |

### Communication Modules (Layer 3)

| Module | May depend on |
|---|---|
| `network_manager` | `state_manager`, `event_bus`, Layer 0 WiFi API |
| `bluetooth_bridge` | `state_manager`, `event_bus`, Layer 0 BT API |
| `telemetry` | `state_manager`, `greta_core`, `network_manager` (send API only) |

### Control Modules (Layer 4)

| Module | May depend on |
|---|---|
| `command_processor` | `state_manager`, `event_bus`, HAL (none directly — via motion) |
| `motion_manager` | `state_manager`, `safety_manager`, HAL (`pwm_hal`, `gpio_hal`) |
| `safety_manager` | `state_manager`, `event_bus`, HAL (`adc_hal`, `gpio_hal`) |

### Personality Modules (Layer 5)

| Module | May depend on |
|---|---|
| `face_engine` | `state_manager`, `behavior_manager`, `event_bus` |

### Brain Modules (Layer 6)

| Module | May depend on |
|---|---|
| `vision_manager` | `event_bus`, HAL (`i2c_hal`, `spi_hal`), Layer 0 camera API |
| `brain_manager` | `event_bus`, `state_manager` (read-only) |

---

## 3. Forbidden Dependencies

The following dependencies are **strictly prohibited**. A pull request introducing any of these is rejected without review.

| Forbidden | Reason |
|---|---|
| Any Layer 3–6 module `#include`ing another Layer 3–6 module directly | Breaks modularity; use event bus |
| `command_processor` calling `motion_manager` directly | Control coupling; must go via event bus or HAL abstraction |
| `network_manager` `#include`ing `command_processor` | Network layer must not know about control layer |
| `brain_manager` writing to HAL directly | Brain layer must not own hardware; route through motion_manager |
| Personality modules influencing mode, safety, task, or motion decisions | Personality layer is deterministic and read-only; it may observe system truth but must not control robot behavior or introduce control-loop timing dependencies |
| Any module calling `delay()` | Blocks cooperative scheduler |
| Any module calling `digitalWrite()`, `analogWrite()` directly (except HAL) | Bypasses hardware abstraction |
| `state_manager` issuing motor commands | Kernel must not own actuators |
| `scheduler` depending on any Layer 3–6 module | Kernel must not know about application modules |

---

## 4. Module Interaction Diagram

```
                          ┌──────────────────────┐
                          │     Dashboard / App   │
                          └──────────┬───────────┘
                                     │ WiFi
                          ┌──────────▼───────────┐
                          │   network_manager     │
                          └──────────┬───────────┘
                                     │ callback
                          ┌──────────▼───────────┐     ┌──────────────────┐
                          │  command_processor    │     │ bluetooth_bridge  │
                          └──────────┬───────────┘     └────────┬─────────┘
                                     │ event_bus                 │ ACK
                          ┌──────────▼───────────┐              │
                          │   state_manager       │◄─────────────┘
                          └──────────┬───────────┘
                      ┌──────────────┼──────────────┐
                      │              │              │
           ┌──────────▼──┐  ┌────────▼───┐  ┌──────▼──────────┐
           │  telemetry  │  │  scheduler │  │ health_manager   │
           └──────────┬──┘  └────────────┘  └──────┬──────────┘
                      │                             │ fault
                      ▼                             ▼
                 Dashboard               state_set(STATE_PANIC)

                          ┌───────────────────────┐
                          │    event_bus           │
                          │  (all modules pub/sub) │
                          └───────────────────────┘
```

All arrows represent allowed data flow directions. The event bus is the horizontal integration plane — it carries notifications between modules at the same layer without creating import dependencies.



