# GRETA OS — Repository Structure

**Convention:** One responsibility per directory. Headers are co-located with their source files. Platform-specific code lives in `hal/` only.

---

## Top-Level Structure

```
greta-v2/
├── src/                    # All firmware source code
│   ├── main.cpp            # Kernel entry point — boot and runtime loop only
│   ├── config.h            # All compile-time constants and capability flags
│   │
│   ├── core/               # Greta OS kernel (Layer 2)
│   ├── hal/                # Hardware abstraction layer (Layer 1)
│   ├── comms/              # Communication modules (Layer 3)
│   ├── control/            # Control modules (Layer 4)
│   └── brain/              # AI and autonomy modules (Layer 5) [future]
│
├── docs/                   # Engineering documentation
├── tests/                  # Unit and integration tests
├── tools/                  # Build scripts, flash tools, dashboard
├── assets/                 # Diagrams, images, hardware references
└── platformio.ini          # Build configuration
```

---

## Source Directories

### `src/` — Root

| File | Purpose |
|---|---|
| `main.cpp` | Boot sequence, runtime loop, panic guard, link recovery. Nothing else. |
| `config.h` | All named constants: timing intervals, heap thresholds, capability flags, version string. Single source of truth for configuration. |

Rule: `main.cpp` may only grow to accommodate new module init calls and task registrations. Logic belongs in modules.

---

### `src/core/` — Greta OS Kernel

```
core/
├── greta_core.h / .cpp       # System identity, boot ID, uptime, capability flags
├── scheduler.h / .cpp        # Cooperative task scheduler
├── event_bus.h / .cpp        # Publish / subscribe messaging
├── health_manager.h / .cpp   # Heartbeat registry and fault detection
├── state_manager.h / .cpp    # Robot FSM
└── system_monitor.h / .cpp   # Heap, RSSI, CPU load observer
```

Modules in `core/` may only depend on each other and on `config.h`. No `core/` module may include anything from `comms/`, `control/`, `hal/`, or `brain/`.

---

### `src/hal/` — Hardware Abstraction Layer

```
hal/
├── gpio_hal.h / .cpp         # Digital I/O
├── pwm_hal.h / .cpp          # PWM output (motors, servos)
├── i2c_hal.h / .cpp          # I2C sensor bus
├── uart_hal.h / .cpp         # UART serial ports
├── adc_hal.h / .cpp          # Analog input
└── spi_hal.h / .cpp          # SPI bus [future]
```

HAL modules are the only modules permitted to call `digitalWrite()`, `analogWrite()`, `Wire.*`, `SPI.*`, or equivalent Arduino/ESP-IDF peripheral APIs.

HAL modules have no knowledge of Greta OS modules. They do not call `state_get()`, `event_publish()`, or `health_kick()`. They are pure hardware wrappers.

---

### `src/comms/` — Communication Modules

```
comms/
├── network_manager.h / .cpp  # WiFi, WebSocket/HTTP, command callback
├── bluetooth_bridge.h / .cpp # BT/BLE link, ACK channel
└── telemetry.h / .cpp        # Outbound status and alert frames
```

`comms/` modules may depend on `core/` modules only. They must not include anything from `control/` or `brain/`.

---

### `src/control/` — Control Modules

```
control/
├── command_processor.h / .cpp  # Command validation, gating, dispatch
├── motion_manager.h / .cpp     # Drive command to HAL translation [future]
└── safety_manager.h / .cpp     # E-stop, tilt, current limits [future]
```

`control/` modules may depend on `core/` and `hal/`. They must not depend on `comms/` directly — they receive commands via callbacks registered in `main.cpp`.

---

### `src/brain/` — AI and Autonomy Modules *(future)*

```
brain/
├── vision_manager.h / .cpp    # Camera pipeline, object detection
└── brain_manager.h / .cpp     # Mission planning, autonomy
```

`brain/` modules may depend on `core/` only. They publish and subscribe to the event bus. They do not import from `comms/`, `control/`, or `hal/`.

---

## Documentation Directory

```
docs/
├── GRETA_OS_ARCHITECTURE.md   # System layers, execution model, design philosophy
├── GRETA_MODULE_MAP.md        # Module ownership, allowed and forbidden dependencies
├── SAFETY_MODEL.md            # Panic, safe state, health monitoring, fault escalation
├── TASK_TIMING.md             # Scheduler philosophy, priority groups, timing budgets
├── BOOT_SEQUENCE.md           # Step-by-step init order, boot diagram
├── NETWORK_PROTOCOL.md        # Command format, telemetry format, BT fallback
├── HARDWARE_SETUP.md          # Wiring, power architecture, testing sequence
├── FUTURE_ROADMAP.md          # Phase 1–5 development plan
├── DESIGN_RULES.md            # Mandatory engineering rules
└── GRETA_REPOSITORY_STRUCTURE.md  # This file
```

Documentation is updated before implementation when a new phase begins. A PR that adds a new module without updating the relevant documentation is blocked.

---

## Tests Directory

```
tests/
├── unit/
│   ├── test_scheduler.cpp     # Scheduler interval accuracy, overrun detection
│   ├── test_health_manager.cpp# Heartbeat registration, fault detection
│   ├── test_state_manager.cpp # FSM transition correctness
│   └── test_command_processor.cpp  # Validation rules, state gating
│
└── integration/
    ├── test_boot_sequence.cpp # Full boot to STATE_READY under sim
    └── test_panic_path.cpp    # Health fault → STATE_PANIC path
```

Unit tests run on host (native) using a test framework (Unity or Google Test). They do not require hardware. Integration tests run on hardware via PlatformIO test runner.

---

## Tools Directory

```
tools/
├── dashboard/                 # Web dashboard source (HTML/JS)
│   ├── index.html
│   ├── ws_client.js           # WebSocket command and telemetry client
│   └── ui.js                  # Dashboard UI logic
│
├── flash.sh                   # One-command flash script (wraps platformio run -t upload)
├── monitor.sh                 # Serial monitor with timestamp prefix
└── i2c_scan/                  # Standalone I2C bus scanner sketch for hardware validation
```

---

## Assets Directory

```
assets/
├── hardware/
│   ├── wiring_diagram.png     # Full wiring reference image
│   ├── power_architecture.png # Power rail diagram
│   └── pin_allocation.csv     # Machine-readable pin table
│
└── diagrams/
    ├── fsm_states.png         # State machine diagram
    └── boot_sequence.png      # Boot flow diagram
```

---

## `platformio.ini` Configuration Notes

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
build_flags =
    -Wall
    -Wextra
    -Werror
    -DGRETA_VERSION='"2.0.0"'
    -DGRETA_BUILD_DATE='"__DATE__"'
    -DMAX_TASKS=12
monitor_speed = 115200
test_framework = unity
```

`-Werror` enforces DR-15. `MAX_TASKS` is set here so it can be overridden per build environment without touching source.

---

## File Naming Conventions

| Type | Convention | Example |
|---|---|---|
| Module source | `snake_case.cpp` | `health_manager.cpp` |
| Module header | `snake_case.h` | `health_manager.h` |
| Test file | `test_<module>.cpp` | `test_health_manager.cpp` |
| Documentation | `UPPER_SNAKE_CASE.md` | `SAFETY_MODEL.md` |
| Config constants | `GRETA_<SCOPE>_<NAME>` | `GRETA_MIN_HEAP_BYTES` |
| Public functions | `<module>_<verb>()` | `health_manager_fault()` |
| Internal functions | `static` in `.cpp`, any name | `static void drain_queue()` |

---

## Current Firmware Layout

The live ESP32 firmware remains under `esp32-brain/include` and `esp32-brain/src`. For the current hardware-integration phase, responsibilities are grouped logically rather than by a disruptive file move:

| Logical group | Current modules |
|---|---|
| `core` | `scheduler`, `event_bus`, `state_manager`, `health_manager` |
| `behavior` | `mode_manager`, `task_manager`, `behavior_manager` |
| `modules` | `network_manager`, `bluetooth_bridge`, `telemetry`, `command_processor` |
| `diagnostics` | `telemetry`, `health_manager`, safety and architecture docs |
| `system` | `main.cpp`, `config.h`, boot/runtime integration |

This keeps the repository stable while making the intended `/core`, `/modules`, `/system`, `/behavior`, and `/diagnostics` boundaries explicit for future growth.
