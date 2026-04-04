#include <Arduino.h>
#include "config.h"
#include "state_manager.h"
#include "network_manager.h"
#include "command_processor.h"
#include "bluetooth_bridge.h"
#include "telemetry.h"

// ─── Command callback (network → command processor) ───────────────────────────
// Kept here rather than in network_manager to preserve module independence.
// network_manager knows nothing about command_processor.
static void on_dashboard_command(const char* cmd) {
    command_receive(cmd);
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(100);    // Allow USB CDC to settle — the only permissible delay at boot
    Serial.printf("\n=== Greta V%s Boot  %s ===\n", GRETA_VERSION, GRETA_BUILD_DATE);

    // Init order matters: state first, then hardware layers, then network.
    // Network init fires WiFi.begin() which is non-blocking; module ordering
    // ensures state machine is valid before any callback could fire.
    state_init();
    state_set(STATE_CONNECTING, "boot");

    bluetooth_init();
    command_init();
    telemetry_init();

    network_set_command_callback(on_dashboard_command);
    network_init();

    Serial.println(F("[MAIN] Boot complete"));
}

// ─── Loop ────────────────────────────────────────────────────────────────────
// Execution order is deliberate:
//   1. Network first — incoming commands must enter the pipeline before watchdogs run.
//   2. Bluetooth second — ACKs consumed before command_update fires watchdogs.
//   3. Command watchdogs — after all ingress is processed.
//   4. State update — pure FSM bookkeeping.
//   5. Telemetry — outbound, rate-limited, lowest priority.
//   6. Recovery checks — explicit link-restoration logic kept in one place.
void loop() {
    network_update();

    bluetooth_update();

    if (bluetooth_available()) {
        command_receive_ack(bluetooth_read());
    }

    command_update();

    state_update();

    telemetry_update();

    // ── Link recovery ────────────────────────────────────────────────────────
    // Centralised here rather than scattered across modules.
    // Both conditions (WiFi + BT) must be healthy before we un-gate movement.
    const bool linksOk = network_wifi_ok() && bluetooth_connected();

    if (state_get() == STATE_CONNECTING && linksOk) {
        state_set(STATE_READY, "links up");
    }

    if (state_get() == STATE_SAFE && linksOk) {
        Serial.println(F("[MAIN] Links restored → READY"));
        state_set(STATE_READY, "links restored");
    }
}
