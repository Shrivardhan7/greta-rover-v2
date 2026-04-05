# GRETA OS — Hardware Setup

**Platform:** Greta V2  
**Primary MCU:** ESP32 (38-pin DevKit or equivalent)  
**Secondary MCU:** Arduino (Uno / Nano — motor driver interface, optional)  
**Document status:** Phase 1 hardware — sensor expansion in Phase 2

---

## ⚠ Safety Warnings

> **Power:** Never connect USB and an external motor power supply simultaneously without a proper power isolation circuit. Ground loops can damage the ESP32 and the host computer USB port.

> **Motor drivers:** Always connect motor driver ground to the common ground before applying motor supply voltage. Floating ground causes undefined PWM behaviour.

> **Current limits:** Do not exceed the rated current of the motor driver. Stall current can be 5–10× running current. Use a fused power rail.

> **ESP32 GPIO:** ESP32 GPIO is 3.3 V logic. Do not connect 5 V signals directly to GPIO pins without a level shifter.

> **Flash proximity:** Keep wiring away from open flame or heat sources during bench testing.

---

## 1. ESP32 Pin Allocation

| GPIO | Function | Connected to | Notes |
|---|---|---|---|
| GPIO 1 | TX0 (Serial) | USB-Serial / host | Boot diagnostic output |
| GPIO 3 | RX0 (Serial) | USB-Serial / host | |
| GPIO 16 | TX2 (UART2) | Arduino RX | Command output to Arduino |
| GPIO 17 | RX2 (UART2) | Arduino TX | ACK input from Arduino |
| GPIO 21 | SDA (I2C) | Sensor bus SDA | 4.7 kΩ pull-up to 3.3 V |
| GPIO 22 | SCL (I2C) | Sensor bus SCL | 4.7 kΩ pull-up to 3.3 V |
| GPIO 2 | Status LED | On-board LED | Active high; reflects FSM state |
| GPIO 4 | E-stop input | Physical button | Active low; pull-up enabled |
| EN | Enable / Reset | Reset button | |
| 3V3 | 3.3 V output | Sensors, logic | Max 500 mA total from LDO |
| GND | Common ground | All modules | |
| VIN | 5 V input | USB / regulator | 5 V regulated input |

Unallocated GPIO pins: 5, 12, 13, 14, 15, 18, 19, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39.  
Pins 34, 35, 36, 39 are input-only (no internal pull-up).

---

## 2. Arduino Connections (Secondary MCU — Optional Phase 1)

The Arduino acts as a motor driver controller, receiving motion commands from the ESP32 over UART2 and sending ACKs back.

| Arduino Pin | Function | Connected to ESP32 |
|---|---|---|
| D0 (RX) | UART RX | GPIO 16 (TX2) |
| D1 (TX) | UART TX | GPIO 17 (RX2) |
| D3 | Motor A PWM | Motor driver IN1 |
| D4 | Motor A DIR | Motor driver IN2 |
| D5 | Motor B PWM | Motor driver IN3 |
| D6 | Motor B DIR | Motor driver IN4 |
| GND | Common ground | ESP32 GND |

**Level shifting required:** Arduino operates at 5 V logic. Use a 3.3 V / 5 V bidirectional level shifter on the UART lines between ESP32 and Arduino.

---

## 3. Motor Driver Wiring

Compatible with L298N, L293D, or equivalent dual H-bridge modules.

```
Motor Supply (7–12 V) ──► Motor Driver VCC  (fused — 3 A minimum)
Common GND            ──► Motor Driver GND
Common GND            ──► ESP32 GND          (connect before power-up)

Arduino D3 (PWM)      ──► Motor Driver ENA
Arduino D4 (DIR)      ──► Motor Driver IN1
Arduino D3 (PWM)      ──► Motor Driver IN2   (complementary to IN1)

Arduino D5 (PWM)      ──► Motor Driver ENB
Arduino D6 (DIR)      ──► Motor Driver IN3
Arduino D5 (PWM)      ──► Motor Driver IN4   (complementary to IN3)

Motor Driver OUT1,2   ──► Motor A terminals
Motor Driver OUT3,4   ──► Motor B terminals
```

> **Note:** Some L298N modules include a 5 V regulator that can power the Arduino. Do not use this to power the ESP32 — use a dedicated 3.3 V regulator for the ESP32.

---

## 4. Sensor Wiring (Phase 2 planned — I2C bus)

All sensors in Phase 2 will connect to the I2C bus on GPIO 21 (SDA) and GPIO 22 (SCL).

| Sensor | I2C Address | Purpose |
|---|---|---|
| MPU-6050 | 0x68 | IMU — tilt detection for safety |
| VL53L0X | 0x29 | Time-of-flight distance sensor |
| INA219 | 0x40 | Current / power monitor on motor rail |

Pull-up resistors: 4.7 kΩ from SDA and SCL to 3.3 V. Do not use 10 kΩ — at 400 kHz (Fast Mode) the higher resistance causes signal degradation.

I2C bus scan should be run at boot and logged. Unexpected addresses indicate wiring errors.

---

## 5. Power Architecture

```
[Battery / DC Supply 7–12 V]
         │
         ├──► [Motor Rail] ── Fused 3 A ── Motor driver VCC
         │
         └──► [Logic Rail] ── Buck converter 5 V 2 A
                                    │
                                    ├──► ESP32 VIN (5 V)
                                    │      └── ESP32 LDO → 3.3 V for ESP32 + sensors
                                    │
                                    └──► Arduino VIN (5 V)
```

**Never power the ESP32 logic rail directly from the motor power rail.** Motor switching noise couples into the power rail and causes WiFi resets and Bluetooth dropouts. Use a dedicated buck converter for logic.

Recommended: Separate power switches for motor rail and logic rail. Power logic first, motors second. Shut down motors first, logic second.

---

## 6. Breadboard Layout Philosophy

For bench development:
- Place the ESP32 at the top of the breadboard.
- Place the Arduino below the ESP32.
- Run the UART level-shifter between them vertically.
- Place the motor driver on a separate breadboard or module board — not on the main prototype breadboard. Motor current spikes corrupt breadboard power rails.
- Run a thick wire (≥ 22 AWG) for motor power and ground. Thin breadboard jumpers have excessive resistance at motor currents.
- Add a 100 µF electrolytic capacitor and a 0.1 µF ceramic capacitor across the motor driver power input, close to the driver.

---

## 7. Testing Sequence

Perform tests in this order. Do not proceed to the next step if the current step fails.

### Step 1 — Power rail verification
- Apply logic power only (no motor supply).
- Measure 3.3 V at ESP32 3V3 pin with a multimeter.
- Measure 5 V at Arduino VIN.
- Verify common ground continuity between ESP32, Arduino, and motor driver with continuity tester.

### Step 2 — Serial communication
- Flash a minimal Serial echo sketch to the ESP32.
- Verify `Serial.println()` output in a serial monitor at 115200 baud.

### Step 3 — UART bridge between ESP32 and Arduino
- Flash a UART loopback test to the Arduino.
- Send a test frame from the ESP32 and verify ACK received.
- Verify level shifter is operating correctly (3.3 V logic on ESP32 side, 5 V on Arduino side).

### Step 4 — WiFi connection
- Flash Greta firmware.
- Verify `[MAIN] Boot complete` in serial output.
- Verify WiFi SSID association in serial log.
- Verify dashboard WebSocket connection established.

### Step 5 — Bluetooth connection
- Pair the secondary BT controller.
- Verify `bluetooth_connected()` returns true in serial log.
- Verify `STATE_READY` transition logged.

### Step 6 — Motor bench test (no load)
- Apply motor supply (motors disconnected from chassis — wheels off ground or belt removed).
- Send a `MOVE_FWD:10\n` command from dashboard.
- Verify motor driver outputs activate.
- Verify `STOP:\n` command stops output immediately.
- Verify `STATE_SAFE` (link drop) stops motor output.

### Step 7 — Full integration test
- Run all modules simultaneously.
- Monitor serial output for health faults, scheduler overruns, or heap warnings.
- Confirm telemetry frames reaching dashboard at expected 100 ms cadence.
