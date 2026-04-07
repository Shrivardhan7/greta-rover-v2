# GRETA V2 Hardware Platform Specification

Greta V2 is a modular distributed robotics platform designed around safety-first hardware integration, processor separation, and scalable robotics architecture.

**Copyright 2026 Shrivardhan Jadhav**
Licensed under Apache License Version 2.0 · SPDX-License-Identifier: Apache-2.0

---

## Platform Overview

- **Revision:** Rev 4.0 · **Status:** Active — Pre-Build
- Dual-processor distributed architecture: ESP32-S3 (AI/comms) + Arduino UNO (motion/actuation)
- Four-layer vertical mechanical stack with defined subsystem boundaries per layer
- All peripherals operate at 3.3V logic (AI layer) or 5V logic (control layer) — rails are isolated
- Phase-based bring-up: each hardware module validated independently before integration
- Command interface: structured ASCII packets over UART or Bluetooth SPP
- Platform designed for incremental expansion — mechanical, electrical, and firmware capacity reserved

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  PERCEPTION LAYER (Layer 4)                                     │
│  OV2640 Camera · HC-SR04 Ultrasonic (front) · Speaker output   │
├─────────────────────────────────────────────────────────────────┤
│  AI LAYER (Layer 3)                                             │
│  ESP32-S3 · OLED · INMP441 Mic · MAX98357A Amp · Breadboard     │
│  → Decision logic · Voice · Vision · WiFi/BT · Status display  │
├─────────────────────────────────────────────────────────────────┤
│  CONTROL LAYER (Layer 2)                                        │
│  Arduino UNO · L293D Shield · HC-05 · Servo · HC-SR04          │
│  → Motor execution · Servo position · Obstacle response        │
├─────────────────────────────────────────────────────────────────┤
│  MOBILITY BASE (Layer 1)                                        │
│  4WD Chassis · 4× BO Motors · Wheels · USB Power Bank          │
│  → Locomotion · Primary power source                           │
└─────────────────────────────────────────────────────────────────┘
         ↕ UART / Bluetooth SPP command interface
         ESP32-S3 (sender) ←→ Arduino UNO (receiver + sensor reporter)
```

### Subsystem Table

| Subsystem | Primary Components | Scope |
|---|---|---|
| Mobility | 4WD Chassis, 4× BO Motors, Wheels | Physical locomotion |
| Control | Arduino UNO, L293D Shield, HC-05, Servo, HC-SR04 | Actuation execution |
| AI | ESP32-S3 | Inference, decisions, wireless comms |
| Perception | OV2640, INMP441, HC-SR04 | Vision, audio, proximity sensing |
| Power | USB Power Bank, 1000µF capacitor, power rails | Regulated multi-rail distribution |

---

## Distributed Processor Model

> **⚡ ENGINEERING RULE**
> The ESP32-S3 and Arduino UNO operate as independent processing nodes with a defined command interface between them. Neither node is subordinate in terms of fault response — both implement independent safety behaviors.

### Processor Responsibility Table

| Attribute | ESP32-S3 (Brain) | Arduino UNO (Body Controller) |
|---|---|---|
| **Role** | Command sender | Command receiver + sensor reporter |
| **Domain** | AI inference, voice, vision, WiFi/BT, decision logic | Motor PWM, servo angle, real-time obstacle response |
| **Motor access** | None — delegates all actuation | Direct via L293D Shield |
| **AI access** | Full | None |
| **Fault behavior** | Watchdog timeout → command halt | No command within timeout → controlled stop |
| **Restart scope** | Independent — OTA updates without Arduino reset | Independent — Arduino holds state during ESP32 OTA |

### Command Flow

```
ESP32-S3 receives input (voice / camera / WiFi / autonomous trigger)
    ↓
Greta OS: AI inference → decision logic → command generation
    ↓
Structured ASCII packet: CMD:FWD:75 | CMD:TURN:L:90 | CMD:STOP
    ↓
UART (direct wire) or Bluetooth SPP (HC-05) → Arduino UNO
    ↓
Arduino decodes → drives L293D motor channels + servo PWM
    ↓
Arduino reports sensor data (HC-SR04 range) → ESP32 feedback loop
```

---

## Mechanical Stack

| Layer | Name | Components | Function |
|---|---|---|---|
| 4 | Sensor Layer | OV2640 (forward), HC-SR04 (front-center), Speaker (side-facing) | Forward perception envelope |
| 3 | AI Layer | ESP32-S3, 400-pt breadboard, OLED, INMP441, MAX98357A, speaker | AI processing, audio, display |
| 2 | Control Layer | Arduino UNO, L293D shield, HC-05, Servo, HC-SR04 | Motor command execution |
| 1 | Mobility Base | 4WD chassis, 4× BO motors, wheels, USB power bank | Locomotion + primary power |

### Structural Hardware

| Component | Spec | Qty | Purpose |
|---|---|---|---|
| Acrylic stack plates | 100×100mm, 3mm | 4 | Rigid per-layer mounting surface |
| M3 × 30mm standoffs | Female-Female Nylon Hex | 16 | Standard inter-layer spacing (recommended) |
| M3 × 25mm standoffs | Female-Female Nylon Hex | 16 | Compact build option |
| M3 × 40mm standoffs | Female-Female Nylon Hex | 8 | Expansion height variant |
| M3 × 10mm screws | Nylon or stainless | 32+ | Board and standoff fastening |

> **ℹ DESIGN NOTE**
> 30mm standoff spacing is the standard build. 25mm is tight — insufficient for rewiring. 40mm reserved for builds adding sensors, battery packs, or expansion boards.

**Board Mounting Sequence:**
1. Mount Layer 1 standoffs to chassis base — four corners
2. Secure Layer 2 plate; install Arduino UNO + motor shield before closing
3. Mount Layer 2→3 standoffs through Layer 2 corners
4. Install all Layer 3 components before securing Layer 3 plate
5. Mount Layer 4 sensor bracket to Layer 3 top plate
6. Route all inter-layer wiring through chassis frame channels alongside standoffs

---

## Hardware Components

### ESP32-S3 AI Layer

| # | Component | Qty | Voltage | Interface | Notes |
|---|---|---|---|---|---|
| 1 | ESP32-S3 Dev Board | 1 | 3.3V logic / 5V USB | GPIO, I2C, I2S, SPI | Brain — place first |
| 2 | OV2640 Camera Module | 1 | 3.3V | CSI ribbon cable | Ribbon connector is fragile |
| 3 | 0.96" OLED (I2C) | 1 | 3.3V | I2C SDA/SCL | Confirm I2C address: 0x3C or 0x3D |
| 4 | INMP441 I2S Microphone | 1 | 3.3V **ONLY** | I2S digital | 5V destroys chip instantly |
| 5 | MAX98357A I2S Amplifier | 1 | 3.3V–5V | I2S digital | Power from 3.3V rail |
| 6 | 8Ω Speaker | 1 | — | MAX98357A output terminals | Never wire to GPIO directly |
| 7 | Touch Sensor Module | 1 | 3.3V | GPIO | Phase 6+ optional |
| 8 | MicroSD Card | 1 | 3.3V | SPI | Insert after full system test only |
| 9 | 400-pt Breadboard | 1 | — | — | Main AI layer build surface |

### Arduino Control Layer

| # | Component | Qty | Voltage | Interface | Notes |
|---|---|---|---|---|---|
| 1 | Arduino UNO R3 | 1 | 5V | GPIO, UART, PWM | Body controller |
| 2 | L293D Motor Driver Shield | 1 | 5V logic / motor VM | Direct stack on UNO | 4-channel, 600mA per channel |
| 3 | HC-05 Bluetooth Module | 1 | 5V | UART | Voltage divider required on RXD pin |
| 4 | SG90 Servo | 1 | 5V | PWM | Camera pan or head direction |
| 5 | HC-SR04 Ultrasonic | 1 | 5V | GPIO Trig/Echo | Forward obstacle detection |
| 6 | 1000µF Electrolytic Capacitor | 1 | — | Power rail | Motor noise filtering; polarity critical |
| 7 | 4× BO Gear Motors | 4 | 5V | L293D output terminals | Differential drive, 2 per side |
| 8 | 4WD Chassis | 1 | — | — | Aluminum or acrylic base |
| 9 | USB Power Bank | 1 | 5V out | USB | 10,000mAh min; 2A min output |

---

## Electrical Architecture

### Power Distribution

```
USB Power Bank (5V / 2A min)
        │
        ├── Left breadboard red rail (+5V) ──→ ESP32-S3 VIN
        │
        └── L293D VM (motor power) ──→ 1000µF cap ──→ 4× DC motors
                │
                └── L293D VCC (logic) ──→ Arduino 5V rail

ESP32-S3 3.3V LDO output
        │
        └── Right breadboard red rail (+3.3V)
                ├── OLED VCC
                ├── INMP441 VDD
                ├── MAX98357A VIN
                ├── Touch Sensor
                └── OV2640 (via board 3.3V)

GND: All layers share a common GND reference.
     Left GND rail ←→ Right GND rail (jumper bridge required)
```

> **⚠ SAFETY RULE**
> The 3.3V and 5V power rails **must never be bridged**. Only the GND (−) rails are bridged. Connecting any 3.3V device (OLED, INMP441, MAX98357A) to the 5V rail causes permanent component damage.

### 1000µF Capacitor — Engineering Function

> **⚡ ENGINEERING RULE**
> - **Motor noise filtering:** Low-impedance shunt for high-frequency switching transients from DC motor brush commutation
> - **Voltage stabilization:** Supplies instantaneous current during direction reversal or high-torque acceleration
> - **Current buffering:** Protects power bank from momentary overcurrent during motor startup
> - **POLARITY IS CRITICAL:** Positive terminal to +5V. Reversed polarity destroys the capacitor.

### Component Voltage Reference

| Component | Required Voltage | Powered From | Peak Current |
|---|---|---|---|
| ESP32-S3 | 5V (USB input) | USB power rail | ~240 mA |
| OLED Display | 3.3V | Right red rail | ~20 mA |
| INMP441 Mic | **3.3V ONLY** | Right red rail | ~1.4 mA |
| MAX98357A Amp | 3.3V–5V | Right red rail | ~500 mA (speaker load) |
| OV2640 Camera | 3.3V | Board 3.3V (built-in) | ~30 mA |
| Touch Sensor | 3.3V | Right red rail | ~1 mA |
| SD Card | 3.3V | Board 3.3V (built-in) | ~100 mA peak |
| Arduino UNO | 5V | USB or L293D VCC | ~50 mA |
| 4× BO Motors | 5V | L293D VM | ~600 mA per channel |

---

## GPIO Allocation

> **⚡ ENGINEERING RULE**
> Verify all assignments against your specific board's pinout (Section 16A of source spec) **before wiring**. Camera-integrated boards reserve GPIO 32–39 (or equivalent) for CSI interface. Conflicts cause silent failures.

### ESP32-S3 GPIO Map

| GPIO | Module | Signal | Protocol | Notes |
|---|---|---|---|---|
| GPIO 8 | OLED Display | SDA (Data) | I2C | Default I2C SDA |
| GPIO 9 | OLED Display | SCL (Clock) | I2C | Default I2C SCL |
| GPIO 40 | INMP441 Mic | SD (Serial Data) | I2S RX | I2S0 data in |
| GPIO 41 | INMP441 Mic | WS (Word Select) | I2S RX | I2S0 WS — avoids camera pins |
| GPIO 42 | INMP441 Mic | SCK (Bit Clock) | I2S RX | I2S0 SCK |
| GPIO 45 | MAX98357A Amp | BCLK (Bit Clock) | I2S TX | I2S1 — separate bus from mic |
| GPIO 46 | MAX98357A Amp | LRC (L/R Clock) | I2S TX | I2S1 LRC |
| GPIO 47 | MAX98357A Amp | DIN (Data In) | I2S TX | I2S1 data out |
| GPIO 2 | Touch Sensor | TOUCH signal | GPIO | Capacitive touch capable |
| GPIO 12 | Future Servo | PWM signal | LEDC PWM | Reserved — Phase 6+ |
| GPIO 13 | Future I/O | Reserved | — | Reserved — Phase 6+ |

### Pins to Avoid — All ESP32-S3 Variants

| GPIO | Reason |
|---|---|
| GPIO 0 | Boot mode select — blocks firmware upload |
| GPIO 1, 3 | UART0 (USB serial) — required for flashing |
| GPIO 32–39 (camera boards) | Reserved for camera CSI interface |
| Any pin marked FLASH or PSRAM | Permanent damage if reassigned |

### Arduino UNO Pin Map

| Pin | Connected To | Signal |
|---|---|---|
| D0 (RX) | HC-05 TXD | Bluetooth receive |
| D1 (TX) | HC-05 RXD (via voltage divider) | Bluetooth transmit |
| D6 | HC-SR04 ECHO | Echo pulse input |
| D7 | HC-SR04 TRIG | Trigger pulse output |
| D9 | SG90 Servo SIGNAL | PWM control |
| D3, D11 | L293D (Motor A PWM, Motor B PWM) | Motor speed control |
| D8, D12, D13 | L293D (EN + DIR pins) | Motor direction/enable |

### Arduino Motor Driver Channel Map

| L293D Channel | Motor Position | Drive Side | Notes |
|---|---|---|---|
| M1 | Front Left | Left | Left side forward direction |
| M2 | Rear Left | Left | Parallel with M1 |
| M3 | Front Right | Right | Right side forward direction |
| M4 | Rear Right | Right | Parallel with M3 |

---

## Wiring Standards

### Wire Category Standards

| Category | Wire Type | AWG | Color Standard |
|---|---|---|---|
| Motor wiring | Stranded copper | 22–24 | Red (+), Black (−) |
| Power distribution | Stranded copper | 22–24 | Red (VCC), Black (GND) |
| Signal wiring | Jumper wire | Std | Yellow (signal/PWM), Blue (TX), Purple (RX) |
| I2S audio lines | Jumper wire | Std | Isolated from motor bundle |
| I2C / SPI / data | Jumper wire | Std | Green (data), Blue (TX), Purple (RX) |

### EMI Separation Rules

> **⚠ SAFETY RULE**
> - Motor wires **must never run parallel** to I2S microphone signal lines
> - Minimum 50mm physical separation between motor wiring and I2S audio lines
> - Motor wires routed along chassis frame perimeter (Layer 1)
> - I2S audio lines routed along center of Layer 3 breadboard
> - Unavoidable crossings: cross at **90 degrees only** — never parallel

### Grounding Rules

> **⚡ ENGINEERING RULE**
> - All GND connections from all layers connect to a **common GND reference**
> - Left and right breadboard GND rails **must be bridged**
> - Motor driver GND terminal must connect to Arduino GND
> - ESP32 GND must connect to Arduino GND through inter-layer wiring
> - Verify GND continuity (0Ω) with multimeter before powering any subsystem

### Component Placement Rules

| Component | Placement Rule | Engineering Reason |
|---|---|---|
| USB Power Bank | Centered on Layer 1 | Weight balance under acceleration |
| Arduino UNO | Centered on Layer 2 plate | Equal cable reach to all motor terminals |
| ESP32-S3 | Centered on Layer 3 breadboard, straddling gap | Equal peripheral reach; USB port accessible |
| OV2640 Camera | Front edge of Layer 3, forward-facing | Unobstructed FOV; ribbon cable exits cleanly |
| INMP441 Mic | Opposite end from MAX98357A speaker | Acoustic isolation — minimum 100mm separation |
| HC-SR04 | Front-center of Layer 2 bracket | Clear forward obstacle detection field |

### Audio Isolation

> **⚡ ENGINEERING RULE**
> INMP441 noise floor: −87 dBFS. Without isolation:
> - MAX98357A speaker output re-captured by mic → I2S receive buffer saturation
> - Motor switching noise couples into I2S lines electromagnetically at <50mm
> - Camera ribbon cable must not cross over the microphone module
> - Speaker must face outward from Layer 3 side — not toward microphone

---

## Exact Wiring Map

### Power Rails

| From | Hole | To | Hole | Wire |
|---|---|---|---|---|
| ESP32 VIN | E2 | USB 5V supply | Left red rail | Red |
| ESP32 GND | E3 | Left blue rail | Left GND | Black |
| ESP32 3.3V | J4 | Right red rail | Right red rail | Red |
| ESP32 GND | J5 | Right blue rail | Right GND | Black |
| Left GND rail | Any | Right GND rail | Any | Black (bridge) |

### OLED Display (I2C)

| OLED Pin | From | → | Destination |
|---|---|---|---|
| VCC | H25 | Right red rail | 3.3V |
| GND | H26 | Right blue rail | GND |
| SDA | H27 | E12 (GPIO 8 row) | GPIO 8 |
| SCL | H28 | E13 (GPIO 9 row) | GPIO 9 |

### INMP441 Microphone (I2S RX)

| MIC Pin | From | → | Destination |
|---|---|---|---|
| VDD | H18 | Right red rail | **3.3V ONLY** |
| GND | H19 | Right blue rail | GND |
| WS | H20 | E22 (GPIO 41) | GPIO 41 |
| SCK | H21 | E23 (GPIO 42) | GPIO 42 |
| SD | H22 | E21 (GPIO 40) | GPIO 40 |
| L/R | H23 | Right blue rail | GND (left channel) |

### MAX98357A Amplifier (I2S TX)

| AMP Pin | From | → | Destination |
|---|---|---|---|
| VIN | H8 | Right red rail | 3.3V |
| GND | H9 | Right blue rail | GND |
| BCLK | H10 | E26 (GPIO 45) | GPIO 45 |
| LRC | H11 | E27 (GPIO 46) | GPIO 46 |
| DIN | H12 | E28 (GPIO 47) | GPIO 47 |
| GAIN | H13 | Right blue rail | GND (12dB gain) |
| Speaker+ | Terminal | Speaker wire + | 8Ω speaker |
| Speaker− | Terminal | Speaker wire − | 8Ω speaker return |

> **⚠ SAFETY RULE**
> Speaker connects **only** to MAX98357A output terminals. Connecting speaker wire to any GPIO pin, power rail, or breadboard row **destroys the ESP32 instantly**.

### Arduino Wiring

| Component | Pin | Arduino Connection | Notes |
|---|---|---|---|
| HC-SR04 | VCC | Arduino 5V | +5V |
| HC-SR04 | GND | Arduino GND | Common ground |
| HC-SR04 | TRIG | Digital Pin 7 | Trigger pulse |
| HC-SR04 | ECHO | Digital Pin 6 | Echo input |
| SG90 Servo | Red (VCC) | Arduino 5V | +5V |
| SG90 Servo | Black (GND) | Arduino GND | Common ground |
| SG90 Servo | Yellow (Signal) | Digital Pin 9 | PWM |
| HC-05 | VCC | Arduino 5V | +5V |
| HC-05 | GND | Arduino GND | Common ground |
| HC-05 | TXD | Arduino RX (D0) | BT → Arduino |
| HC-05 | RXD | Arduino TX (D1) | Via 1kΩ/2kΩ divider |

---

## Assembly Phases

| Phase | Hardware Added | Pass Criteria |
|---|---|---|
| 1 — Bare Boot | ESP32-S3 + power rails only | Serial shows boot message; LED blinks 1Hz; 3.3V stable (3.2–3.4V) |
| 2 — OLED | OLED via I2C | Display shows "Greta V2 Ready"; I2C scanner reports 0x3C or 0x3D |
| 3 — Microphone | INMP441 via I2S RX | Serial prints non-zero audio levels; values change on voice input |
| 4 — Speaker | MAX98357A + speaker via I2S TX | Audible 1kHz test tone; no distortion; I2S TX reports no errors |
| 5 — Camera | OV2640 via CSI ribbon | Browser stream at `/stream` is live, clear, correct orientation |
| 6 — Integration | Full Greta OS + SD card | Dashboard loads; all subsystems pass; SD mission log written; 5-min stability |

> **⚡ ENGINEERING RULE**
> Do not skip phases. Do not proceed until current phase passes all criteria. Each phase failure narrows the fault search to the most recently added module.

**Pre-Assembly Quick-Start:**
1. Read ESP32-S3 board-specific pinout before any I2S wiring
2. Place ESP32-S3 straddling center gap — orientation is permanent after wiring
3. Wire 3.3V power rail first; verify with multimeter before connecting any peripheral
4. Add one peripheral per session; never wire all at once
5. Inspect for shorts before every power-on
6. Do not insert SD card until Phase 6

---

## Safety Engineering Rules

> **⚠ SAFETY RULES — NON-NEGOTIABLE**
>
> **S1:** Never wire while USB is connected. Power off → wire → inspect → power on.
>
> **S2:** Never connect 3.3V devices to the 5V power rail. Permanent damage is instantaneous.
>
> **S3:** Never connect speaker wire to a GPIO pin. Connect only to MAX98357A output terminals.
>
> **S4:** All GND pins of all devices must share a common ground reference.
>
> **S5:** INMP441 operates at **3.3V ONLY**. This is the most commonly damaged component.
>
> **S6:** Do not insert SD card until all Phase 1–5 subsystem tests pass.
>
> **S7:** Discharge static by touching a grounded object before handling ESP32-S3 or OV2640.
>
> **S8:** Left and right breadboard power (+) rails are **never bridged**. Only GND rails bridge.
>
> **S9:** Camera ribbon connector latch must be fully closed. Cable must not slide under light tension.
>
> **S10:** Arduino watchdog: no command received within timeout → controlled motor stop. This behavior is mandatory in firmware — not optional.

---

## Validation Criteria

Greta V2 is fully operational when **all** of the following are simultaneously true:

| # | Criterion |
|---|---|
| 1 | OLED displays live status; updates on mode change |
| 2 | Microphone detects voice input — confirmed by audio level in dashboard |
| 3 | Speaker plays audio responses at audible volume without distortion |
| 4 | Camera stream accessible from browser — live, clear, correct orientation |
| 5 | Dashboard loads and responds to commands from any browser on local WiFi |
| 6 | WiFi connection stable — no drops over 10-minute idle |
| 7 | Mission log entry written to SD card and visible in dashboard |
| 8 | Voice command "Greta status" triggers spoken response |
| 9 | Serial monitor shows no ERROR-level messages during normal operation |
| 10 | System recovers from WiFi drop without USB reset |

---

## Expansion Architecture

| Module | Purpose | Layer | Interface | Priority |
|---|---|---|---|---|
| IR / ToF / LiDAR sensors | Enhanced obstacle detection | 4 | I2C / GPIO | High |
| GPS / IMU | Autonomous navigation position tracking | 3 | I2C / UART | High |
| OTA firmware updates | Wireless deployment — no USB required | SW | WiFi / ESP32 | High |
| LiPo battery upgrade | Extended field operation; replace USB bank | 1 | Direct power | Medium |
| 2D LiDAR | Environment mapping for SLAM | 4 | UART / I2C | Medium |
| Pan/tilt servo assembly | Camera + sensor directional control | 3/4 | PWM GPIO 12/13 | Medium |
| Manipulator / arm | Physical object interaction | 5 (new) | PWM + servo | Future |

> **ℹ DESIGN NOTE**
> Reserved capacity: GPIO 12/13 (PWM), Layer 4 sensor mounting points, 40mm standoff expansion height, breadboard rows 25–30. Do not fill unused capacity prematurely.

---

## Engineering Practices

### Build Discipline Rules

| Rule | Requirement |
|---|---|
| Wiring discipline | Follow color standards; never route randomly |
| Voltage verification | Measure before connecting any new component |
| Change isolation | Test after every single change — not in batches |
| Documentation | Record every hardware change after each session |
| Physical organization | No loose wires; no cables crossing camera area |
| Single change rule | Never change wiring AND firmware simultaneously |

### Layered Architecture Benefits

| Benefit | Description |
|---|---|
| Fault isolation | Layer 2 fault diagnosed without touching Layer 3 |
| Thermal separation | Motor driver heat (L2) isolated from ESP32 (L3) |
| Modular upgrades | Individual layers replaceable without full rebuild |
| Debugging clarity | Motor issues target Arduino only; voice issues target ESP32 only |
| Wiring clarity | Each layer's wiring contained within its layer |

---

## Build Log Template

Copy this block for each build session:

| Field | Entry |
|---|---|
| Session Date | |
| Firmware Version | |
| Phase Completed | |
| Hardware Changes Made | |
| Issues Encountered | |
| Root Cause | |
| Fix Applied | |
| Serial Monitor Notes | |
| Lessons Learned | |
| 5-Minute Stability Check | |
| Next Session Goal | |

---

*GRETA V2 Hardware Platform Specification · Rev 4.0 · Build safely. Test thoroughly. Expand methodically.*
