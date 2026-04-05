#include "network_manager.h"
#include "config.h"
#include "state_manager.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebSocketsServer.h>
#include <Arduino.h>

// ─── Multi-SSID credential table ─────────────────────────────────────────────
// Populated from wifi_secrets.h — define WIFI_SSID_n / WIFI_PASS_n there.
struct WifiCredential {
    const char* ssid;
    const char* pass;
};

static const WifiCredential WIFI_NETWORKS[] = {
    { WIFI_SSID_0, WIFI_PASS_0 },
#ifdef WIFI_SSID_1
    { WIFI_SSID_1, WIFI_PASS_1 },
#endif
#ifdef WIFI_SSID_2
    { WIFI_SSID_2, WIFI_PASS_2 },
#endif
};
static const uint8_t WIFI_NETWORK_COUNT =
    sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);

// ─── Private state ────────────────────────────────────────────────────────────
static WebSocketsServer _ws(WS_PORT);
static CommandCallback  _cmdCb          = nullptr;
static bool             _wifiOk         = false;
static bool             _wsActive       = false;   // ≥1 dashboard client connected
static uint8_t          _ssidIndex      = 0;       // Which network we're trying
static uint32_t         _connectStartMs = 0;       // When we began this attempt
static uint32_t         _reconnectMs    = 0;       // Retry cooldown

// Heartbeat (PING → dashboard, expect PONG back)
static uint32_t         _lastPingMs     = 0;
static uint32_t         _lastPongMs     = 0;
static bool             _heartbeatArmed = false;

// ─── Forward declarations ────────────────────────────────────────────────────
static void _start_connect(uint8_t idx);
static void _on_wifi_up();
static void _ws_event(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void network_init() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);    // We manage reconnection explicitly
    _ssidIndex      = 0;
    _connectStartMs = millis();
    _start_connect(_ssidIndex);
}

void network_update() {
    const uint32_t now = millis();

    // ── WiFi state machine ───────────────────────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
        if (_wifiOk) {
            // Connection was up — just dropped
            _wifiOk   = false;
            _wsActive = false;
            Serial.println(F("[NET] WiFi lost → SAFE"));
            state_set(STATE_SAFE, "WiFi lost");
        }

        // Check if this attempt has timed out
        if ((now - _connectStartMs) >= WIFI_CONNECT_TIMEOUT_MS) {
            // Try next SSID in the list
            _ssidIndex = (_ssidIndex + 1) % WIFI_NETWORK_COUNT;
            Serial.printf("[NET] Timeout — trying SSID[%d]: %s\n",
                          _ssidIndex, WIFI_NETWORKS[_ssidIndex].ssid);
            _start_connect(_ssidIndex);
        }
        return;
    }

    // WiFi is connected
    if (!_wifiOk) {
        _wifiOk = true;
        _on_wifi_up();
    }

    _ws.loop();

    // ── Heartbeat PING ───────────────────────────────────────────────────────
#ifdef FEATURE_HEARTBEAT
    if (_wsActive && _heartbeatArmed) {
        if ((now - _lastPingMs) >= HEARTBEAT_INTERVAL_MS) {
            _ws.broadcastTXT("{\"event\":\"PING\"}");
            _lastPingMs = now;
        }
        // PONG watchdog — if dashboard hasn't replied in time, treat as disconnected
        if ((now - _lastPongMs) >= HEARTBEAT_PONG_TIMEOUT_MS) {
            Serial.println(F("[NET] Heartbeat PONG timeout → SAFE"));
            state_set(STATE_SAFE, "heartbeat timeout");
            _heartbeatArmed = false;
        }
    }
#endif
}

// ─── TX ──────────────────────────────────────────────────────────────────────
void network_broadcast(const char* msg) {
    if (_wifiOk && msg) {
        _ws.broadcastTXT(msg);
    }
}

// ─── Heartbeat ───────────────────────────────────────────────────────────────
void network_on_pong() {
    _lastPongMs = millis();
    Serial.println(F("[NET] PONG received"));
}

// ─── Status ──────────────────────────────────────────────────────────────────
bool        network_wifi_ok()               { return _wifiOk; }
bool        network_ws_client_connected()   { return _wsActive; }
int8_t      network_rssi()                  { return _wifiOk ? (int8_t)WiFi.RSSI() : 0; }
const char* network_ssid()                  { return _wifiOk ? WiFi.SSID().c_str() : ""; }

void network_set_command_callback(CommandCallback cb) { _cmdCb = cb; }

// ─── Private ─────────────────────────────────────────────────────────────────
static void _start_connect(uint8_t idx) {
    WiFi.disconnect(true);
    Serial.printf("[NET] Connecting to '%s'…\n", WIFI_NETWORKS[idx].ssid);
    WiFi.begin(WIFI_NETWORKS[idx].ssid, WIFI_NETWORKS[idx].pass);
    _connectStartMs = millis();
}

static void _on_wifi_up() {
    Serial.printf("[NET] WiFi UP  SSID='%s'  IP=%s  RSSI=%d dBm\n",
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());

    if (MDNS.begin(MDNS_HOSTNAME)) {
        Serial.printf("[NET] mDNS → %s.local\n", MDNS_HOSTNAME);
    } else {
        Serial.println(F("[NET] mDNS init failed"));
    }

    _ws.begin();
    _ws.onEvent(_ws_event);
    Serial.printf("[NET] WebSocket listening on :%d/ws\n", WS_PORT);

    _lastPongMs     = millis();    // Arm heartbeat from now
    _heartbeatArmed = false;       // Wait for first client before pinging
}

static void _ws_event(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {

        case WStype_CONNECTED:
            _wsActive = true;
            Serial.printf("[WS] Client #%u connected\n", num);
            // Arm heartbeat once a client is present
            _lastPongMs     = millis();
            _heartbeatArmed = true;
            break;

        case WStype_DISCONNECTED:
            Serial.printf("[WS] Client #%u disconnected\n", num);
            // Conservative: mark inactive. A real fleet would query client count.
            // WebSocketsServer does not expose a live client count cleanly,
            // so we set SAFE — the browser auto-reconnects and recovers.
            _wsActive       = false;
            _heartbeatArmed = false;
            state_set(STATE_SAFE, "WS client disconnected");
            break;

        case WStype_TEXT: {
            if (!payload || length == 0) break;
            // Null-terminate in place (payload is library-managed buffer)
            payload[length] = '\0';
            // Trim trailing whitespace
            char* p = (char*)payload;
            while (length > 0 && (p[length-1] == ' ' || p[length-1] == '\r' || p[length-1] == '\n')) {
                p[--length] = '\0';
            }
            Serial.printf("[WS] RX #%u: %s\n", num, p);
            if (_cmdCb) _cmdCb(p);
            break;
        }

        default:
            break;
    }
}
