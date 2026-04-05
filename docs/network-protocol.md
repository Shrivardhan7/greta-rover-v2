# GRETA OS — Network Protocol

**Transport:** WiFi — WebSocket (primary), HTTP fallback  
**Bluetooth:** BLE or Classic BT (ACK and secondary control channel)  
**Direction:** Bidirectional — dashboard issues commands, robot sends telemetry  
**Encoding:** UTF-8 text frames (Phase 1); binary framing planned for Phase 3

---

## 1. Command Format

Commands flow from the dashboard to the robot via the WiFi link. They arrive in `network_manager` and are forwarded to `command_processor` via the registered callback.

### Command frame structure

```
<COMMAND_ID>:<PAYLOAD>\n
```

- `COMMAND_ID` — uppercase alphanumeric string, no spaces
- `:` — separator (required even if payload is empty)
- `PAYLOAD` — command-specific data; may be empty
- `\n` — frame terminator

### Command examples

| Frame | Meaning |
|---|---|
| `MOVE_FWD:50\n` | Move forward at 50% speed |
| `MOVE_BWD:30\n` | Move backward at 30% speed |
| `TURN_LEFT:45\n` | Turn left, 45-degree heading delta |
| `TURN_RIGHT:45\n` | Turn right, 45-degree heading delta |
| `STOP:\n` | Immediate stop — highest priority |
| `STATUS:\n` | Request immediate status telemetry frame |
| `PING:\n` | Link keepalive — expects `PONG` in telemetry |

### Command validation rules (enforced by `command_processor`)

1. Frame must end with `\n`.
2. `COMMAND_ID` must be in the registered command table.
3. `PAYLOAD` must parse to the expected type for the command.
4. State gate: motion commands are rejected if `state_get() != STATE_READY`.
5. Malformed frames are dropped with no response (prevents amplification attacks).

---

## 2. Telemetry Format

Telemetry frames flow from the robot to the dashboard at 100 ms intervals via `telemetry_update()`, and on-demand in response to `STATUS:` commands.

### Telemetry frame structure

```json
{
  "t": <uptime_ms>,
  "state": "<CONNECTING|READY|SAFE|PANIC>",
  "wifi": <rssi_dbm>,
  "bt": <0|1>,
  "heap": <free_bytes>,
  "tasks": <scheduler_task_count>,
  "health": "<OK|FAULT>",
  "cpu": <load_percent_estimate>
}
```

All fields are present in every frame. No optional fields in Phase 1.

### Telemetry field definitions

| Field | Type | Description |
|---|---|---|
| `t` | uint32 | `millis()` at frame generation time |
| `state` | string | Current FSM state name |
| `wifi` | int | WiFi RSSI in dBm; -1 if disconnected |
| `bt` | int | 1 if bluetooth connected, 0 otherwise |
| `heap` | uint32 | `ESP.getFreeHeap()` at frame time |
| `tasks` | uint8 | `scheduler_task_count()` — sanity check |
| `health` | string | `"OK"` or `"FAULT"` from `health_manager_fault()` |
| `cpu` | uint8 | Estimated CPU load 0–100 from `system_monitor` |

---

## 3. Alert Frames

Alert frames are sent immediately on demand by `telemetry_send_alert()`. They are not rate-limited and bypass the 100 ms telemetry interval.

### Alert frame structure

```json
{
  "t": <uptime_ms>,
  "alert": "<message>",
  "state": "<state_name>"
}
```

### Alert trigger conditions

| Alert message | Trigger |
|---|---|
| `"PANIC: system fault — movement disabled"` | `STATE_PANIC` entered |
| `"SAFE: link lost"` | `STATE_SAFE` entered |
| `"READY: links restored"` | `STATE_READY` after `STATE_SAFE` |
| `"HEALTH: <module> timeout"` | Health manager fault on specific module |

---

## 4. Error Responses

Command errors are reported in the telemetry stream, not as a separate response channel, to avoid multiplying socket connections.

### Error codes

| Code | Meaning |
|---|---|
| `ERR_UNKNOWN_CMD` | `COMMAND_ID` not in registered table |
| `ERR_BAD_PAYLOAD` | Payload failed type validation |
| `ERR_STATE_GATE` | Command rejected due to current FSM state |
| `ERR_BT_ACK_FAIL` | Bluetooth ACK not received within timeout |

Error frames are embedded in the regular telemetry frame as an optional `"err"` field (present only when an error occurred in the last 100 ms window).

---

## 5. Bluetooth Fallback

Bluetooth serves as a secondary control and ACK channel, not a primary command channel in Phase 1.

### Bluetooth roles

| Role | Description |
|---|---|
| ACK receiver | Receives command acknowledgements from a secondary controller (e.g., motor driver MCU) |
| Emergency stop | A dedicated STOP frame sent over BT takes effect even when WiFi is degraded |
| Link health indicator | BT connection status contributes to the `STATE_SAFE` trigger condition |

### Bluetooth frame format (Phase 1)

Plain text, same `COMMAND_ID:PAYLOAD\n` structure as WiFi commands.

ACK frames from secondary controller:

```
ACK:<COMMAND_ID>\n     — success
NAK:<COMMAND_ID>\n     — failure
```

---

## 6. Link Keepalive

The dashboard must send a `PING:\n` frame at least every 5 seconds. If `network_manager` does not receive any frame within the keepalive window, it triggers a link-loss condition that may escalate to `STATE_SAFE`.

The robot does not send keepalive frames unprompted. The dashboard is responsible for maintaining the link.

---

## 7. Future Protocol Expansion

| Phase | Addition | Change |
|---|---|---|
| 2 | Motion command set | Add `MOVE_*`, `ROTATE_*`, `SPEED_SET:` commands |
| 3 | Vision telemetry | Add `"vision"` object to telemetry frame with detection results |
| 3 | Binary framing | Replace JSON telemetry with MessagePack for bandwidth efficiency |
| 4 | Mission commands | Add `MISSION_START:<id>`, `MISSION_ABORT:` commands |
| 4 | Bidirectional event stream | Separate WebSocket channel for event bus mirroring to dashboard |
| 5 | Authenticated commands | Add HMAC signature field to command frames |

Protocol versioning: A `"proto"` field will be added to the telemetry frame in Phase 2 to allow the dashboard to negotiate protocol version.
