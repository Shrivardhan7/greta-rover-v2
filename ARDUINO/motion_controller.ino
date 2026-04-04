// ============================================================
//  Greta V2  —  motion_controller.ino
//  Arduino Uno  —  dedicated motion executor
//
//  Design rationale:
//    The Arduino is a pure executor: it receives pre-validated
//    commands from the ESP32 over Bluetooth serial, drives the
//    L298N motor driver, polls the HC-SR04 continuously, and
//    ACKs every command. It enforces its own command timeout
//    watchdog independently of the ESP32 — two independent
//    safety layers that do not share a clock.
//
//    No String objects used. All serial I/O is char-array based
//    to eliminate heap fragmentation on the Uno's 2 KB SRAM.
//
//  Wiring:
//    HC-05 TX → Uno D0 (RX)
//    HC-05 RX → Uno D1 (TX)   ← disconnect during upload
//    L298N IN1/IN2/IN3/IN4 → D5/D6/D9/D10
//    HC-SR04 TRIG → D7   ECHO → D8
//
//  Disconnect HC-05 from pins 0/1 before uploading.
// ============================================================

// ─── Pin definitions ─────────────────────────────────────────────────────────
#define PIN_IN1   5
#define PIN_IN2   6
#define PIN_IN3   9
#define PIN_IN4   10

#define PIN_TRIG  7
#define PIN_ECHO  8

// ─── Safety thresholds ───────────────────────────────────────────────────────
#define OBSTACLE_THRESHOLD_CM   20
#define CMD_TIMEOUT_MS        2000
#define ULTRASONIC_TIMEOUT_US 30000UL  // 30 ms → ~515 cm max range

// ─── Serial RX buffer ────────────────────────────────────────────────────────
#define RX_BUF_SIZE  32

// ─── Command / ACK string constants ──────────────────────────────────────────
static const char CMD_FORWARD[]  = "MOVE F";
static const char CMD_BACKWARD[] = "MOVE B";
static const char CMD_LEFT[]     = "MOVE L";
static const char CMD_RIGHT[]    = "MOVE R";
static const char CMD_STOP[]     = "STOP";

static const char ACK_FORWARD[]  = "ACK F";
static const char ACK_BACKWARD[] = "ACK B";
static const char ACK_LEFT[]     = "ACK L";
static const char ACK_RIGHT[]    = "ACK R";
static const char ACK_STOP[]     = "ACK STOP";
static const char ACK_OBSTACLE[] = "OBSTACLE";
static const char ACK_BOOT[]     = "ACK BOOT";

// ─── State ───────────────────────────────────────────────────────────────────
static char     _rxBuf[RX_BUF_SIZE];
static uint8_t  _rxLen          = 0;
static uint32_t _lastCmdMs      = 0;
static bool     _obstacleActive = false;

// ─── Motor driver ────────────────────────────────────────────────────────────

static void motors_init() {
    pinMode(PIN_IN1, OUTPUT);
    pinMode(PIN_IN2, OUTPUT);
    pinMode(PIN_IN3, OUTPUT);
    pinMode(PIN_IN4, OUTPUT);
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, LOW);
}

static void motors_stop() {
    // Brake: all pins LOW (coast). For active brake tie IN1=IN2=HIGH.
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, LOW);
}

static void motors_forward() {
    digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
}

static void motors_backward() {
    digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_IN3, LOW);  digitalWrite(PIN_IN4, HIGH);
}

static void motors_left() {
    // Pivot left: left reverse, right forward
    digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
}

static void motors_right() {
    // Pivot right: left forward, right reverse
    digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, LOW);  digitalWrite(PIN_IN4, HIGH);
}

// ─── Ultrasonic ──────────────────────────────────────────────────────────────

static void ultrasonic_init() {
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    digitalWrite(PIN_TRIG, LOW);
}

// Returns distance in cm, or -1 on timeout (no echo).
// pulseIn blocks for up to ULTRASONIC_TIMEOUT_US (30 ms).
// This is acceptable on the Uno for this use case.
static int16_t ultrasonic_read_cm() {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    unsigned long dur = pulseIn(PIN_ECHO, HIGH, ULTRASONIC_TIMEOUT_US);
    if (dur == 0) return -1;
    return (int16_t)(dur / 58);
}

// ─── Command dispatcher ──────────────────────────────────────────────────────

static void process_command(const char* cmd) {
    _lastCmdMs = millis();    // Refresh watchdog on every received command

    // STOP is always honoured, obstacle or not
    if (strcmp(cmd, CMD_STOP) == 0) {
        motors_stop();
        Serial.println(ACK_STOP);
        return;
    }

    // Reject movement commands while obstacle is active
    if (_obstacleActive) {
        Serial.println(ACK_OBSTACLE);
        return;
    }

    if (strcmp(cmd, CMD_FORWARD)  == 0) { motors_forward();  Serial.println(ACK_FORWARD);  }
    else if (strcmp(cmd, CMD_BACKWARD) == 0) { motors_backward(); Serial.println(ACK_BACKWARD); }
    else if (strcmp(cmd, CMD_LEFT)     == 0) { motors_left();     Serial.println(ACK_LEFT);     }
    else if (strcmp(cmd, CMD_RIGHT)    == 0) { motors_right();    Serial.println(ACK_RIGHT);    }
    // Unknown commands are silently discarded — ESP32 is the gatekeeper
}

// ─── Setup ───────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(9600);   // HC-05 baud — must match BT_BAUD in config.h
    motors_init();
    ultrasonic_init();
    _lastCmdMs = millis();
    _rxLen     = 0;

    // Signal boot complete to ESP32
    // ESP32 will transition STATE_CONNECTING → STATE_READY on receiving this
    Serial.println(ACK_BOOT);
}

// ─── Main loop ───────────────────────────────────────────────────────────────

void loop() {
    const uint32_t now = millis();

    // ── 1. Receive serial bytes into line buffer ───────────────────────────
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            // Strip trailing CR
            if (_rxLen > 0 && _rxBuf[_rxLen - 1] == '\r') _rxLen--;
            if (_rxLen > 0) {
                _rxBuf[_rxLen] = '\0';
                process_command(_rxBuf);
            }
            _rxLen = 0;
        } else if (c != '\r') {
            if (_rxLen < RX_BUF_SIZE - 1) {
                _rxBuf[_rxLen++] = c;
            } else {
                // Buffer overrun — discard and reset
                _rxLen = 0;
            }
        }
    }

    // ── 2. Command timeout watchdog ──────────────────────────────────────────
    // Independent of ESP32 — the Uno stops itself if the channel goes silent.
    // We do not send ACK_STOP here to avoid flooding serial on a dead link.
    if ((now - _lastCmdMs) >= CMD_TIMEOUT_MS) {
        motors_stop();
        _lastCmdMs = now;    // Reset to avoid continuous serial writes
    }

    // ── 3. Ultrasonic obstacle detection ─────────────────────────────────────
    int16_t dist    = ultrasonic_read_cm();
    bool    blocked = (dist > 0 && dist < OBSTACLE_THRESHOLD_CM);

    if (blocked && !_obstacleActive) {
        _obstacleActive = true;
        motors_stop();
        Serial.println(ACK_OBSTACLE);
    } else if (!blocked && _obstacleActive) {
        _obstacleActive = false;
        // Stay stopped — ESP32/user must re-issue a move command
    }
}
