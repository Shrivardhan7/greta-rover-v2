# Greta V2 — Architecture

**Version:** 2.0.0
**Status:** Production baseline

---

## 1. Design Philosophy

Greta is not a demonstration project. It is designed as a foundation that can evolve into an autonomous AI rover without requiring architectural rewrites at each capability milestone.

Three principles govern every design decision:

**Reliability over features.** A feature that compromises system stability is not included. Every communication channel has an independent watchdog. Every state transition is guarded. The rover stops by default; movement is the exception, not the default.

**Layered safety, not centralised safety.** Safety is not a single module that can fail. It is implemented independently at the Arduino hardware layer, the ESP32 firmware layer, and the dashboard application layer. The failure of any one layer leaves the other two in force.

**Module independence.** No module imports another module's internal state. Modules communicate only through defined function interfaces. This makes each module testable in isolation and replaceable without cascade changes.

---

## 2. System Topology

```
┌─────────────────────────────────────────┐
│            Web Dashboard                │
│  index.html / style.css / app.js        │
│  Any browser — LAN or GitHub Pages      │
└─────────────────┬───────────────────────┘
                  │  WebSocket  ws://greta.local:81/ws
                  │  WiFi 802.11
┌─────────────────▼───────────────────────┐
│           ESP32-S3  (Brain)             │
│  • WiFi client + WebSocket server       │
│  • 6-state safety FSM                   │
│  • Command validation & routing         │
│  • ACK correlation + latency tracking   │
│  • Telemetry broadcast (1 Hz)           │
│  • PING/PONG heartbeat watchdog         │
│  • Bluetooth UART bridge (HC-05)        │
└─────────────────┬───────────────────────┘
                  │  Bluetooth Serial (HC-05) / UART2
                  │  9600 baud, newline-terminated ASCII
┌─────────────────▼───────────────────────┐
│       Arduino Uno  (Motion Executor)    │
│  • L298N motor driver                   │
│  • HC-SR04 ultrasonic obstacle stop     │
│  • Independent command timeout watchdog │
│  • ACK protocol                         │
└──────────┬──────────────────────────────┘
           │
    ┌──────┴──────┐
    │             │
  Motors     HC-SR04
  (L298N)  Ultrasonic
```

### Architectural constraint

The dashboard never communicates directly with the Arduino. All commands pass through the ESP32. This constraint exists because:

1. The ESP32 owns the safety state machine. Commands must be validated against system state before reaching the motors.
2. The Arduino's Bluetooth serial port is hardware UART (pins 0/1). It cannot simultaneously serve both the dashboard and the ESP32.
3. Future capabilities (vision, voice, autonomous navigation) will originate in the ESP32. The routing topology must already be correct for those to plug in.

---

## 3. ESP32 Module Map

```
main.cpp
  │
  ├── network_manager     WiFi watchdog, multi-SSID fallback, mDNS,
  │                       WebSocket server, PING/PONG heartbeat
  │                       → on text frame: CommandCallback → command_receive()
  │
  ├── bluetooth_bridge    UART2 RX/TX, static line buffer, silence watchdog
  │                       → on complete line: main.cpp calls command_receive_ack()
  │
  ├── command_processor   Whitelist, state gate, ACK correlation,
  │                       cmd timeout watchdog, ACK timeout watchdog,
  │                       round-trip latency measurement
  │                       → writes via bluetooth_send()
  │                       → reads/writes state via state_manager
  │                       → broadcasts events via network_broadcast()
  │
  ├── state_manager       6-state FSM with transition guard table
  │                       Stores last transition reason in static buffer
  │
  └── telemetry           1 Hz JSON snapshot of all module states
                          Writes into static char buffer — no heap
```

### Module interface contract

Every module exposes exactly:

```c
void module_init();    // Called once in setup()
void module_update();  // Called every loop() — must be non-blocking
```

No `delay()` calls exist anywhere in the main execution path. All timing uses `millis()` delta comparison.

---

## 4. State Machine

### States

| State        | Meaning                                         | Motors |
|--------------|-------------------------------------------------|--------|
| `BOOT`       | Firmware initialising                           | Off    |
| `CONNECTING` | Waiting for WiFi + Bluetooth links              | Off    |
| `READY`      | All links healthy, awaiting commands            | Off    |
| `MOVING`     | Motion command active, ACK pending or confirmed | On     |
| `SAFE`       | Safety condition — all movement blocked         | Off    |
| `ERROR`      | Unrecoverable fault — requires reset            | Off    |

### Transition guard table

Transitions not listed are **blocked** at the firmware level and logged.

| From         | To           | Trigger                                 |
|--------------|--------------|-----------------------------------------|
| `BOOT`       | `CONNECTING` | `setup()` completes                     |
| `CONNECTING` | `READY`      | WiFi up AND first BT byte received      |
| `CONNECTING` | `ERROR`      | Unrecoverable hardware failure          |
| `READY`      | `MOVING`     | Valid move command received             |
| `READY`      | `SAFE`       | Link loss or obstacle event             |
| `MOVING`     | `READY`      | `ACK STOP` received                     |
| `MOVING`     | `SAFE`       | Link loss, obstacle, or ACK timeout     |
| `SAFE`       | `READY`      | Both WiFi and BT links confirmed healthy|
| `*`          | `ERROR`      | Unrecoverable fault                     |

### STOP priority

`STOP` and `ESTOP` bypass the state gate entirely. They are forwarded to the Arduino regardless of current state — including during `STATE_SAFE` and `STATE_ERROR`. This is the only command that bypasses the admission control path.

### Automatic SAFE recovery

`main.cpp` checks `state_get() == STATE_SAFE && network_wifi_ok() && bluetooth_connected()` every loop iteration. Recovery is automatic — no user action is required. This design choice reflects operational reality: if Greta drops WiFi briefly while crossing a room, it should recover and resume control transparently.

---

## 5. Communication Channels

### Channel 1: WebSocket (Dashboard ↔ ESP32)

| Property  | Value                        |
|-----------|------------------------------|
| Protocol  | WebSocket RFC 6455           |
| Port      | 81                           |
| Path      | `/ws`                        |
| URL       | `ws://greta.local:81/ws`     |
| Encoding  | UTF-8 text frames            |

**Dashboard → ESP32 commands:**

| Frame    | Action                        |
|----------|-------------------------------|
| `MOVE F` | Move forward                  |
| `MOVE B` | Move backward                 |
| `MOVE L` | Turn left                     |
| `MOVE R` | Turn right                    |
| `STOP`   | Stop (state-gated)            |
| `ESTOP`  | Emergency stop (always passes)|
| `PONG`   | Heartbeat reply               |

**ESP32 → Dashboard frames:**

| Frame                        | Trigger              |
|------------------------------|----------------------|
| Telemetry JSON (see §6)      | Every 1000 ms        |
| `{"event":"OBSTACLE"}`       | Obstacle detected    |
| `{"event":"PING"}`           | Heartbeat probe      |

### Channel 2: Bluetooth Serial (ESP32 ↔ Arduino)

| Property | Value                          |
|----------|--------------------------------|
| Hardware | HC-05 on ESP32 UART2           |
| Pins     | TX=GPIO17, RX=GPIO18           |
| Baud     | 9600                           |
| Format   | Newline-terminated ASCII       |

**ACK strings (Arduino → ESP32):**

| String     | Meaning                      |
|------------|------------------------------|
| `ACK BOOT` | Arduino boot complete        |
| `ACK F`    | `MOVE F` executed            |
| `ACK B`    | `MOVE B` executed            |
| `ACK L`    | `MOVE L` executed            |
| `ACK R`    | `MOVE R` executed            |
| `ACK STOP` | `STOP` executed              |
| `OBSTACLE` | Obstacle < 20 cm detected    |

`ACK BOOT` is new in V2. It replaces the V1 behaviour where the Arduino sent `ACK STOP` at boot — a semantically incorrect signal that caused confusion in logs.

---

## 6. Telemetry Schema

Broadcast every 1000 ms to all connected WebSocket clients.

```json
{
  "state":     "READY",
  "wifi":      "OK",
  "bt":        "OK",
  "uptime":    145,
  "lastCmd":   "MOVE F",
  "rssi":      -58,
  "latencyMs": 28
}
```

| Field       | Type    | Always | Notes                                      |
|-------------|---------|--------|--------------------------------------------|
| `state`     | string  | Yes    | FSM state name                             |
| `wifi`      | string  | Yes    | `"OK"` or `"LOST"`                         |
| `bt`        | string  | Yes    | `"OK"` or `"LOST"`                         |
| `uptime`    | integer | Yes    | Seconds since boot                         |
| `lastCmd`   | string  | Yes    | Last command forwarded to Arduino          |
| `rssi`      | integer | No     | WiFi RSSI dBm — present if `FEATURE_TELEMETRY_RSSI` defined |
| `latencyMs` | integer | No     | Last cmd→ACK round-trip ms — present if `FEATURE_TELEMETRY_LATENCY` defined |

Optional fields are controlled by `#ifdef` in `config.h`. Disabling them reduces the JSON payload and saves `serializeJson()` processing time on each tick.

---

## 7. Safety Architecture

Safety is implemented redundantly. Each layer operates independently.

### Layer 1 — Arduino hardware watchdog

The Arduino enforces:
- **Command timeout:** If no command is received for 2000 ms, `motors_stop()` is called silently.
- **Obstacle stop:** `pulseIn()` on HC-SR04 runs every loop iteration. Distance < 20 cm → `motors_stop()` → `Serial.println("OBSTACLE")`.

The Arduino does not know about WiFi, WebSocket, or the ESP32 state machine. It stops when it has no commands, and it stops when it detects an obstacle. These are unconditional.

### Layer 2 — ESP32 firmware watchdog

The ESP32 enforces:
- **Bluetooth silence timeout:** No bytes from Arduino for 6000 ms → `STATE_SAFE`.
- **ACK timeout:** No ACK for sent command within 1500 ms → `STOP` sent → `STATE_READY`.
- **Command timeout:** No movement command received while in `STATE_MOVING` for 2000 ms → `STOP` sent.
- **WiFi loss:** `WiFi.status() != WL_CONNECTED` → `STATE_SAFE`.
- **WebSocket client disconnect:** → `STATE_SAFE`.
- **Heartbeat PONG timeout:** No PONG from dashboard for 12000 ms → `STATE_SAFE`.

### Layer 3 — Dashboard emergency stop

- **Tab hidden** (`visibilitychange` event): `ESTOP` sent immediately.
- **Tab close / navigation** (`beforeunload`): `ESTOP` sent.
- **Manual STOP button:** Always sends `STOP`.
- **`E` key:** Sends `ESTOP`.

### Safety event flow (obstacle example)

```
Arduino: HC-SR04 reads 14 cm (< 20 cm threshold)
Arduino: motors_stop()
Arduino: Serial.println("OBSTACLE")

ESP32 bluetooth_bridge: receives "OBSTACLE"
main.cpp: command_receive_ack("OBSTACLE")
command_processor: state_set(STATE_SAFE, "obstacle detected")
command_processor: network_broadcast('{"event":"OBSTACLE"}')

Dashboard: ws_on_message → d.event === 'OBSTACLE'
Dashboard: obstacle_show() — warning banner, 5 s auto-hide
Dashboard: log_add('OBSTACLE DETECTED — motors stopped', 'ev-obstacle')

Recovery:
Arduino obstacle clears → _obstacleActive = false (stays stopped)
ESP32: next loop — state_get() == STATE_SAFE
         network_wifi_ok() && bluetooth_connected() → true
         state_set(STATE_READY, "links restored")
Dashboard: next telemetry frame — state "READY" → face_set("READY")
```

---

## 8. Memory Strategy

The primary risk in long-running embedded firmware is heap fragmentation. V2 eliminates dynamic allocation from all hot paths.

| V1 (problem)                          | V2 (solution)                                |
|---------------------------------------|----------------------------------------------|
| `String _rxBuffer` in BT loop         | `static char _rxBuf[BT_RX_BUF_SIZE]`        |
| `String bluetooth_read()` by value    | `const char* bluetooth_read()` into static   |
| `String _lastCmd` on heap             | `static char _lastCmd[32]`                   |
| `telemetry_build()` returns `String`  | `telemetry_build(char* buf, size_t len)`     |
| Magic number `6000` in BT module      | `BT_SILENCE_TIMEOUT_MS` from config.h       |

The only remaining `String` usage in the ESP32 firmware is inside the Arduino WebSocketsServer and ArduinoJson libraries. These are third-party and outside direct control. Both are well-tested against fragmentation.

---

## 9. Future Expansion Paths

### AI assistant (voice + vision)

`voice_engine.h` and `vision_engine.h` define the module interfaces. Both observe state via callback registration — they do not poll state_manager. This means adding voice/vision cues requires only:
1. Implement the driver in the `.cpp`.
2. Register `voice_on_state_change` with `state_manager` in `main.cpp`.

No existing module changes.

### Autonomous navigation

The command processor already has a state gate. Autonomous navigation would be a new module that calls `command_receive()` with validated move commands — the same path the dashboard uses. It requires no new routing.

### OTA firmware update

The `platformio.ini` already specifies `min_spiffs.csv` partitions which includes two OTA app slots. Implementing OTA requires adding the Arduino `Update` library and an HTTP endpoint — no architectural change.

### Second sensor layer

The obstacle threshold is a constant in `config.h`. Additional sensors (IR cliff detection, IMU tilt) would add new `STATE_SAFE` triggers in `main.cpp`'s recovery check block — the state machine does not change.
