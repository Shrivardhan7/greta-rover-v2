/*
 * event_bus.h
 * Greta OS — Phase 1 System Backbone
 *
 * Lightweight synchronous publish-subscribe dispatcher.
 * Decouples non-safety-critical module communication.
 *
 * RULES (enforce in code review):
 *   - Must NOT be used to trigger FSM state transitions.
 *   - Subscriptions registered at init time ONLY. Never at runtime.
 *   - Handlers must be non-blocking and return quickly.
 *   - event_publish() dispatches synchronously in the caller's context.
 *   - No dynamic memory allocation.
 */

#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>

/* ── Configuration ──────────────────────────────────────────────────────── */

#define EVENT_BUS_MAX_SUBSCRIBERS 8   /* per channel; tune to fit RAM budget */

/* ── Event Channels ─────────────────────────────────────────────────────── */

typedef enum {
    EVENT_STATE_CHANGED     = 0,  /* FSM state changed (read-only notification) */
    EVENT_MODE_CHANGED      = 1,  /* Operational mode changed                   */
    EVENT_LINK_LOST         = 2,  /* WiFi or BT link lost                       */
    EVENT_LINK_RESTORED     = 3,  /* WiFi or BT link restored                   */
    EVENT_HEALTH_WARNING    = 4,  /* Health score crossed warning threshold      */

    EVENT_CHANNEL_COUNT           /* Keep last — used for array sizing          */
} EventChannel;

/* ── Event Payload ──────────────────────────────────────────────────────── */

/*
 * Fixed-size payload. data[] is channel-specific.
 * Subscribers must not retain the payload pointer beyond the handler call.
 */
typedef struct {
    EventChannel channel;
    uint32_t     timestamp_ms;    /* millis() at publish time */
    uint8_t      data[8];         /* channel-specific context; see channel docs */
} EventPayload;

/* ── Handler Type ───────────────────────────────────────────────────────── */

typedef void (*EventHandler)(const EventPayload* payload);

/* ── Public API ─────────────────────────────────────────────────────────── */

/*
 * event_bus_init()
 * Clear all subscription tables. Call once from main.cpp before any
 * event_subscribe() calls. Must be called before scheduler and
 * health_manager init.
 */
void event_bus_init(void);

/*
 * event_subscribe()
 * Register a handler for a channel. Call at module init time ONLY.
 * Returns true on success. Returns false if the subscriber table is full.
 * A false return is a configuration error — increase EVENT_BUS_MAX_SUBSCRIBERS.
 */
bool event_subscribe(EventChannel channel, EventHandler handler);

/*
 * event_publish()
 * Dispatch payload to all subscribers registered on this channel.
 * Calls handlers synchronously in registration order.
 * Handlers must not call event_publish() (no re-entrant dispatch).
 * Must be called from main loop context only — not from ISRs.
 */
void event_publish(EventChannel channel, const EventPayload* payload);

#endif /* EVENT_BUS_H */
