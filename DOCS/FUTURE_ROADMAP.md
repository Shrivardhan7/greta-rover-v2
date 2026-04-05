# GRETA OS — Development Roadmap

**Platform:** Greta V2  
**Methodology:** Each phase delivers a working, testable system before the next phase begins. No phase is marked complete until hardware integration tests pass.

---

## Phase 1 — Stable Robot Platform *(current)*

**Goal:** A teleoperated robot with a reliable OS kernel, health monitoring, and dashboard control.

### Deliverables

- Greta OS cooperative kernel: scheduler, event bus, health manager, state manager
- WiFi dashboard command and telemetry pipeline
- Bluetooth ACK channel
- Boot diagnostics and system monitor
- `STATE_PANIC` safety escalation
- Link recovery FSM
- All engineering documentation (this repository)

### Success criteria

- Robot connects to dashboard within 10 seconds of boot
- Dashboard commands reach `command_processor` within 20 ms of transmission
- Telemetry frames delivered to dashboard at 10 Hz
- `STATE_PANIC` entered and motors disabled within 200 ms of health fault detection
- System runs for 30 minutes without scheduler overrun or heap warning

### Known limitations

- No physical motion — `motion_manager` not yet implemented
- No sensor input
- No autonomy

---

## Phase 2 — Motion and Sensors

**Goal:** Physical robot motion under dashboard control with basic sensor feedback.

### Deliverables

- `motion_manager` — drive command to HAL PWM translation
- `safety_manager` — hardware e-stop, tilt detection via IMU, current monitoring
- `gpio_hal`, `pwm_hal`, `i2c_hal` — hardware abstraction modules
- IMU integration (MPU-6050 or equivalent)
- Distance sensor integration (VL53L0X or equivalent)
- Motor current monitoring (INA219 or equivalent)
- Extended telemetry: speed, heading, distance, current draw
- Extended command set: `MOVE_*`, `ROTATE_*`, `SPEED_SET:`
- Hardware watchdog integration (`esp_task_wdt`)
- Physical chassis integration and calibration

### Scheduler additions

| Task | Interval | Notes |
|---|---|---|
| `safety_update` | 20 ms | Must run before `motion_update` |
| `motion_update` | 50 ms | Reads safety gate before issuing PWM |
| `sensor_update` | 50 ms | Reads IMU and distance sensor |

### Success criteria

- Robot moves reliably under dashboard control
- IMU tilt beyond threshold triggers `STATE_SAFE`
- Motor overcurrent triggers `STATE_SAFE`
- e-stop button triggers `STATE_PANIC`
- All Phase 1 success criteria still met

---

## Phase 3 — AI and Vision

**Goal:** Robot perceives its environment and reports structured sensor data to the dashboard.

### Deliverables

- `vision_manager` — camera pipeline, object detection, frame publish to event bus
- Camera module integration (OV2640 or ESP32-CAM)
- Basic object detection (TensorFlow Lite Micro or YOLO-nano on ESP32)
- Vision telemetry frame added to dashboard protocol
- Event bus vision events: `EVENT_OBJECT_DETECTED`, `EVENT_OBSTACLE`
- Binary telemetry framing (MessagePack) for bandwidth efficiency

### Scheduler additions

| Task | Interval | Notes |
|---|---|---|
| `vision_update` | 500 ms | Runs on Core 0 (RTOS task) or at 2 Hz on Core 1 |

### Architectural change

Phase 3 introduces the first dual-core split:
- Vision inference moves to Core 0 via a dedicated RTOS task.
- Result is published to event bus and consumed by `brain_manager` on Core 1.
- Scheduler on Core 1 is not affected.

### Success criteria

- Camera frame captured and processed at minimum 1 Hz
- Object detection results visible on dashboard within 1 second
- Vision processing does not cause scheduler overruns on Core 1
- All Phase 2 success criteria still met

---

## Phase 4 — Autonomy

**Goal:** Robot executes simple autonomous missions without continuous dashboard input.

### Deliverables

- `brain_manager` — mission state machine, goal planning, obstacle response
- Mission command set: `MISSION_START:<id>`, `MISSION_ABORT:`, `WAYPOINT_SET:`
- Autonomous obstacle avoidance using distance sensor and vision data
- Mission status reporting in telemetry
- Manual override always available from dashboard (interrupts any mission)

### Architectural change

`brain_manager` subscribes to:
- `EVENT_OBJECT_DETECTED` from `vision_manager`
- `EVENT_OBSTACLE` from `sensor_update`

`brain_manager` issues commands to `command_processor` via event bus — it does not write to motion directly.

### Success criteria

- Robot navigates a defined waypoint route without human input
- Obstacle detection triggers avoidance manoeuvre within 500 ms
- Dashboard manual override interrupts autonomous mission within 100 ms
- Robot returns to `STATE_SAFE` and stops on mission abort command
- All Phase 3 success criteria still met

---

## Phase 5 — Advanced Robotics Features

**Goal:** Production-quality autonomous platform with OTA updates, multi-robot coordination, and extended AI capabilities.

### Planned deliverables

- `ota_manager` — over-the-air firmware updates via dashboard
- Authenticated command protocol (HMAC on command frames)
- Multi-robot coordination via MQTT broker
- Extended ML models — scene understanding, person tracking
- Battery management and low-power sleep modes
- ROS 2 bridge (publish sensor data and receive commands via ROS topics)
- CAN bus integration for industrial sensor modules

### Architectural notes

- OTA updates are only permitted in `STATE_READY` with all health monitors green.
- Multi-robot coordination uses the event bus pattern extended over MQTT — same publish/subscribe model as local event bus.
- ROS 2 bridge runs as a separate RTOS task on Core 0, publishing to event bus on Core 1.

---

## Cross-Phase Principles

These principles apply at every phase and are not relaxed as complexity increases:

1. Each phase delivers hardware-tested functionality before the next begins.
2. Safety model is never weakened — new modules add safety registrations, they do not bypass existing ones.
3. Scheduler budget is reviewed at the start of each phase. New tasks must fit within the timing model.
4. Documentation is updated at the start of each phase, before implementation.
5. All new modules follow `DESIGN_RULES.md` without exception.
