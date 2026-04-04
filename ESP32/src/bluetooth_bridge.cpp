#include "bluetooth_bridge.h"
#include "config.h"
#include "state_manager.h"
#include <HardwareSerial.h>
#include <Arduino.h>

// ─── Private ─────────────────────────────────────────────────────────────────
static HardwareSerial _btSerial(BT_UART_NUM);

// RX line buffer — static, no heap allocation
static char     _rxBuf[BT_RX_BUF_SIZE];
static uint8_t  _rxLen      = 0;
static char     _lastLine[BT_RX_BUF_SIZE];
static bool     _lineReady  = false;

static bool     _connected  = false;
static uint32_t _lastRxMs   = 0;

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void bluetooth_init() {
    _btSerial.begin(BT_BAUD, SERIAL_8N1, BT_RX_PIN, BT_TX_PIN);
    _lastRxMs  = millis();
    _rxLen     = 0;
    _lineReady = false;
    Serial.printf("[BT] UART%d init  TX=%d RX=%d baud=%d\n",
                  BT_UART_NUM, BT_TX_PIN, BT_RX_PIN, BT_BAUD);
}

void bluetooth_update() {
    _lineReady = false;    // Cleared each update; caller checks within same loop tick

    // ── Drain UART RX FIFO ───────────────────────────────────────────────────
    while (_btSerial.available()) {
        char c = static_cast<char>(_btSerial.read());
        _lastRxMs = millis();

        if (!_connected) {
            _connected = true;
            Serial.println(F("[BT] Link UP (first byte)"));
        }

        if (c == '\n') {
            // Strip trailing CR if present
            if (_rxLen > 0 && _rxBuf[_rxLen - 1] == '\r') {
                _rxLen--;
            }
            if (_rxLen > 0) {
                _rxBuf[_rxLen] = '\0';
                memcpy(_lastLine, _rxBuf, _rxLen + 1);
                _lineReady = true;
                Serial.printf("[BT] RX: %s\n", _lastLine);
            }
            _rxLen = 0;
        } else {
            // Guard against buffer overrun — discard overlong lines
            if (_rxLen < BT_RX_BUF_SIZE - 1) {
                _rxBuf[_rxLen++] = c;
            } else {
                // Overrun: reset and discard
                Serial.println(F("[BT] RX overrun — discarding line"));
                _rxLen = 0;
            }
        }
    }

    // ── Silence watchdog ─────────────────────────────────────────────────────
    // Only armed after first connection to avoid false positive at boot.
    if (_connected) {
        const uint32_t silence = millis() - _lastRxMs;
        if (silence >= BT_SILENCE_TIMEOUT_MS) {
            _connected = false;
            Serial.printf("[BT] Link LOST (silence %lu ms) → SAFE\n", silence);
            state_set(STATE_SAFE, "BT silence timeout");
        }
    }
}

// ─── TX ──────────────────────────────────────────────────────────────────────
void bluetooth_send(const char* cmd) {
    _btSerial.println(cmd);     // println appends \r\n — Arduino trims both
    Serial.printf("[BT] TX: %s\n", cmd);
}

// ─── RX ──────────────────────────────────────────────────────────────────────
bool        bluetooth_available()  { return _lineReady; }
const char* bluetooth_read()       { return _lastLine; }

// ─── Status ──────────────────────────────────────────────────────────────────
bool     bluetooth_connected()  { return _connected; }
uint32_t bluetooth_last_rx_ms() { return _lastRxMs; }
