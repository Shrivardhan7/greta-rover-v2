# Greta V2 — Engineering Review: Suggested Improvements

**Reviewer:** Senior Embedded Systems / Robotics Architecture Review
**Scope:** Structure suggestions, code quality notes, and future considerations
**Note:** These are suggestions only. Nothing in this document has been automatically implemented.

---

## 1. Structure Suggestions (Do Not Implement Automatically)

These suggestions improve repository clarity. Implement them manually when ready — there is no rush and some may not apply depending on the current state of the codebase.

### 1.1 Add a top-level `config.h` if not already present

All compile-time constants (`GRETA_MIN_HEAP_BYTES`, `TASK_INTERVAL_CRITICAL_MS`, `BT_SILENCE_TIMEOUT_MS`, etc.) should live in one file. This avoids magic numbers scattered across modules and makes tuning easy.

```c
// src/config.h
// Greta Rover OS — Configuration Constants
// Copyright (c) 2026 Shrivardhan Jadhav
// Licensed under Apache License 2.0

#pragma once

// Firmware identity
#define GRETA_VERSION_STRING    "2.0.0"
#define GRETA_ROBOT_NAME        "Greta"

// Scheduler
#define MAX_TASKS               12
#define TASK_INTERVAL_CRITICAL_MS   10
#define TASK_INTERVAL_CONTROL_MS    20
#define TASK_INTERVAL_STATE_MS      50
#define TASK_INTERVAL_TELEMETRY_MS  100

// Health monitoring
#define HEALTH_FAULT_THRESHOLD      3    // consecutive missed kicks before fault
#define HEALTH_KICK_MULTIPLIER      3    // kick timeout = interval × this value

// Safety thresholds
#define GRETA_MIN_HEAP_BYTES        20000
#define BT_SILENCE_TIMEOUT_MS       6000
#define ACK_TIMEOUT_MS              1500
#define CMD_TIMEOUT_MS              2000
#define PING_TIMEOUT_MS             12000

// Network
#define WS_PORT                     81
#define TELEMETRY_INTERVAL_MS       100

// Serial
#define SERIAL_BAUD_RATE            115200
#define SERIAL_SETTLE_MS            100    // USB CDC settle — only delay() permitted
```

### 1.2 Add a `CHANGELOG.md`

A simple per-date log of what changed. This is already partially documented in `greta-master-log.md` but a `CHANGELOG.md` in the root gives contributors a single place to check history.

### 1.3 Add `tests/readme.md`

A short note explaining how to run unit tests (`pio test -e native`) and integration tests (`pio test -e esp32`). This is not obvious to a first-time contributor.

### 1.4 Move `greta-master-log.md` to `docs/`

The master development log belongs in `docs/` alongside the other engineering documents. It is not a root-level file.

### 1.5 Rename `greta-master-log.md` entries to use consistent date format

The log uses `03 april 2026` format. Engineering logs conventionally use `YYYY-MM-DD` for sorting and tooling compatibility.

---

## 2. License Headers — Where to Add Them

Add the following minimal header to these file types. Do not add headers to Markdown docs, HTML, CSS, or config files unless they contain significant original logic.

### Header format

```c
/*
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 */
```

### Files that need headers

| File type | Examples | Add header? |
|---|---|---|
| C++ source files (`.cpp`) | `scheduler.cpp`, `health_manager.cpp`, `network_manager.cpp` | Yes |
| C++ header files (`.h`) | `scheduler.h`, `health_manager.h`, `config.h` | Yes |
| Arduino sketches (`.ino`) | Any Arduino firmware files | Yes |
| JavaScript files (`.js`) | `ws_client.js`, `ui.js` | Yes |
| Markdown docs (`.md`) | All docs in `docs/` | No — footer attribution is sufficient |
| HTML (`.html`) | `index.html` | No |
| CSS (`.css`) | `style.css` | No |
| Config files (`.ini`, `.csv`) | `platformio.ini` | Comment header only (already done) |

---

## 3. Code Quality Notes

These apply to future code additions. Do not refactor existing working code just to apply these.

### 3.1 Consistent module comment block

At the top of each `.cpp` file (after the license header), add a brief module description:

```c
/*
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 */

/*
 * scheduler.cpp
 *
 * Cooperative task scheduler for Greta OS.
 * Manages task registration, interval tracking, and dispatch.
 * All tasks share a single execution context on Core 1.
 *
 * See: docs/TASK_TIMING.md for timing rationale
 * See: docs/DESIGN_RULES.md DR-01, DR-08, DR-11
 */
```

### 3.2 Health kick placement

Health kicks must be the FIRST line of every `_update()` function, before any conditional logic. If the module is in an error path and skips work, the kick still occurs — which is the correct behaviour. Heartbeat = "I am running", not "I completed work successfully".

```c
// Correct
void network_update(void) {
    health_kick("network");     // first — always
    // ... rest of logic
}

// Wrong — kick is skipped if early return fires
void network_update(void) {
    if (!network_wifi_ok()) return;
    health_kick("network");
}
```

### 3.3 State gate pattern in command processor

The state gate check in `command_processor` should use a named constant comparison, not a raw integer:

```c
// Preferred
if (state_get() != STATE_READY) {
    return CMD_ERR_STATE_GATE;
}

// Not preferred (magic comparison)
if (state_get() != 2) { ... }
```

### 3.4 Event bus handler length

Event handlers registered via `event_subscribe()` must complete in microseconds, not milliseconds. Handlers must not call `event_publish()`. Add a comment above every handler registration to document this constraint:

```c
// Handler contract: must be non-blocking, under 100µs, no re-entrant publish
event_subscribe(EVENT_PANIC, on_panic_event);
```

### 3.5 Serial output format

All Serial output should use module prefix tags for easy log filtering:

```c
Serial.println(F("[NET] WiFi connected"));
Serial.println(F("[SCHED] task overrun: network_update"));
Serial.println(F("[HEALTH] fault: telemetry missed 3 kicks"));
Serial.println(F("[MAIN] boot complete"));
```

Use `F()` macro for all string literals in Serial calls to keep strings in flash and save RAM.

---

## 4. Safety Notes to Add to Code

These are comments to add near safety-critical sections. They do not change any logic.

### 4.1 Above the only permitted `delay()` call

```c
// ONLY permitted delay() in the firmware — USB CDC settle (DR-01)
// All subsequent waiting uses millis() comparisons.
delay(SERIAL_SETTLE_MS);
```

### 4.2 Above `state_set(STATE_PANIC, ...)`

```c
// STATE_PANIC has no software exit.
// Recovery requires a physical power cycle or hardware watchdog reset.
// Do not add a software escape from this state.
state_set(STATE_PANIC, "health fault");
```

### 4.3 Above `network_init()` in boot sequence

```c
// network_init() fires WiFi.begin() (non-blocking).
// The command callback may fire almost immediately after this line.
// All modules that the callback depends on MUST be initialized above this call.
network_init();
```

---

## 5. Documentation Consistency

### 5.1 All docs should reference the license

Add a footer to each document in `docs/`:

```
---
*Copyright (c) 2026 Shrivardhan Jadhav — Licensed under Apache License 2.0*
```

### 5.2 readme should link to all key docs

The `readme.md` already does this partially. Ensure links to `safety-model.md`, `design-rules.md`, and `boot-sequence.md` are present so a new reader can navigate quickly.

### 5.3 safety.md in root

A `safety.md` in the repository root keeps safety information discoverable without reading the full documentation set. This file has been created as part of this review.

---

## 6. Realistic Next Steps (Phase 1 Completion)

In priority order:

1. Flash Greta OS to hardware and confirm boot diagnostics in serial monitor.
2. Confirm `STATE_READY` transition with WiFi + BT both connected.
3. Confirm telemetry reaching dashboard at 100 ms cadence.
4. Test `STATE_PANIC` path: kill a health kick intentionally and confirm FSM escalation.
5. Run 30-minute stability test — check heap, scheduler overruns, health status.
6. Only after all Phase 1 success criteria pass: begin Phase 2 planning.

---

*Greta V2 Engineering Review — April 2026*

