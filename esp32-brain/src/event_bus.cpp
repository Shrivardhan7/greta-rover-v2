/*
 * event_bus.cpp
 * Greta OS — Phase 1 System Backbone
 *
 * Implementation notes:
 *   - Subscription table is a static 2D array: [channel][subscriber_index].
 *   - event_publish() iterates registered handlers and calls each in order.
 *   - No heap allocation. No queuing. Fully synchronous dispatch.
 *   - Re-entrant publish is not supported and must be avoided by callers.
 */

#include "event_bus.h"
#include <string.h>  /* memset */

/* ── Internal State ─────────────────────────────────────────────────────── */

/* One slot table per channel. Zeroed by event_bus_init(). */
static EventHandler s_handlers[EVENT_CHANNEL_COUNT][EVENT_BUS_MAX_SUBSCRIBERS];
static uint8_t      s_handler_count[EVENT_CHANNEL_COUNT];

/* ── Public Functions ───────────────────────────────────────────────────── */

void event_bus_init(void)
{
    memset(s_handlers,      0, sizeof(s_handlers));
    memset(s_handler_count, 0, sizeof(s_handler_count));
}

bool event_subscribe(EventChannel channel, EventHandler handler)
{
    if (channel >= EVENT_CHANNEL_COUNT)  return false;
    if (handler == NULL)                 return false;

    uint8_t count = s_handler_count[channel];

    if (count >= EVENT_BUS_MAX_SUBSCRIBERS) {
        /* Configuration error: increase EVENT_BUS_MAX_SUBSCRIBERS */
        return false;
    }

    s_handlers[channel][count] = handler;
    s_handler_count[channel]   = count + 1;

    return true;
}

void event_publish(EventChannel channel, const EventPayload* payload)
{
    if (channel >= EVENT_CHANNEL_COUNT) return;
    if (payload == NULL)                return;

    uint8_t count = s_handler_count[channel];

    for (uint8_t i = 0; i < count; i++) {
        if (s_handlers[channel][i] != NULL) {
            s_handlers[channel][i](payload);
        }
    }
}
