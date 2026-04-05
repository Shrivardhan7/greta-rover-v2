#include "telemetry.h"
#include "config.h"
#include "state_manager.h"
#include "network_manager.h"
#include "bluetooth_bridge.h"
#include "command_processor.h"
#include <ArduinoJson.h>
#include <Arduino.h>

// ─── Private ─────────────────────────────────────────────────────────────────
static uint32_t _lastBroadcastMs = 0;

// Static JSON output buffer — sized to fit worst-case telemetry frame.
// 256 bytes is sufficient; ArduinoJson StaticJsonDocument lives on the stack.
static char _jsonBuf[256];

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void telemetry_init() {
    _lastBroadcastMs = millis();
    Serial.println(F("[TEL] init"));
}

void telemetry_update() {
    const uint32_t now = millis();
    if ((now - _lastBroadcastMs) < TELEMETRY_INTERVAL_MS) return;
    _lastBroadcastMs = now;

    telemetry_build(_jsonBuf, sizeof(_jsonBuf));
    network_broadcast(_jsonBuf);
}

// ─── Build ────────────────────────────────────────────────────────────────────
// Writes JSON into the provided buffer. Does NOT allocate from the heap.
void telemetry_build(char* buf, size_t bufLen) {
    // StaticJsonDocument lives on the stack frame of this call — no heap.
    StaticJsonDocument<256> doc;

    doc[TEL_KEY_STATE]    = state_name();
    doc[TEL_KEY_WIFI]     = network_wifi_ok()     ? "OK" : "LOST";
    doc[TEL_KEY_BT]       = bluetooth_connected() ? "OK" : "LOST";
    doc[TEL_KEY_UPTIME]   = millis() / 1000;
    doc[TEL_KEY_LAST_CMD] = command_last();

#ifdef FEATURE_TELEMETRY_RSSI
    doc[TEL_KEY_RSSI]     = network_rssi();
#endif

#ifdef FEATURE_TELEMETRY_LATENCY
    const uint32_t lat = command_last_latency_ms();
    if (lat > 0) doc[TEL_KEY_LATENCY] = lat;
#endif

    serializeJson(doc, buf, bufLen);
}
