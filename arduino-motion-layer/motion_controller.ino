// =============================================================================
//  Greta Rover OS
//  Copyright (c) 2026 Shrivardhan Jadhav
//  Licensed under Apache License 2.0
// =============================================================================
//
//  FILE:    motion_controller.ino
//  TARGET:  Arduino Uno (ATmega328P)
//  ROLE:    Dedicated motion executor layer — part of Greta Rover V2
//
//  DESIGN RATIONALE:
//    The Arduino is a pure command executor. It receives pre-validated
//    movement commands from the ESP32-S3 brain over Bluetooth serial
//    (HC-05), drives both channels of an L298N motor driver, continuously
//    polls an HC-SR04 ultrasonic sensor for obstacle detection, and ACKs
//    every command back to the ESP32.
//
//    Two independent safety layers are implemented:
//      1. Obstacle watchdog  — halts motors if anything enters the
//                              OBSTACLE_THRESHOLD_CM range.
//      2. Command timeout    — halts motors if no command is received
//                              within CMD_TIMEOUT_MS. This protects
//                              against Bluetooth link drops or ESP32
//                              crashes without needing shared state.
//
//    No String objects are used anywhere. All serial I/O is char-array
//    based to prevent heap fragmentation on the Uno's 2 KB SRAM.
//
//  WIRING SUMMARY:
//    HC-05  TX  →  Uno D0 (RX)
//    HC-05  RX  →  Uno D1 (TX)   ← disconnect during sketch upload
//    L298N  IN1 →  D5   IN2 → D6
//    L298N  IN3 →  D9   IN4 → D10
//    HC-SR04 TRIG → D7   ECHO → D8
//
//  UPLOAD NOTE:
//    Disconnect HC-05 from pins 0 and 1 before uploading this sketch.
//    Reconnect after upload is complete.
//
//  SAFETY NOTICE:
//    This software is intended for educational robotics use only.
//    Do not deploy on any platform where motor failures could cause
//    injury or property damage without adding appropriate hardware
//    safety mechanisms (e-stop, current limiting, etc.).
// =============================================================================


// =============================================================================
//  SECTION 1 — Pin Definitions
// =============================================================================

// L298N motor driver control pins (IN1-IN4 → left motor, right motor)
#define PIN_IN1   5
#define PIN_IN2   6
#define PIN_IN3   9
#define PIN_IN4   10

// HC-SR04 ultrasonic sensor pins
#define PIN_TRIG  7
#define PIN_ECHO  8


// =============================================================================
//  SECTION 2 — Safety & Timing Constants
// =============================================================================

// Distance (cm) at which the rover treats an object as an obstacle
// and halts all forward motion.
#define OBSTACLE_THRESHOLD_CM   20

// If no command is received within this window (ms), the Uno stops
// motors independently. Protects against Bluetooth dropout or ESP32 hang.
#define CMD_TIMEOUT_MS        2000

// pulseIn timeout (µs) for the ultrasonic echo. 30 ms ≈ 515 cm max range.
// Keeps loop() latency bounded even if no echo returns.
#define ULTRASONIC_TIMEOUT_US 30000UL

// RX line buffer size (bytes). Commands are short; 32 bytes is sufficient.
#define RX_BUF_SIZE  32


// =============================================================================
//  SECTION 3 — Command & ACK String Constants
// =============================================================================
//
//  Commands arrive from the ESP32 as plain ASCII lines (terminated with \n).
//  ACKs are sent back the same way so the ESP32 can track motion state.
//
//  Command protocol (must match ESP32 config.h definitions):
//    "MOVE F"  — move forward
//    "MOVE B"  — move backward
//    "MOVE L"  — pivot left
//    "MOVE R"  — pivot right
//    "STOP"    — stop immediately (always honoured, even during obstacle)

static const char CMD_FORWARD[]  = "MOVE F";
static const char CMD_BACKWARD[] = "MOVE B";
static const char CMD_LEFT[]     = "MOVE L";
static const char CMD_RIGHT[]    = "MOVE R";
static const char CMD_STOP[]     = "STOP";

// ACKs sent back to ESP32 after each command is executed
static const char ACK_FORWARD[]  = "ACK F";
static const char ACK_BACKWARD[] = "ACK B";
static const char ACK_LEFT[]     = "ACK L";
static const char ACK_RIGHT[]    = "ACK R";
static const char ACK_STOP[]     = "ACK STOP";

// Sent when a movement command is rejected due to obstacle detection
static const char ACK_OBSTACLE[] = "OBSTACLE";

// Sent once on power-on; ESP32 transitions STATE_CONNECTING → STATE_READY
static const char ACK_BOOT[]     = "ACK BOOT";


// =============================================================================
//  SECTION 4 — Module State
// =============================================================================

static char     _rxBuf[RX_BUF_SIZE];  // Serial receive line buffer
static uint8_t  _rxLen          = 0;  // Current number of bytes in buffer
static uint32_t _lastCmdMs      = 0;  // Timestamp of last valid command (ms)
static bool     _obstacleActive = false; // True while obstacle is in range


// =============================================================================
//  SECTION 5 — Motor Driver Module
// =============================================================================
//
//  L298N truth table for a single motor channel:
//    IN1=HIGH, IN2=LOW  → forward
//    IN1=LOW,  IN2=HIGH → backward
//    IN1=LOW,  IN2=LOW  → coast (free stop)
//    IN1=HIGH, IN2=HIGH → active brake (avoid sustained use)
//
//  Left motor  = IN1 (D5) + IN2 (D6)
//  Right motor = IN3 (D9) + IN4 (D10)

static void motors_init() {
    pinMode(PIN_IN1, OUTPUT);
    pinMode(PIN_IN2, OUTPUT);
    pinMode(PIN_IN3, OUTPUT);
    pinMode(PIN_IN4, OUTPUT);

    // Ensure motors are stopped at boot — never start moving unexpectedly
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, LOW);
}

// Coast stop: both IN pins LOW. Use active brake (HIGH/HIGH) if faster
// deceleration is needed, but avoid leaving in that state continuously.
static void motors_stop() {
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

// Pivot left: left motor reverses, right motor forwards
static void motors_left() {
    digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
}

// Pivot right: left motor forwards, right motor reverses
static void motors_right() {
    digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, LOW);  digitalWrite(PIN_IN4, HIGH);
}


// =============================================================================
//  SECTION 6 — Ultrasonic Sensor Module (HC-SR04)
// =============================================================================

static void ultrasonic_init() {
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    digitalWrite(PIN_TRIG, LOW);  // Ensure trigger starts LOW
}

// Fires a single ultrasonic pulse and returns distance in cm.
// Returns -1 if no echo is received within ULTRASONIC_TIMEOUT_US.
//
// NOTE: pulseIn() is a blocking call (~30 ms worst case).
// This is acceptable for the Uno at this polling rate. If lower latency
// is required in a future revision, consider a non-blocking ping library.
static int16_t ultrasonic_read_cm() {
    // Ensure clean trigger pulse
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);

    // 10 µs HIGH pulse starts the measurement
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    // Echo duration in µs; divide by 58 to get cm (speed of sound approximation)
    unsigned long dur = pulseIn(PIN_ECHO, HIGH, ULTRASONIC_TIMEOUT_US);
    if (dur == 0) return -1;   // Timeout: no object detected in range
    return (int16_t)(dur / 58);
}


// =============================================================================
//  SECTION 7 — Command Dispatcher
// =============================================================================
//
//  Called with a null-terminated command string once a full line is received.
//  STOP is always executed first, regardless of obstacle state.
//  All other movement commands are blocked while an obstacle is active.
//  Unknown commands are silently discarded — the ESP32 is responsible for
//  validating commands before sending them.

static void process_command(const char* cmd) {
    _lastCmdMs = millis();  // Refresh watchdog timer on every valid command

    // STOP is unconditional — always honoured
    if (strcmp(cmd, CMD_STOP) == 0) {
        motors_stop();
        Serial.println(ACK_STOP);
        return;
    }

    // Block all movement while obstacle is detected in front
    if (_obstacleActive) {
        Serial.println(ACK_OBSTACLE);
        return;
    }

    if      (strcmp(cmd, CMD_FORWARD)  == 0) { motors_forward();  Serial.println(ACK_FORWARD);  }
    else if (strcmp(cmd, CMD_BACKWARD) == 0) { motors_backward(); Serial.println(ACK_BACKWARD); }
    else if (strcmp(cmd, CMD_LEFT)     == 0) { motors_left();     Serial.println(ACK_LEFT);     }
    else if (strcmp(cmd, CMD_RIGHT)    == 0) { motors_right();    Serial.println(ACK_RIGHT);    }
    // Unrecognised commands are silently dropped
}


// =============================================================================
//  SECTION 8 — Arduino Setup
// =============================================================================

void setup() {
    // Baud rate must match HC-05 configuration and ESP32 config.h BT_BAUD
    Serial.begin(9600);

    motors_init();      // All motor pins LOW before anything else
    ultrasonic_init();  // Sensor ready, trigger LOW

    _lastCmdMs = millis();  // Seed watchdog timer
    _rxLen     = 0;         // Clear receive buffer

    // Signal successful boot to the ESP32.
    // The ESP32 motion_bridge module listens for this before marking
    // the Arduino link as ready (STATE_CONNECTING → STATE_READY).
    Serial.println(ACK_BOOT);
}


// =============================================================================
//  SECTION 9 — Main Loop
// =============================================================================
//
//  Loop order is deliberate:
//    1. Drain serial RX buffer first — keeps command latency low
//    2. Command timeout watchdog — safety check
//    3. Obstacle detection — may halt motors or clear the blocked flag
//
//  Typical loop execution time: ~30 ms (dominated by ultrasonic pulseIn).

void loop() {
    const uint32_t now = millis();

    // ── 1. Serial receive: accumulate bytes into line buffer ─────────────────
    while (Serial.available()) {
        char c = (char)Serial.read();

        if (c == '\n') {
            // End of line — strip any trailing CR and dispatch command
            if (_rxLen > 0 && _rxBuf[_rxLen - 1] == '\r') _rxLen--;
            if (_rxLen > 0) {
                _rxBuf[_rxLen] = '\0';
                process_command(_rxBuf);
            }
            _rxLen = 0;  // Reset buffer for next command

        } else if (c != '\r') {
            if (_rxLen < RX_BUF_SIZE - 1) {
                _rxBuf[_rxLen++] = c;
            } else {
                // Buffer overrun: discard partial command and reset
                // This should not happen with well-formed ESP32 commands
                _rxLen = 0;
            }
        }
    }

    // ── 2. Command timeout watchdog ──────────────────────────────────────────
    // If the Bluetooth link drops or the ESP32 hangs, stop motors after
    // CMD_TIMEOUT_MS of silence. We reset the timestamp rather than sending
    // ACK_STOP to avoid flooding serial on a dead link.
    if ((now - _lastCmdMs) >= CMD_TIMEOUT_MS) {
        motors_stop();
        _lastCmdMs = now;  // Prevent repeated stop triggers on every iteration
    }

    // ── 3. Ultrasonic obstacle detection ─────────────────────────────────────
    int16_t dist    = ultrasonic_read_cm();
    bool    blocked = (dist > 0 && dist < OBSTACLE_THRESHOLD_CM);

    if (blocked && !_obstacleActive) {
        // Obstacle entered range — halt motors and notify ESP32
        _obstacleActive = true;
        motors_stop();
        Serial.println(ACK_OBSTACLE);

    } else if (!blocked && _obstacleActive) {
        // Obstacle cleared — allow movement again
        // Motors remain stopped: the ESP32/user must re-issue a move command.
        // This is intentional — the rover does not autonomously resume movement.
        _obstacleActive = false;
    }
}
