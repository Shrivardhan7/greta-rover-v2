#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  bluetooth_bridge.h  —  Greta V2
//
//  Design rationale:
//    This module owns the UART2 ↔ HC-05 physical channel. It knows nothing
//    about commands — it knows bytes and lines. The command processor
//    interprets meaning; this module only guarantees framing and connection
//    health detection.
//
//    RX uses a fixed-size stack buffer to eliminate heap fragmentation from
//    Arduino String concatenation in a tight loop.
// ══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "config.h"

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void bluetooth_init();
void bluetooth_update();    // Drain UART, detect silence — call every loop()

// ─── TX ──────────────────────────────────────────────────────────────────────
// Sends cmd + '\n'. cmd must be null-terminated ASCII, max 32 chars.
void bluetooth_send(const char* cmd);

// ─── RX ──────────────────────────────────────────────────────────────────────
// Returns true when a complete newline-terminated line is ready.
bool bluetooth_available();

// Returns pointer to the last received line (valid until next bluetooth_update call).
// Caller must NOT hold this pointer across loop() iterations.
const char* bluetooth_read();

// ─── Status ──────────────────────────────────────────────────────────────────
bool     bluetooth_connected();
uint32_t bluetooth_last_rx_ms();    // millis() of last received byte
