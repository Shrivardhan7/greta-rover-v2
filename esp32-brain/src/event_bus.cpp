/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

// ============================================================================
//  event_bus.cpp — Synchronous Inter-Module Event Bus
//
//  Purpose:
//    Allows modules to communicate without including each other's headers.
//    Module A publishes an event on a channel; Module B handles it via a
//    registered callback.
//
//  Design constraints:
//    - No heap allocation. Subscription table is a static 2D array.
//    - No event queuing. Dispatch is synchronous and in-order.
//    - Re-entrant publish is NOT supported. Do not call event_publish()
//      from inside an event handler.
//    - Maximum subscribers per channel is EVENT_BUS_MAX_SUBSCRIBERS.
//      If this limit is reached, event_subscribe() returns false — treat
//      this as a configuration error and increase the constant.
//
//  Usage:
//    event_subscribe(EVENT_HEALTH_WARNING, my_handler);
//    event_publish(EVENT_HEALTH_WARNING, &payload);
// ============================================================================

#include "event_bus.h"
#include <string.h>   // memset

// ── Internal State ────────────────────────────────────────────────────────────
static EventHandler s_handlers[EVENT_CHANNEL_COUNT][EVENT_BUS_MAX_SUBSCRIBERS];
static uint8_t      s_handler_count[EVENT_CHANNEL_COUNT];

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void event_bus_init(void) {
    memset(s_handlers,      0, sizeof(s_handlers));
    memset(s_handler_count, 0, sizeof(s_handler_count));
}

// ── Subscribe ─────────────────────────────────────────────────────────────────
// Returns false if the channel is out of range, handler is NULL,
// or the subscriber limit for this channel has been reached.
bool event_subscribe(EventChannel channel, EventHandler handler) {
    if (channel >= EVENT_CHANNEL_COUNT) return false;
    if (handler == NULL)               return false;

    uint8_t count = s_handler_count[channel];

    if (count >= EVENT_BUS_MAX_SUBSCRIBERS) {
        // Configuration error: increase EVENT_BUS_MAX_SUBSCRIBERS in event_bus.h
        return false;
    }

    s_handlers[channel][count] = handler;
    s_handler_count[channel]   = count + 1;

    return true;
}

// ── Publish ───────────────────────────────────────────────────────────────────
// Calls all registered handlers for this channel in registration order.
// Does nothing if channel is out of range or payload is NULL.
void event_publish(EventChannel channel, const EventPayload* payload) {
    if (channel >= EVENT_CHANNEL_COUNT) return;
    if (payload == NULL)                return;

    uint8_t count = s_handler_count[channel];

    for (uint8_t i = 0; i < count; i++) {
        if (s_handlers[channel][i] != NULL) {
            s_handlers[channel][i](payload);
        }
    }
}
