#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  command_processor.h  —  Greta V2
//
//  Design rationale:
//    The command processor is the system's policy enforcer. It sits between
//    the network layer (raw WebSocket text) and the Bluetooth bridge (raw
//    UART writes), and owns:
//
//      • Command whitelist validation
//      • State-based admission control (can we move right now?)
//      • ACK correlation (did the Arduino confirm?)
//      • Command timeout watchdog (auto-STOP on loss of input)
//      • ACK timeout watchdog (auto-STOP on silent Arduino)
//      • Round-trip latency measurement
//
//    It does NOT know about WiFi, WebSocket, or UART details. Those are
//    the responsibility of network_manager and bluetooth_bridge respectively.
// ══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void command_init();
void command_update();    // Watchdog pump — call every loop()

// ─── Ingress (from network layer) ────────────────────────────────────────────
void command_receive(const char* cmd);

// ─── Ingress (from bluetooth layer) ──────────────────────────────────────────
void command_receive_ack(const char* ack);

// ─── Accessors ───────────────────────────────────────────────────────────────
const char* command_last();           // Last command forwarded to Arduino
uint32_t    command_last_latency_ms(); // Last ACK round-trip time (0 if no ACK yet)
bool        command_waiting_ack();
