/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

// ============================================================================
//  command_processor.cpp — Command Validation, Forwarding, and ACK Handling
//
//  Ingress path:
//    1. command_receive()     — called by network_manager on dashboard frames
//    2. command_receive_ack() — called by main.cpp after bluetooth_update()
//
//  Safety gates (in order):
//    a. Reject empty or unknown commands
//    b. STOP / ESTOP always forwarded regardless of current state
//    c. Movement commands blocked unless state_can_move() returns true
//    d. CMD_TIMEOUT_MS watchdog stops the rover if commands stop arriving
//    e. BT_ACK_TIMEOUT_MS watchdog stops the rover if Arduino doesn't ACK
//
//  ACK flow:
//    ESP32 sends command → Arduino executes → Arduino sends ACK string back
//    Round-trip latency is measured and stored in _lastLatencyMs.
//    Unacknowledged commands trigger a forced STOP after BT_ACK_TIMEOUT_MS.
//
//  Safety note:
//    _issue_stop() sends CMD_STOP over Bluetooth but does NOT directly call
//    state_set(). State transitions are driven by ACK reception or ACK timeout,
//    not by the send event. This keeps state consistent with Arduino reality.
// ============================================================================

#include "command_processor.h"
#include "config.h"
#include "state_manager.h"
#include "bluetooth_bridge.h"
#include "network_manager.h"
#include <Arduino.h>

// ── Private state ─────────────────────────────────────────────────────────────
static char     _lastCmd[32]    = "NONE";
static uint32_t _lastCmdMs      = 0;
static uint32_t _cmdSentMs      = 0;    // Timestamp of last BT send (for RTT)
static bool     _waitingAck     = false;
static uint32_t _ackWaitMs      = 0;
static uint32_t _lastLatencyMs  = 0;

// ── Command classification helpers ───────────────────────────────────────────
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

// Whitelist: only these command strings are ever forwarded to the Arduino.
// Unknown strings are rejected before any state check.
static bool _is_whitelisted(const char* cmd) {
    return _is_move_cmd(cmd) || _is_stop_cmd(cmd) ||
           strcmp(cmd, CMD_PING) == 0 ||
           strcmp(cmd, CMD_PONG) == 0;
}

static void _record_cmd(const char* cmd) {
    strncpy(_lastCmd, cmd, sizeof(_lastCmd) - 1);
    _lastCmd[sizeof(_lastCmd) - 1] = '\0';
    _lastCmdMs = millis();
}

// ── Internal stop ─────────────────────────────────────────────────────────────
// Sends CMD_STOP over Bluetooth and arms the ACK wait timer.
// Does NOT set state here — state is driven by ACK or ACK timeout.
static void _issue_stop(const char* reason) {
    Serial.printf("[CMD] STOP → Arduino (reason: %s)\n", reason);
    bluetooth_send(CMD_STOP);
    _record_cmd(CMD_STOP);
    _cmdSentMs  = millis();
    _waitingAck = true;
    _ackWaitMs  = millis();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void command_init() {
    strncpy(_lastCmd, "NONE", sizeof(_lastCmd));
    _lastCmdMs  = millis();
    _waitingAck = false;
    Serial.println(F("[CMD] init"));
}

void command_update() {
    const uint32_t now = millis();

    // ── Command timeout watchdog ──────────────────────────────────────────────
    // If the rover is MOVING but the dashboard has stopped sending commands
    // for CMD_TIMEOUT_MS, force a stop. This handles dashboard disconnections.
    if (state_get() == STATE_MOVING) {
        if ((now - _lastCmdMs) >= CMD_TIMEOUT_MS) {
            _issue_stop("cmd timeout");
            state_set(STATE_READY, "cmd timeout");
        }
    }

    // ── ACK timeout watchdog ──────────────────────────────────────────────────
    // If the Arduino hasn't ACKed within BT_ACK_TIMEOUT_MS, assume the command
    // was lost. Force STOP and clear the ACK wait flag so we don't lock up.
    if (_waitingAck && ((now - _ackWaitMs) >= BT_ACK_TIMEOUT_MS)) {
        Serial.println(F("[CMD] ACK timeout → force STOP"));
        _waitingAck    = false;
        _lastLatencyMs = 0;
        bluetooth_send(CMD_STOP);
        state_set(STATE_READY, "ACK timeout");
    }
}

// ── Ingress: from dashboard (via network_manager) ─────────────────────────────
void command_receive(const char* cmd) {
    if (!cmd || cmd[0] == '\0') return;

    // PING/PONG — heartbeat protocol, handled here, not forwarded to Arduino
    if (strcmp(cmd, CMD_PING) == 0) {
        network_broadcast("{\"event\":\"PONG\"}");
        return;
    }
    if (strcmp(cmd, CMD_PONG) == 0) {
        network_on_pong();
        return;
    }

    // STOP / ESTOP — always forward, regardless of state
    if (_is_stop_cmd(cmd)) {
        Serial.printf("[CMD] STOP received (%s)\n", cmd);
        _issue_stop("explicit stop");
        state_set(STATE_READY, "STOP command");
        return;
    }

    // Unknown commands are rejected before any further processing
    if (!_is_whitelisted(cmd)) {
        Serial.printf("[CMD] Rejected unknown command: %s\n", cmd);
        return;
    }

    // Movement commands blocked unless the FSM allows it
    if (!state_can_move()) {
        Serial.printf("[CMD] Rejected '%s' — state is %s\n", cmd, state_name());
        return;
    }

    // Forward to Arduino
    bluetooth_send(cmd);
    _record_cmd(cmd);
    _cmdSentMs  = millis();
    _waitingAck = true;
    _ackWaitMs  = millis();
    state_set(STATE_MOVING, cmd);
}

// ── Ingress: ACK from Arduino (via bluetooth_bridge) ─────────────────────────
void command_receive_ack(const char* ack) {
    if (!ack || ack[0] == '\0') return;

    // Measure round-trip latency on the first ACK after a send
    if (_waitingAck) {
        _lastLatencyMs = millis() - _cmdSentMs;
        _waitingAck    = false;
        Serial.printf("[CMD] ACK '%s' — RTT %lu ms\n", ack, _lastLatencyMs);
    } else {
        Serial.printf("[CMD] ACK '%s' (unsolicited)\n", ack);
    }

    // Obstacle detected by Arduino ultrasonic sensor
    if (strcmp(ack, ACK_OBSTACLE) == 0) {
        _record_cmd("OBSTACLE");
        state_set(STATE_SAFE, "obstacle detected");
        network_broadcast("{\"event\":\"OBSTACLE\"}");
        return;
    }

    // Arduino confirmed stop
    if (strcmp(ack, ACK_STOP) == 0) {
        state_set(STATE_READY, "STOP ACK");
        return;
    }

    // Arduino boot confirmation — log only
    if (strcmp(ack, ACK_BOOT) == 0) {
        Serial.println(F("[CMD] Arduino boot confirmed"));
        return;
    }

    // Move ACKs — state was already set to MOVING when the command was sent.
    // No state change needed here.
}

// ── Accessors ────────────────────────────────────────────────────────────────
const char* command_last()            { return _lastCmd; }
uint32_t    command_last_latency_ms() { return _lastLatencyMs; }
bool        command_waiting_ack()     { return _waitingAck; }
