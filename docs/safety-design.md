# Greta V2 — Safety Design

**Version:** 2.0.0

---

## Design Basis

A personal robotics platform operating in a home environment must stop reliably under any failure condition — not just expected failures, but any combination of link loss, software bug, power interruption, or operator error.

The safety design is based on two principles derived from industrial practice:

**Fail-safe by default.** The motors are off by default. Power is required to keep them moving. Every watchdog stops the motors on expiry. No code path exists that starts motors without an explicit active command.

**Independent layers.** No safety layer depends on another being functional. The Arduino stops the motors without waiting for the ESP32. The ESP32 enters SAFE state without waiting for the dashboard. The dashboard sends ESTOP without waiting for a server acknowledgement.

---

## Safety Layer Summary

| Layer         | Mechanism                        | Trigger              | Action               |
|---------------|----------------------------------|----------------------|----------------------|
| Arduino L1    | Command timeout watchdog         | No cmd for 2000 ms   | `motors_stop()`      |
| Arduino L1    | Ultrasonic obstacle              | Distance < 20 cm     | `motors_stop()`      |
| ESP32 L2      | BT silence watchdog              | No bytes for 6000 ms | `STATE_SAFE`         |
| ESP32 L2      | ACK timeout                      | No ACK in 1500 ms    | `STOP` → `STATE_READY` |
| ESP32 L2      | Command timeout                  | No cmd in 2000 ms    | `STOP` → `STATE_READY` |
| ESP32 L2      | WiFi loss                        | `WL_CONNECTED` false | `STATE_SAFE`         |
| ESP32 L2      | WS client disconnect             | Client drops         | `STATE_SAFE`         |
| ESP32 L2      | Heartbeat PONG timeout           | No PONG in 12000 ms  | `STATE_SAFE`         |
| Dashboard L3  | Tab hidden / phone locked        | `visibilitychange`   | `ESTOP` via WS       |
| Dashboard L3  | Tab close / page navigation      | `beforeunload`       | `ESTOP` via WS       |
| Dashboard L3  | Manual operator action           | STOP button / Space  | `STOP` via WS        |

---

## STOP Priority Rule

`STOP` and `ESTOP` are the only commands that bypass the state machine admission gate in `command_processor`. They are forwarded to the Arduino regardless of current state — including `STATE_SAFE` and `STATE_ERROR`.

**Rationale:** If the operator presses STOP during a safety condition, the intent is unambiguous. Refusing to forward STOP because the system is already in SAFE would be confusing and potentially dangerous if the state machine has an incorrect internal state.

---

## Recovery Design

`STATE_SAFE` is not a latched fault. It recovers automatically when both link conditions are true:

```
network_wifi_ok() && bluetooth_connected()
```

This check runs in `main.cpp` every loop iteration (~1 ms). Recovery from link loss is therefore nearly instantaneous once the link is restored.

**Why automatic recovery?** A rover crossing a room may briefly lose WiFi as it moves between access points. Requiring manual intervention every time this happens makes the platform unusable in practice. Automatic recovery is safe because `STATE_SAFE` blocks all movement — the rover cannot be moving when it recovers.

**Why not recover from ERROR automatically?** `STATE_ERROR` represents an unrecoverable hardware fault (e.g., UART failure, flash corruption). Automatic recovery from an unknown fault condition could mask a real problem. ERROR recovery requires a power cycle.

---

## Timing Interdependencies

The two command timeout values (ESP32 and Arduino) are intentionally set identically at 2000 ms. This means both layers trigger their stop actions at approximately the same time on a command stream interruption.

The ACK timeout (1500 ms) is shorter than the command timeout (2000 ms). This means a failed command is detected and cleared before the command timeout watchdog fires, avoiding a double-stop sequence.

The BT silence timeout (6000 ms) is much longer than the ACK timeout. The silence timeout is designed to catch physical HC-05 disconnection, not a missed ACK. A single missed ACK should not trigger SAFE — the ACK timeout handles that more gracefully.

```
Timeline of a missed ACK (best case — no physical link loss):

t=0ms    Command sent to Arduino
t=1500ms ACK timeout fires → STOP sent → STATE_READY
t=2000ms Command timeout would have fired → suppressed (already READY)
t=6000ms BT silence timeout — NOT triggered (bytes still flowing; STOP ACK arrived)
```

---

## Arduino Independence Guarantee

The Arduino firmware is intentionally ignorant of all ESP32 state. It does not know about WiFi, WebSocket, or the state machine. This is by design.

If the ESP32 crashes, the Arduino's command timeout watchdog stops the motors within 2000 ms independently. If the Bluetooth link goes down, the Arduino again stops within 2000 ms because no new commands arrive.

The Arduino is a safety layer that operates entirely on its own.

---

## Known Limitations

**HC-SR04 blocking window.** `pulseIn()` blocks the Arduino for up to 30 ms per loop iteration while waiting for the ultrasonic echo. During this window, incoming serial bytes are buffered in hardware UART. No commands are missed, but response to a STOP command may be delayed by up to 30 ms. This is acceptable for the current use case.

**Single WebSocket client assumption.** The `network_manager` sets `_wsActive = false` when any client disconnects, even if other clients remain connected. The WebSocketsServer library does not expose a clean connected-client count. This means a second monitoring browser tab disconnecting will put the system into `STATE_SAFE`. In practice this is the correct behaviour — but it is a known simplification.

**HC-05 pairing is manual.** The Bluetooth pairing between HC-05 and ESP32 must be pre-configured. There is no automatic pairing logic. Loss of the pairing relationship requires manual AT-command reconfiguration of the HC-05 module.
