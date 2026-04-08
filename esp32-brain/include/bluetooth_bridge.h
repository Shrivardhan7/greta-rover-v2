/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once

// ============================================================================
//  bluetooth_bridge.h - Greta V2
//
//  Owns the UART2 <-> HC-05 physical channel.
//  Knows about bytes and lines, not command policy.
//  Command meaning is interpreted by higher-level modules.
// ============================================================================

#include <Arduino.h>

// Lifecycle
void bluetooth_init(void);
void bluetooth_update(void);

// TX
// Sends cmd + "\r\n". cmd must be null-terminated ASCII, max 32 chars.
void bluetooth_send(const char* cmd);

// RX
bool bluetooth_available(void);
const char* bluetooth_read(void);

// Status
bool bluetooth_connected(void);
uint32_t bluetooth_last_rx_ms(void);
