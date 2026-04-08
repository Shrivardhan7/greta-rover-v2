# GRETA OS — System Architecture

**Platform:** Greta V2  
**Target MCU:** ESP32 (Xtensa LX6, dual-core, 240 MHz)  
**Kernel model:** Cooperative real-time, single-core execution  
**Document status:** Living document — update on each architectural change

---

## 1. System Layer Model

Greta OS is structured into five horizontal layers. Each layer may only depend on layers below it. No upward dependencies are permitted.

```
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 5 — APPLICATION / BRAIN LAYER                           │
│  brain_manager  vision_manager  mission_planner  (future)      │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 4 — CONTROL LAYER                                       │
│  command_processor  motion_manager  safety_manager  (future)   │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 3 — COMMUNICATION LAYER                                 │
│  network_manager  bluetooth_bridge  telemetry                  │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 2 — GRETA OS KERNEL                                     │
│  scheduler  event_bus  health_manager  system_monitor          │
│  state_manager  greta_core                                     │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 1 — HARDWARE ABSTRACTION LAYER (HAL)                    │
│  gpio_hal  uart_hal  i2c_hal  spi_hal  pwm_hal  adc_hal        │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 0 — PLATFORM                                            │
│  Arduino / ESP-IDF  FreeRTOS  ESP32 silicon                    │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Module Responsibilities

### Layer 0 — Platform
| Module | Responsibility |
|---|---|
| Arduino / ESP-IDF | Peripheral drivers, WiFi stack, BLE stack |
| FreeRTOS | Task scheduler (used for idle/yield only in Phase 1) |

### Layer 1 — HAL
| Module | Responsibility |
|---|---|
| `gpio_hal` | Digital I/O abstraction |
| `pwm_hal` | Motor and servo PWM generation |
| `i2c_hal` | Sensor bus abstraction |
| `uart_hal` | Serial port abstraction |

No module above Layer 1 may call `digitalWrite`, `analogWrite`, or any Arduino peripheral function directly.

### Layer 2 — Greta OS Kernel
| Module | Responsibility |
|---|---|
| `greta_core` | Boot ID, robot identity, uptime tracker, capability flags |
| `scheduler` | Cooperative task dispatch, interval management, fault detection |
| `event_bus` | Decoupled inter-module publish/subscribe |
| `health_manager` | Heartbeat registry, stall detection, fault reporting |
| `state_manager` | FSM: `CONNECTING → READY → SAFE → PANIC` |
| `system_monitor` | Heap, stack, WiFi RSSI, CPU load observation |

### Layer 3 — Communication
| Module | Responsibility |
|---|---|
| `network_manager` | WiFi connection, WebSocket or HTTP to dashboard |
| `bluetooth_bridge` | BLE or Classic BT link to secondary controller |
| `telemetry` | Outbound status frames to dashboard |

### Layer 4 — Control
| Module | Responsibility |
|---|---|
| `command_processor` | Validates, gates, and dispatches inbound commands |
| `motion_manager` | Translates commands to actuator outputs via HAL *(future)* |
| `safety_manager` | Hardware e-stop, tilt sensing, current limits *(future)* |

### Layer 5 — Brain
| Module | Responsibility |
|---|---|
| `vision_manager` | Camera pipeline, object detection feed *(future)* |
| `brain_manager` | Autonomy, planning, ML inference *(future)* |

---

## 3. Execution Model

Greta OS uses a **cooperative non-preemptive scheduling model** on Core 1. No task may block, sleep, or call `delay()`. Every module exposes an `_update()` function that completes within its timing budget and returns.

```
loop() iteration:
  │
  ├─ [PANIC check] ─── if STATE_PANIC → panic_safe_loop(); return
  │
  ├─ scheduler_run_due_tasks()
  │     ├─ network_update()        every 10 ms
  │     ├─ bluetooth_update()      every 10 ms
  │     ├─ command_update()        every 20 ms
  │     ├─ state_update()          every 50 ms
  │     ├─ telemetry_update()      every 100 ms
  │     └─ system_monitor_update() every 100 ms
  │
  ├─ bluetooth_ack_drain()  [inline — not schedulable]
  │
  └─ link_recovery_update() [inline — runs every tick]
```

The scheduler does not preempt a running task. If a task overruns its budget, the overrun is measured and logged. Repeated overruns escalate to `STATE_PANIC`.

---

## 4. Task Timing Model

| Group | Interval | Tasks |
|---|---|---|
| CRITICAL | 10 ms | network ingress, bluetooth update |
| CONTROL | 20 ms | command watchdogs, safety check *(future)* |
| STATE | 50 ms | FSM transitions, motion *(future)* |
| TELEMETRY | 100 ms | outbound data, system monitor |
| SLOW | 500 ms | OTA, vision tick, brain tick *(future)* |

Timing rationale: 10 ms is the minimum meaningful human-control latency over WiFi. All safety-critical ingress runs at this rate. Control decisions run at 20 ms to ensure ingress has been processed first. Telemetry and monitoring run at 100 ms to avoid saturating the WiFi uplink.

---

## 5. Communication Flow

```
Dashboard (browser / app)
        │
        │  WebSocket / HTTP (WiFi)
        ▼
  network_manager
        │  on_dashboard_command() callback
        ▼
  command_processor ──► state_manager (gate check)
        │                      │
        │                      ▼
        │               motion_manager (future)
        │
        ▼  (ACK path)
  bluetooth_bridge ──► command_receive_ack()
        │
        ▼
  telemetry ──────────────────────────────────► Dashboard
  system_monitor ─────────────────────────────► Dashboard
```

Modules do not call each other directly across layers. The event bus carries notifications. The command callback in `main.cpp` is the only permitted cross-layer adapter.

---

## 6. Design Philosophy

**Safety first.** The system defaults to the most restrictive safe state on any uncertainty. Panic is preferred over undefined behaviour.

**Modular independence.** No module `#include`s another module at the same layer. All cross-module communication is via callbacks registered at boot or via the event bus.

**Non-blocking everywhere.** `delay()` is forbidden after the USB CDC settle at boot. Every `_update()` function must return within its timing budget.

**Hardware abstraction.** Direct register or Arduino peripheral calls are confined to HAL modules. Upper layers use HAL APIs only.

**AI ready.** Task slots, timing budgets, and capability flags are pre-allocated for vision, planning, and inference modules. Adding a brain module requires no architectural change.

**Observable.** Every module registers a health heartbeat. System monitor reports heap, RSSI, and CPU load on every telemetry cycle.

---

## 7. Future Expansion

| Phase | Addition | Architectural impact |
|---|---|---|
| 2 | `motion_manager`, `safety_manager` | Register tasks at 20 ms and 50 ms slots |
| 3 | `vision_manager` | Register task at 500 ms; publishes to event bus |
| 4 | `brain_manager` | Subscribes to vision events; issues commands via command bus |
| 5 | Dual-core split | Pin scheduler to Core 1; pin vision/brain to Core 0 via RTOS tasks |
| 5 | `ota_manager` | Registers at 500 ms; safe only in `STATE_READY` |

---

## 8. Control Architecture Update

The current firmware control path now includes three explicit coordination modules:

| Module | Responsibility |
|---|---|
| `mode_manager` | Owns `IDLE`, `MANUAL`, `AUTONOMOUS`, `SAFE`, and `ERROR` operating modes |
| `task_manager` | Tracks active task ownership, interruptions, and deterministic task priority |
| `behavior_manager` | Resolves command admission, safety override, link recovery, and mode/task arbitration |

The runtime hierarchy is:

`scheduler -> state_manager -> behavior_manager -> command_processor`

Safety response flow is:

`health_manager` or link/watchdog fault -> `behavior_manager` forces `SAFE` -> `state_manager` records the transition reason -> scheduler continues only the cooperative runtime needed for recovery and telemetry.
