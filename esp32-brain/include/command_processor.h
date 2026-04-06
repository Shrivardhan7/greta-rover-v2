/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  command_processor.h  —  Greta V2
//
//  The command processor is the system's policy enforcer.
//  It sits between the network layer (raw WebSocket text) and the Bluetooth
//  bridge (raw UART writes), and owns:
//
//    - Command whitelist validation (unknown strings rejected)
//    - State-based admission control (movement blocked unless state_can_move())
//    - ACK correlation (did the Arduino confirm the command?)
//    - Command timeout watchdog (auto-STOP if dashboard goes silent)
//    - ACK timeout watchdog (auto-STOP if Arduino goes silent)
//    - Round-trip latency measurement (cmd sent → ACK received)
//
//  This module does not know about WiFi, WebSocket, or UART details.
//  Those are the responsibility of network_manager and bluetooth_bridge.
// ============================================================================

#include <Arduino.h>

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void command_init();
void command_update();    // Run timeout watchdogs — call every loop()

// ── Ingress: from network layer ───────────────────────────────────────────────
void command_receive(const char* cmd);

// ── Ingress: from bluetooth layer ─────────────────────────────────────────────
void command_receive_ack(const char* ack);

// ── Accessors ────────────────────────────────────────────────────────────────
const char* command_last();            // Last command forwarded to Arduino
uint32_t    command_last_latency_ms(); // Last ACK round-trip time in ms (0 if no ACK yet)
bool        command_waiting_ack();     // True if a command has been sent but not yet ACK'd
