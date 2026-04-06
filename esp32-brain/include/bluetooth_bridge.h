/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  bluetooth_bridge.h  —  Greta V2
//
//  Owns the UART2 ↔ HC-05 physical channel.
//  Knows about bytes and lines, not commands.
//  Command meaning is interpreted by command_processor, not here.
//
//  Buffer strategy:
//    RX uses a fixed-size static buffer (BT_RX_BUF_SIZE from config.h).
//    No heap allocation. No Arduino String. Safe in a tight loop.
//
//  Safety:
//    If no bytes arrive within BT_SILENCE_TIMEOUT_MS after first connection,
//    the module declares the link lost and calls state_set(STATE_SAFE).
// ============================================================================

#include <Arduino.h>
#include "config.h"

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void bluetooth_init();
void bluetooth_update();    // Drain UART RX, run silence watchdog — call every loop()

// ── TX ────────────────────────────────────────────────────────────────────────
// Sends cmd + "\r\n". cmd must be null-terminated ASCII, max 32 chars.
void bluetooth_send(const char* cmd);

// ── RX ────────────────────────────────────────────────────────────────────────
// Returns true when a complete newline-terminated line has been received.
bool bluetooth_available();

// Returns a pointer to the last received line (null-terminated).
// Valid until the next call to bluetooth_update().
// Do NOT store this pointer across loop() iterations.
const char* bluetooth_read();

// ── Status ────────────────────────────────────────────────────────────────────
bool     bluetooth_connected();
uint32_t bluetooth_last_rx_ms();    // millis() timestamp of the last received byte
