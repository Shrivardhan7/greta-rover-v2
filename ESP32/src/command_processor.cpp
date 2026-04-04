#include "command_processor.h"
#include "config.h"
#include "state_manager.h"
#include "bluetooth_bridge.h"
#include "network_manager.h"
#include <Arduino.h>

// ─── Private state ────────────────────────────────────────────────────────────
static char     _lastCmd[32]   = "NONE";
static uint32_t _lastCmdMs     = 0;
static uint32_t _cmdSentMs     = 0;    // Timestamp of last BT send — for latency
static bool     _waitingAck    = false;
static uint32_t _ackWaitMs     = 0;
static uint32_t _lastLatencyMs = 0;

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool _is_move_cmd(const char* cmd) {
    return (strcmp(cmd, CMD_FORWARD)  == 0 ||
            strcmp(cmd, CMD_BACKWARD) == 0 ||
            strcmp(cmd, CMD_LEFT)     == 0 ||
            strcmp(cmd, CMD_RIGHT)    == 0);
}

static bool _is_stop_cmd(const char* cmd) {
    return (strcmp(cmd, CMD_STOP)  == 0 ||
            strcmp(cmd, CMD_ESTOP) == 0);
}

static bool _is_whitelist(const char* cmd) {
    return _is_move_cmd(cmd) || _is_stop_cmd(cmd) ||
           strcmp(cmd, CMD_PING) == 0 ||
           strcmp(cmd, CMD_PONG) == 0;
}

static void _record_cmd(const char* cmd) {
    strncpy(_lastCmd, cmd, sizeof(_lastCmd) - 1);
    _lastCmd[sizeof(_lastCmd) - 1] = '\0';
    _lastCmdMs = millis();
}

static void _issue_stop(const char* reason) {
    Serial.printf("[CMD] Issuing STOP — reason: %s\n", reason);
    bluetooth_send(CMD_STOP);
    _record_cmd(CMD_STOP);
    _cmdSentMs  = millis();
    _waitingAck = true;
    _ackWaitMs  = millis();
    // Do not set state here — let ACK reception drive it.
    // ACK_STOP will transition → READY.
    // If ACK never arrives, the ACK timeout will also call state_set(READY).
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void command_init() {
    strncpy(_lastCmd, "NONE", sizeof(_lastCmd));
    _lastCmdMs  = millis();
    _waitingAck = false;
    Serial.println(F("[CMD] init"));
}

void command_update() {
    const uint32_t now = millis();

    // ── Command timeout watchdog ─────────────────────────────────────────────
    // Fires only in MOVING state. If the dashboard stops sending, we stop the rover.
    if (state_get() == STATE_MOVING) {
        if ((now - _lastCmdMs) >= CMD_TIMEOUT_MS) {
            _issue_stop("cmd timeout");
            state_set(STATE_READY, "cmd timeout watchdog");
        }
    }

    // ── ACK timeout watchdog ─────────────────────────────────────────────────
    // If the Arduino hasn't ACKed within the window, force-stop and clear ack wait.
    // This prevents the processor from becoming indefinitely locked on _waitingAck.
    if (_waitingAck && ((now - _ackWaitMs) >= BT_ACK_TIMEOUT_MS)) {
        Serial.println(F("[CMD] ACK timeout → STOP"));
        _waitingAck    = false;
        _lastLatencyMs = 0;
        bluetooth_send(CMD_STOP);
        state_set(STATE_READY, "ACK timeout");
    }
}

// ─── Ingress: from network ────────────────────────────────────────────────────
void command_receive(const char* cmd) {
    if (!cmd || cmd[0] == '\0') return;

    // ── PING handling ────────────────────────────────────────────────────────
    // PING comes from heartbeat; respond with PONG via network (not BT).
    if (strcmp(cmd, CMD_PING) == 0) {
        network_broadcast("{\"event\":\"PONG\"}");
        return;
    }

    // ── PONG handling ────────────────────────────────────────────────────────
    // Dashboard replies PONG to our server-sent PING. Handled by network watchdog.
    if (strcmp(cmd, CMD_PONG) == 0) {
        network_on_pong();
        return;
    }

    // ── STOP / ESTOP: always forward, no state gate ──────────────────────────
    if (_is_stop_cmd(cmd)) {
        Serial.printf("[CMD] STOP received (cmd=%s)\n", cmd);
        _issue_stop("explicit stop");
        state_set(STATE_READY, "STOP command");
        return;
    }

    // ── Reject unknown commands before any state check ───────────────────────
    if (!_is_whitelist(cmd)) {
        Serial.printf("[CMD] Rejected unknown: %s\n", cmd);
        return;
    }

    // ── State gate for movement commands ─────────────────────────────────────
    if (!state_can_move()) {
        Serial.printf("[CMD] Rejected '%s' — state %s\n", cmd, state_name());
        return;
    }

    // ── Forward to Arduino ───────────────────────────────────────────────────
    bluetooth_send(cmd);
    _record_cmd(cmd);
    _cmdSentMs  = millis();
    _waitingAck = true;
    _ackWaitMs  = millis();
    state_set(STATE_MOVING, cmd);
}

// ─── Ingress: from bluetooth ──────────────────────────────────────────────────
void command_receive_ack(const char* ack) {
    if (!ack || ack[0] == '\0') return;

    // Measure round-trip latency
    if (_waitingAck) {
        _lastLatencyMs = millis() - _cmdSentMs;
        _waitingAck    = false;
        Serial.printf("[CMD] ACK '%s' — RTT %lu ms\n", ack, _lastLatencyMs);
    } else {
        Serial.printf("[CMD] ACK '%s' (unsolicited)\n", ack);
    }

    // ── Obstacle ─────────────────────────────────────────────────────────────
    if (strcmp(ack, ACK_OBSTACLE) == 0) {
        _record_cmd("OBSTACLE");
        state_set(STATE_SAFE, "obstacle detected");
        network_broadcast("{\"event\":\"OBSTACLE\"}");
        return;
    }

    // ── STOP confirmed ───────────────────────────────────────────────────────
    if (strcmp(ack, ACK_STOP) == 0) {
        state_set(STATE_READY, "STOP ACK");
        return;
    }

    // ── Boot confirmation from Arduino ───────────────────────────────────────
    if (strcmp(ack, ACK_BOOT) == 0) {
        Serial.println(F("[CMD] Arduino boot confirmed"));
        return;
    }

    // ── Move ACKs — state remains MOVING; last ack tracked ───────────────────
    // Intentionally no state change here — MOVING was set when we sent the cmd.
}

// ─── Accessors ───────────────────────────────────────────────────────────────
const char* command_last()            { return _lastCmd; }
uint32_t    command_last_latency_ms() { return _lastLatencyMs; }
bool        command_waiting_ack()     { return _waitingAck; }
