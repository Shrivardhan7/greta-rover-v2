/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  event_bus.h  —  Greta OS
//
//  Lightweight synchronous publish-subscribe dispatcher.
//  Decouples non-safety-critical module communication.
//
//  Rules (enforce in code review):
//    - Must NOT be used to trigger FSM state transitions directly.
//    - Subscriptions must be registered at init time only — never at runtime.
//    - Handlers must be non-blocking and return quickly.
//    - event_publish() dispatches synchronously in the caller's context.
//    - Re-entrant publish is NOT supported — do not call event_publish()
//      from inside an event handler.
//    - No dynamic memory allocation.
// ============================================================================

#include <stdint.h>

// ── Configuration ─────────────────────────────────────────────────────────────
#define EVENT_BUS_MAX_SUBSCRIBERS  8    // Max handlers per channel — tune to fit RAM

// ── Event Channels ────────────────────────────────────────────────────────────
typedef enum {
    EVENT_STATE_CHANGED   = 0,  // FSM state changed (read-only notification)
    EVENT_MODE_CHANGED    = 1,  // Operating mode changed
    EVENT_LINK_LOST       = 2,  // WiFi or BT link lost
    EVENT_LINK_RESTORED   = 3,  // WiFi or BT link restored
    EVENT_HEALTH_WARNING  = 4,  // Health score dropped below warning threshold

    EVENT_CHANNEL_COUNT         // Keep last — used for array sizing
} EventChannel;

// ── Event Payload ─────────────────────────────────────────────────────────────
// Fixed-size payload. data[] contents are channel-specific (see channel docs).
// Subscribers must NOT retain the payload pointer beyond the handler call.
typedef struct {
    EventChannel channel;
    uint32_t     timestamp_ms;  // millis() at publish time
    uint8_t      data[8];       // Channel-specific context data
} EventPayload;

// ── Handler Type ──────────────────────────────────────────────────────────────
typedef void (*EventHandler)(const EventPayload* payload);

// ── Public API ────────────────────────────────────────────────────────────────

// Clear all subscription tables.
// Call once from main.cpp before any event_subscribe() calls.
void event_bus_init(void);

// Register a handler for a channel.
// Call at module init time only. Never at runtime.
// Returns true on success, false if the subscriber table for this channel is full.
// A false return is a configuration error — increase EVENT_BUS_MAX_SUBSCRIBERS.
bool event_subscribe(EventChannel channel, EventHandler handler);

// Dispatch payload to all subscribers on this channel.
// Calls handlers synchronously in registration order.
// Must be called from main loop context only — not from ISRs.
void event_publish(EventChannel channel, const EventPayload* payload);
