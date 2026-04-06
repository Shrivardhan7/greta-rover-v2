# Greta V2 — Hardware Specification

**Revision:** Rev 4.0
**Classification:** Engineering Reference — Pre-Assembly
**Platform:** ESP32-S3 + Arduino Uno — Distributed Robotics Architecture
**Status:** Active — Pre-Build Documentation
**Date:** April 2026

---

## Quick-Start Checklist

Complete these steps in order before touching any other section of this document.

| # | Required Action |
|---|---|
| 1 | Read Section 15 (Breadboard Basics) completely before placing any hardware. |
| 2 | Read Section 16A (ESP32-S3 Pin Validation) and verify your specific board's pinout BEFORE any I2S wiring. |
| 3 | Place ESP32-S3 across the center gap per Section 16 — orientation matters. |
| 4 | Wire ONLY the 3.3V power rail first. Do not connect any peripheral yet. |
| 5 | Verify 3.3V on the right power rail with a multimeter before connecting devices. |
| 6 | Connect OLED display only and run Phase 1 firmware test (Section 20). |
| 7 | Confirm OLED displays correctly before proceeding to microphone wiring. |
| 8 | Add one peripheral at a time. Never wire all at once. |
| 9 | After every new connection: inspect for shorts before applying power. |
| 10 | Never connect speaker wire directly to a GPIO pin. |
| 11 | Do not insert SD card until camera and I2S tests pass. |

---

## Section 0 — Greta Design Philosophy

Greta V2 is not a simple hobbyist project assembled from random components. It is a deliberate, engineered robotics platform designed around four core principles that govern every hardware decision, every wiring choice, and every firmware architecture decision in this document.

### 0.1 Modular Robotics Architecture

Greta V2 is designed as a modular system. Each subsystem — mobility, AI processing, power distribution, sensing, and audio — is logically and physically separable from every other subsystem. This modularity allows individual subsystems to be repaired, upgraded, or replaced without disturbing the rest of the platform. A fault in the audio subsystem does not require disassembly of the mobility layer.

### 0.2 Separation of AI and Motor Control

The most critical architectural decision in Greta V2 is the separation of AI processing from motor control. These two functions are handled by entirely different processors, connected by a defined command interface.

The ESP32-S3 (Greta Brain) handles all cognitive functions: voice recognition, image processing, decision-making, wireless communication, and user interaction. The Arduino Uno (Greta Body Controller) handles all physical actuation: motor speed and direction, servo positioning, and real-time obstacle response.

This separation exists because motor control is time-sensitive and real-time. AI processing is computationally intensive and variable in duration. Running both on a single processor creates timing conflicts, unpredictable motor response latency, and a single point of failure.

### 0.3 Distributed Processing Design

Greta V2 implements a distributed processing architecture: intelligence at the top layer, execution at the bottom layer, with a defined communication channel between them. The ESP32 makes decisions and transmits structured command packets to the Arduino. The Arduino translates those packets into motor states and reports sensor data back upward.

### 0.4 Safety-First Hardware Integration

> **⚠ SAFETY RULE**
>
> Power rails are separated by voltage level. Sensitive components are protected by rail separation rules. Grounding is disciplined and verified before power-on. The safety rules defined in this document are hardware protection requirements — not suggestions.

Every phase of Greta V2 hardware bring-up is governed by a safety-first philosophy. No component is powered until its wiring is physically verified. No firmware is flashed until hardware is confirmed safe. No integration step is attempted until the prior step has passed its success criteria.

### 0.5 Phase Testing Strategy

Greta V2 is built using a phase-based bring-up strategy. Hardware is added one module at a time, tested against defined success criteria, and locked in before the next module is added. When a fault occurs, the root cause can be isolated to the most recently added component.

### 0.6 Future Expandability Philosophy

Greta V2 is designed to grow. The 4-layer mechanical stack provides vertical expansion space. Wiring standards define color-coding and routing rules that accommodate new subsystems without restructuring existing wiring. The firmware architecture separates subsystem modules so new capabilities can be added without rewriting core logic. Greta V2 is a platform, not a finished product.

---

## Section 1 — Greta System Architecture

Greta V2 consists of five defined subsystems. Each subsystem has a defined scope of responsibility, defined interfaces, and defined failure modes.

### 1.1 Subsystem Definitions

| Subsystem | Primary Components | Responsibilities |
|---|---|---|
| Mobility Subsystem | 4x BO Motors, 4WD Chassis, Wheels | Physical locomotion — forward, reverse, turn, stop |
| Control Subsystem | Arduino Uno, L293D Motor Driver Shield, Servo, HC-05 Bluetooth | Motor command execution, servo positioning, obstacle response |
| AI Subsystem | ESP32-S3 | Voice recognition, image inference, decision-making, wireless comms |
| Perception Subsystem | OV2640 Camera, INMP441 Microphone, HC-SR04 Ultrasonic | Environmental sensing — vision, audio, proximity |
| Power Subsystem | USB Power Bank, 1000µF Capacitor, Power Wiring | Regulated power delivery to all subsystems |

### 1.2 Processor Role Definitions

> **⚡ ENGINEERING RULE**
>
> **ESP32-S3 = GRETA BRAIN**
> Handles: AI inference, voice commands, image processing, WiFi/Bluetooth, user interface, decision logic. Does NOT directly drive motors. Sends commands only.
>
> **Arduino Uno = GRETA BODY CONTROLLER**
> Handles: Motor speed/direction, servo angle, ultrasonic obstacle detection, real-time actuation. Does NOT perform AI inference. Executes commands only.

### 1.3 Command Flow Architecture

The command flow in Greta V2 follows a strict top-down hierarchy:

1. ESP32-S3 receives environmental input — voice command, camera image, WiFi instruction, or autonomous trigger.
2. ESP32-S3 processes input through Greta OS — applies AI inference, decision logic, or command parsing.
3. ESP32-S3 generates a structured command packet — e.g., `MOVE_FORWARD`, `TURN_LEFT`, `STOP`, `SERVO_ANGLE_45`.
4. Command packet is transmitted to Arduino via serial UART or Bluetooth (HC-05).
5. Arduino receives and decodes the command packet.
6. Arduino drives motor driver outputs and servo PWM signals accordingly.
7. Arduino reports sensor data (ultrasonic range) back to ESP32 for closed-loop decision-making.

---

## Section 2 — Mechanical Stack Architecture

Greta V2 is physically organized as a 4-layer vertically stacked structure. Each layer has a defined set of components, defined interfaces to adjacent layers, and defined wiring responsibility.

### 2.1 Layer Overview

| Layer | Name | Primary Components | Function |
|---|---|---|---|
| Layer 1 | Mobility Base | 4WD chassis, 4x BO motors, wheels, power bank | Physical locomotion and primary power source |
| Layer 2 | Control Layer | Arduino Uno, L293D shield, HC-05, servo, ultrasonic | Motor control and physical actuation |
| Layer 3 | AI Layer | ESP32-S3, breadboard, OLED, INMP441, MAX98357A, speaker | AI processing, voice, display, audio |
| Layer 4 | Sensor Layer | OV2640 camera, ultrasonic front mount, speaker direction | Forward-facing perception and output |

### 2.2 Layer 1 — Mobility Base

Layer 1 is the foundation of the robot. It contains the drive chassis, motor assembly, and primary power source. All structural weight rests on this layer.

- 4WD chassis — aluminum or acrylic base plate with motor mount slots
- 4x BO motors — gear-reduced DC motors providing sufficient torque for surface navigation
- Wheel assembly — rubber-tired wheels press-fit onto motor output shafts
- Motor wiring — color-coded leads routed upward through chassis frame to Layer 2
- Power bank — centered on the chassis for balance; USB output feeds Layer 2 motor driver

### 2.3 Layer 2 — Control Layer

Layer 2 contains the Arduino Uno and all components directly controlled by the Arduino. This is the actuation layer — it receives commands from above and drives physical outputs below.

- Arduino Uno — mounted centrally for equal wire reach to all connected peripherals
- L293D motor driver shield — stacked directly onto Arduino pin headers
- HC-05 Bluetooth module — connected to Arduino UART for receiving commands from ESP32
- Servo motor — connected to Arduino PWM pin for camera pan or head direction control
- HC-SR04 ultrasonic sensor — forward-facing, connected to Arduino trigger/echo GPIO pins

### 2.4 Layer 3 — AI Layer

Layer 3 contains the ESP32-S3 and all AI subsystem components. This is the intelligence layer — it processes all sensor input, makes decisions, and issues commands downward to the control layer.

- ESP32-S3 — centered on breadboard, straddling the center gap as primary AI processor
- 400-point breadboard — primary wiring surface for all AI layer components
- 0.96-inch OLED display — connected via I2C for status and expression output
- INMP441 I2S microphone — positioned away from the speaker to minimize acoustic feedback
- MAX98357A I2S amplifier — connected to speaker for audio output
- 8-ohm speaker — mounted to side of Layer 3 frame, direction angled outward

### 2.5 Layer 4 — Sensor Layer

Layer 4 defines the forward-facing perception envelope of Greta V2. Rather than a discrete physical board layer, Layer 4 represents the mounting positions and orientations of all outward-facing sensors.

- OV2640 camera — forward-facing, mounted at front edge of Layer 3 frame
- HC-SR04 ultrasonic — mounted at front of Layer 2 or Layer 3 frame, centered for obstacle detection
- Speaker — side-facing from Layer 3 for audio output without obstructing camera field of view
- Reserved sensor mounting points — forward and rear positions available for future sensor expansion

### 2.6 Advantages of Layered Architecture

| Advantage | Engineering Benefit |
|---|---|
| Cleaner wiring | Each layer's wiring stays within its layer, reducing cross-layer interference |
| Better debugging | A fault in Layer 2 motor control can be diagnosed without touching Layer 3 AI components |
| Thermal separation | Motor driver heat (Layer 2) does not conduct directly to ESP32 processing components (Layer 3) |
| Modular upgrades | Individual layers can be replaced without rebuilding the full robot |
| Structural clarity | Mechanical assembly sequence maps directly to software bring-up sequence |

---

## Section 3 — Structural Hardware

Greta V2's layered mechanical structure is implemented using acrylic plates connected by nylon hex standoffs. All structural hardware is electrically non-conductive, lightweight, and field-replaceable.

### 3.1 Structural Component List

| Component | Specification | Qty | Purpose |
|---|---|---|---|
| Acrylic stack plates | 4×4 inch (100mm×100mm), 3mm thickness | 4 | Rigid mounting surface for each layer |
| M3 × 25mm Standoffs | Female-Female Nylon Hex, with M3 × 10mm screws | 16 | Compact layer spacing |
| M3 × 30mm Standoffs | Female-Female Nylon Hex, with M3 × 10mm screws | 16 | Recommended standard spacing |
| M3 × 40mm Standoffs | Female-Female Nylon Hex (optional expansion) | 8 | Expansion height for additional wiring clearance |
| M3 × 10mm Screws | Nylon or stainless — non-conductive preferred | 32+ | Board and standoff fastening |

### 3.2 Standoff Spacing Recommendations

- **25mm** — Compact build. Minimum clearance. Wiring space is tight. Not recommended for first builds.
- **30mm** — Recommended standard. Adequate clearance for all Layer 2 and Layer 3 components.
- **40mm** — Expansion height. Use when additional sensors, battery packs, or expansion boards are planned.

### 3.3 Board Mounting Sequence

1. Mount Layer 1 standoffs to chassis base plate — four corners.
2. Secure Layer 2 acrylic plate to standoff tops. Install Arduino Uno and motor driver shield before closing.
3. Mount Layer 2 to Layer 3 standoffs through Layer 2 plate corners.
4. Install all Layer 3 components (ESP32, breadboard, audio modules) before securing Layer 3 plate.
5. Mount Layer 4 sensor bracket to Layer 3 top plate standoffs.
6. Route all inter-layer wiring through chassis frame channels or alongside standoffs.

---

## Section 4 — Wiring Standards

All wiring in Greta V2 follows defined standards for wire gauge, color coding, routing, and EMI separation. These standards are engineering requirements that prevent interference, simplify debugging, and protect components.

### 4.1 Wiring Categories

| Category | Purpose | Wire Type | AWG | Color Standard |
|---|---|---|---|---|
| Motor wiring | Drive current from motor driver to DC motors | Stranded copper | 22–24 | Red (+), Black (−) |
| Power wiring | VCC and GND distribution between layers | Stranded copper | 22–24 | Red (VCC), Black (GND) |
| Signal wiring | GPIO, PWM, UART, trigger/echo signals | Jumper wire | Std | Yellow (signal), Blue (TX), Purple (RX) |
| Audio wiring | I2S mic and amplifier signal lines | Jumper wire | Std | Isolated from motor wires |
| Data wiring | I2C, SPI, serial data buses | Jumper wire | Std | Green (data), Blue (TX), Purple (RX) |

### 4.2 Color Standard Summary

| Color | Signal | Mandatory |
|---|---|---|
| Red | VCC / Positive supply | Yes |
| Black | GND / Negative / Ground | Yes |
| Yellow | Trigger, control, or PWM signals | Recommended |
| Green | Sensor output or data signals | Recommended |
| Blue | TX (transmit) serial signals | Recommended |
| Purple | RX (receive) serial signals | Recommended |
| Orange | Motor control lines (L293D to motor) | Recommended |

### 4.3 EMI Separation Rules

> **⚠ SAFETY RULE**
>
> Motor wires must NEVER run parallel to microphone signal wires. Maintain a minimum physical separation of 50mm between motor wiring bundles and I2S audio lines.
>
> Route motor wires along the chassis frame perimeter (Layer 1). Route I2S audio wires along the center of Layer 3 breadboard. If cross-routing is unavoidable, cross at 90 degrees — do not run parallel.

### 4.4 Grounding Discipline

> **⚡ ENGINEERING RULE**
>
> All GND connections from Layer 1 (motors), Layer 2 (Arduino), and Layer 3 (ESP32) must connect to a common GND reference point. Left and right breadboard GND rails must be bridged with a jumper wire. The motor driver GND terminal must connect to the same GND reference as the Arduino GND pin. Measure GND continuity with a multimeter before powering any subsystem.

---

## Section 5 — Arduino Robotics Control System

The Arduino control subsystem forms the physical actuation backbone of Greta V2. It translates high-level movement commands received from the ESP32 into precise motor driver outputs, servo positions, and sensor readings.

### 5.1 Motor Wiring

| Motor Driver Channel | Motor Position | Connected Motor Colors | Notes |
|---|---|---|---|
| M1 | Front Left | Red (+), Black (−) | Left side forward direction |
| M2 | Rear Left | Red (+), Black (−) | Left side — parallel with M1 |
| M3 | Front Right | Red (+), Black (−) | Right side forward direction |
| M4 | Rear Right | Red (+), Black (−) | Right side — parallel with M3 |

### 5.2 Ultrasonic Sensor Wiring

| HC-SR04 Pin | Connects To | Signal |
|---|---|---|
| VCC | Arduino 5V or separate 5V supply | +5V |
| GND | Arduino GND (common ground) | Ground |
| Trig | Arduino Digital Pin 7 | Trigger pulse output |
| Echo | Arduino Digital Pin 6 | Echo pulse input |

### 5.3 Servo Motor Wiring

| Servo Wire Color | Connects To | Signal |
|---|---|---|
| Red (VCC) | Arduino 5V or external 5V BEC | +5V supply |
| Black/Brown (GND) | Arduino GND (common ground) | Ground |
| Yellow/White (Signal) | Arduino Digital Pin 9 | PWM control signal |

### 5.4 Bluetooth Module Wiring (HC-05)

| HC-05 Pin | Connects To | Notes |
|---|---|---|
| VCC | Arduino 5V pin | Requires 5V supply |
| GND | Arduino GND | Common ground |
| TXD | Arduino RX (Digital Pin 0 or SoftwareSerial) | Transmit from HC-05 to Arduino |
| RXD | Arduino TX (Digital Pin 1 or SoftwareSerial) | Use voltage divider for 3.3V RXD pin |

### 5.5 1000µF Capacitor — Engineering Purpose

> **⚡ ENGINEERING RULE**
>
> **Motor Noise Filtering:** DC motors generate high-frequency switching noise. The capacitor acts as a low-impedance shunt, preventing noise from propagating to Arduino logic circuits.
>
> **Voltage Stabilization:** During high-torque acceleration or direction reversal, motors can cause brief supply voltage drops. The capacitor provides instantaneous current from stored charge.
>
> **Current Buffering:** Peak motor startup current can exceed the USB power bank output rating momentarily. The capacitor buffers this demand, protecting the power bank from overcurrent shutdown.
>
> **POLARITY IS CRITICAL:** Connect capacitor positive terminal to +5V. Reversed polarity destroys the capacitor.

### 5.6 Common Ground Requirement

All GND connections across the system must share a single reference node. Measure resistance between all GND points before powering on — it should read 0Ω (continuity). Use a dedicated GND bus wire on Layer 2 to ensure all components share a common ground.

---

## Section 6 — ESP32 AI System

The ESP32-S3 AI subsystem is the cognitive center of Greta V2. It integrates voice capture, audio output, visual perception, and status display into a single breadboard-based AI processing unit. All components on this layer operate at 3.3V logic.

### 6.1 OLED Display Wiring (I2C)

| OLED Pin | Connects To | Protocol | Notes |
|---|---|---|---|
| VCC | Right power rail (3.3V) | Power | 3.3V strictly |
| GND | Right GND rail | Ground | Common ground |
| SDA | ESP32-S3 GPIO 8 | I2C Data | Pull-up resistor on board |
| SCL | ESP32-S3 GPIO 9 | I2C Clock | Pull-up resistor on board |

### 6.2 INMP441 Microphone Wiring (I2S)

| MIC Pin | Connects To | Notes |
|---|---|---|
| VDD | Right power rail (3.3V) | **3.3V ONLY — 5V destroys this chip instantly** |
| GND | Right GND rail | Common ground |
| WS | ESP32-S3 GPIO 41 | I2S Word Select — left/right channel clock |
| SCK | ESP32-S3 GPIO 42 | I2S Bit Clock |
| SD | ESP32-S3 GPIO 40 | I2S Serial Data output |
| L/R | GND rail | Pulls low = left channel output |

### 6.3 MAX98357A Amplifier Wiring (I2S)

| AMP Pin | Connects To | Notes |
|---|---|---|
| VIN | Right power rail (3.3V) | 3.3V supply |
| GND | Right GND rail | Common ground |
| BCLK | ESP32-S3 GPIO 45 | I2S Bit Clock — separate I2S bus from microphone |
| LRC | ESP32-S3 GPIO 46 | I2S Left/Right Clock |
| DIN | ESP32-S3 GPIO 47 | I2S Data input from ESP32 |
| GAIN | GND rail | GND = 12dB gain setting |
| Speaker+ | Speaker positive terminal | 8-ohm speaker — do NOT connect to GPIO |
| Speaker− | Speaker negative terminal | 8-ohm speaker return |

### 6.4 Audio Isolation Engineering

The INMP441 has a noise floor below −87 dBFS. Without proper acoustic isolation, the speaker output will be re-captured by the microphone, creating an audio feedback loop that destroys voice command recognition accuracy.

- Place INMP441 on the opposite side of the breadboard from MAX98357A. Minimum 100mm physical separation.
- Motor wires must not run near microphone signal wires — motor switching noise couples electromagnetically into I2S signal lines below 50mm distance.
- Camera ribbon cable must not cross over the microphone module.
- Speaker must face outward from the side of Layer 3 — not toward the microphone.

---

## Section 7 — Power Architecture

Greta V2 operates from a single USB power bank that feeds a multi-rail power distribution architecture.

### 7.1 Primary Power Source

Recommended power bank specification:
- Capacity: 10,000 mAh minimum for field operation
- Output: 5V / 2A minimum (5V / 3A recommended)
- Peak current handling: Must tolerate 2A surge without shutting down
- Auto-shutoff: Disable or work around low-current auto-shutoff modes during development

### 7.2 Motor Driver Power Routing

- Motor supply (VM): Fed from USB power bank +5V through the 1000µF bulk capacitor. Supports up to 600mA per channel.
- Logic supply (VCC): Fed from Arduino Uno 5V regulated output or shared with VM rail.
- The separation between VM and VCC within the L293D prevents motor switching transients from disrupting Arduino logic operation.

### 7.3 ESP32 Regulated Supply

> **⚠ SAFETY RULE**
>
> The 3.3V rail from the ESP32 is the ONLY acceptable power source for OLED, INMP441, and MAX98357A on Layer 3.
>
> The USB 5V rail must NEVER connect to any 3.3V device on Layer 3. The left (5V) and right (3.3V) power rails on the breadboard must NEVER be bridged.

### 7.4 Power Noise and Capacitor Usage

DC motor brushes generate electrical noise — high-frequency voltage spikes caused by arc discharge at the brush commutator interface. This noise propagates through power supply wiring and can corrupt digital logic, reset microcontrollers, and interfere with I2S audio capture.

The 1000µF electrolytic capacitor at the motor driver power input provides a low-impedance path to ground for high-frequency transients. For high-motor-load builds, an additional 100nF ceramic capacitor placed directly across each motor's terminals provides high-frequency bypass that the bulk electrolytic cannot handle alone.

---

## Section 8 — Physical Layout Engineering

### 8.1 Placement Rules

| Component | Placement Rule | Engineering Reason |
|---|---|---|
| USB Power Bank | Centered on Layer 1 chassis | Even weight distribution — prevents tilting under acceleration |
| Arduino Uno | Centered on Layer 2 plate | Equal cable reach to all motor terminals and peripheral pins |
| ESP32-S3 | Centered on Layer 3 breadboard, straddling gap | Equal wiring reach to all peripherals; USB port accessible |
| OV2640 Camera | Front edge of Layer 3, facing forward | Unobstructed field of view; ribbon cable exits cleanly |
| INMP441 Microphone | Opposite end of breadboard from speaker | Acoustic isolation from speaker output |
| MAX98357A + Speaker | Side of Layer 3, speaker facing outward | Audio output directed outward; away from camera FOV |
| HC-SR04 Ultrasonic | Front-center of Layer 2 bracket | Clear forward obstacle detection field |

### 8.2 Wire Routing Standards

- **Power wires (Red/Black):** Route vertically between layers alongside standoffs. Secure with cable ties.
- **Signal wires (Yellow/Green):** Route horizontally along breadboard rows. Keep close to the board surface.
- **Audio wires (I2S lines):** Route along the opposite side of the breadboard from motor wires. Maintain 50mm minimum separation from Layer 2 motor wiring.
- **Camera ribbon cable:** Lay flat and straight from camera connector to ESP32-S3 CSI port. No sharp bends, no loops, no wires crossing over it.
- **Motor wires:** Bundle left-side motor wires together and right-side separately. Route each bundle along the chassis frame perimeter.
- **Inter-layer wiring harness:** Group all Layer 2-to-Layer 3 signal wires into a single harness. Route alongside the rear standoff column.

---

## Section 9 — Robot Assembly Order

Greta V2 must be assembled in a defined sequence. Deviating from this sequence creates situations where later steps require disassembly of earlier steps.

| Step | Assembly Task | Validation Before Proceeding |
|---|---|---|
| 1 | Mechanical frame — assemble chassis base, install Layer 1 standoffs | All standoffs torqued. Chassis flat and level. |
| 2 | Install motors — press wheels onto shafts, wire motor leads upward | All four motors spin freely. No binding. |
| 3 | Install Arduino system — mount Layer 2 plate, Arduino Uno, motor driver shield, HC-05, servo, ultrasonic | Arduino powers on. Serial monitor responsive. |
| 4 | Install ESP32 system — mount Layer 3 breadboard, ESP32-S3, OLED, mic, amplifier, speaker | ESP32 powers on. 3.3V rail confirmed on multimeter. |
| 5 | Route wiring — organize inter-layer wiring harness, secure cable bundles | No loose wires. No cables crossing camera area. |
| 6 | Power verification — measure voltages at all supply points | 5V at Layer 2. 3.3V at Layer 3 right rail. GND continuity confirmed. |
| 7 | Phase testing — execute all phase tests per Section 20 | All phases pass success criteria. |
| 8 | Integration testing — flash complete Greta OS, execute full system test | All subsystems operational simultaneously. |

All mechanical assembly must be completed before any wiring begins. Wiring into an unfinished mechanical structure results in incorrect wire lengths and cables routed through areas that will later be blocked by structural components.

---

## Section 10 — System Integration Design

System integration is the phase at which the AI subsystem and the control subsystem are connected and operated together as a unified robot.

### 10.1 ESP32 to Arduino Communication Interface

- **UART Serial (direct wire):** TX from ESP32 GPIO connects to Arduino RX. RX from ESP32 GPIO connects to Arduino TX. Common GND shared. Baud rate: 9600 or 115200.
- **HC-05 Bluetooth (wireless):** ESP32 sends commands via Bluetooth SPP profile. HC-05 on Arduino side receives and passes commands to Arduino UART. Useful for physically separated layers.

Command packets follow a defined ASCII protocol: `CMD:FWD:75` (forward at 75% speed), `CMD:TURN:L:90` (turn left 90 degrees), `CMD:STOP` (immediate halt).

### 10.2 Greta Distributed Robotics Architecture

| Processor | Decision Domain | Execution Domain | Communication Role |
|---|---|---|---|
| ESP32-S3 (Brain) | AI inference, command generation, user interface | None — delegates all physical actuation | Command sender |
| Arduino Uno (Body) | None — executes commands only | Motor control, servo, obstacle response | Command receiver + sensor reporter |

### 10.3 Reliability Benefits of Distributed Architecture

- **Fault isolation:** A crash in the ESP32 AI subsystem does not automatically stop the Arduino. The Arduino implements a watchdog safety behavior — if no command is received within a defined timeout, the Arduino brings the robot to a controlled stop independently.
- **Independent restart:** Either processor can be reset without requiring the other to restart. This allows OTA firmware updates to the ESP32 while the Arduino maintains physical safety.
- **Debugging clarity:** Because each processor has a defined responsibility boundary, motor behavior issues target only the Arduino; voice command issues target only the ESP32.
- **Performance:** The Arduino executes motor control at deterministic cycle times without being interrupted by WiFi processing, AI inference, or USB communication.

---

## Section 11 — Professional Build Practices

### 11.1 Core Engineering Habits

| Engineering Habit | Mandatory Rule | Why It Matters |
|---|---|---|
| Wiring discipline | Never route wires randomly | Random wiring creates untraceable connections |
| Voltage verification | Always verify voltage before connecting a component | Connecting a 3.3V device to 5V destroys it permanently |
| Change isolation | Always test after every single change | Multiple simultaneous changes make fault isolation impossible |
| Documentation | Keep wiring documented after every session | Memory is unreliable; undocumented changes create phantom faults |
| Physical organization | Maintain clean layout at all times | Loose wires cause intermittent contacts — the hardest failures to debug |
| One change at a time | Never change wire position AND firmware simultaneously | If the problem resolves, you will not know which change fixed it |

### 11.2 Safety Rules — Non-Negotiable

> **⚠ SAFETY RULES**
>
> 1. Never wire while USB is connected. Power off first, wire, inspect, then power on.
> 2. Never connect 3.3V devices to the 5V power rail.
> 3. Never connect speaker wire to a GPIO pin — only to MAX98357A output terminals.
> 4. All GND pins of all devices must share a common ground reference.
> 5. The INMP441 microphone operates at 3.3V ONLY. 5V destroys it immediately.
> 6. Never insert SD card until all subsystem tests pass.
> 7. Discharge static by touching a grounded object before handling ESP32-S3 or camera.

### 11.3 Documentation Standard

All build sessions must be documented. Required fields for each session:
- Date and firmware version flashed
- Phase completed and result
- All hardware changes made — including wire color substitutions
- Issues encountered and root cause identified
- Fix applied and whether issue was fully resolved
- Serial monitor output for any errors or unexpected behavior
- 5-minute stability check result
- Photograph of breadboard state at end of session

---

## Section 12 — Future Expansion

Greta V2 is designed as an expandable robotics platform. The mechanical stack, power architecture, wiring standards, and firmware modular architecture all accommodate future hardware additions without requiring fundamental redesign.

### 12.1 Planned Expansion Modules

| Module | Purpose | Layer | Interface | Priority |
|---|---|---|---|---|
| Additional sensors (IR, LiDAR, ToF) | Enhanced obstacle detection and distance measurement | Layer 4 | I2C / GPIO | High |
| Navigation modules (GPS, IMU) | Position tracking for autonomous navigation | Layer 3 | I2C / UART | High |
| OTA firmware updates | Wireless firmware deployment | Software | WiFi / ESP32 | High |
| Battery upgrade (LiPo) | Extended field operation | Layer 1 | Direct power | Medium |
| Mapping sensors (LiDAR 2D) | Environment mapping for SLAM navigation | Layer 4 | UART / I2C | Medium |
| Pan/tilt servo assembly | Camera and sensor directional control | Layer 3/4 | PWM GPIO 12/13 | Medium |
| Arm or manipulator | Physical object interaction | Layer 5 (new) | PWM + servo | Future |

---

## Section 14 — Hardware List

### 14.1 Complete Component Kit

| # | Component | Qty | Voltage | Interface | Critical Notes |
|---|---|---|---|---|---|
| 1 | ESP32-S3 Dev Board | 1 | 3.3V logic / 5V USB | GPIO, I2C, I2S, SPI | Brain — place first |
| 2 | OV2640 Camera Module | 1 | 3.3V | CSI ribbon cable | Ribbon cable is fragile |
| 3 | 0.96-inch OLED (I2C) | 1 | 3.3V | I2C (SDA/SCL) | Confirm I2C address 0x3C or 0x3D |
| 4 | INMP441 I2S Microphone | 1 | 3.3V ONLY | I2S digital | NEVER connect to 5V — chip damage |
| 5 | MAX98357A I2S Amplifier | 1 | 3.3V–5V | I2S digital | Power from 3.3V rail |
| 6 | 8Ω Speaker | 1 | N/A | Direct to MAX98357A | Never wire to GPIO directly |
| 7 | Touch Sensor Module | 1 | 3.3V | Single GPIO | Optional — Phase 6+ |
| 8 | 400-point Breadboard | 1 | N/A | N/A | Main build surface |
| 9 | Jumper Wires (mixed) | 40+ | N/A | N/A | Male-to-male for breadboard |
| 10 | USB Cable | 1 | 5V | USB-C or Micro-USB | Matches your ESP32-S3 board |
| 11 | MicroSD Card | 1 | 3.3V | SPI | Insert last — after full test |
| 12 | Multimeter (recommended) | 1 | N/A | N/A | Essential for voltage verification |

> **⚡ ENGINEERING RULE**
>
> Before opening any component: discharge static electricity by touching a grounded metal object before handling the ESP32-S3 or camera module. Keep all ICs in their anti-static bags until the moment of installation. The OV2640 camera ribbon cable connector is fragile — do not flex repeatedly.

---

## Section 15 — Breadboard Basics

### 15.1 Anatomy of a 400-Point Breadboard

A 400-point breadboard has 30 numbered rows divided by a center gap, with power rails running along both long edges. Understanding the internal connection pattern is the single most important skill before wiring anything.

### 15.2 Connection Rules

| Rule | What It Means | Common Mistake |
|---|---|---|
| Row A–E are one node | Holes A1, B1, C1, D1, E1 are all internally wired together | Assuming A1 and F1 are connected — they are NOT |
| Row F–J are one node | Holes F1, G1, H1, I1, J1 are all internally wired together | Assuming a wire at E1 reaches F1 across the gap |
| Center gap is a break | No electrical connection crosses the center gap | Forgetting to jumper across the gap when needed |
| Power rail is vertical | All + holes in the left red rail are connected top to bottom | Assuming + rail on left connects to + rail on right — it does NOT |
| IC spans the gap | ESP32 pins on one side connect to A–E; other side to F–J | Placing IC without straddling gap — shorts all pins together |

### 15.3 Power Rail Wiring Sequence

```
Step 1: Connect USB 5V pin  → Left red rail (+)
Step 2: Connect USB GND     → Left blue rail (−)
Step 3: Connect ESP32 3.3V  → Right red rail (+)
Step 4: Connect ESP32 GND   → Right blue rail (−)
Step 5: Bridge left GND ↔ right GND with a jumper wire

3.3V devices: RIGHT red rail ONLY
5V left rail:  ESP32 input power ONLY
ALL GND rails must share a common ground bridge
```

### 15.4 Beginner Mistakes — Know These Before You Wire

> **⚠ SAFETY RULE**
>
> - Inserting a component into two holes in the SAME row (creates a short through the component).
> - Not straddling the IC across the center gap (shorts both sides of the chip together).
> - Connecting a 3.3V component to the 5V power rail (destroys the component instantly).
> - Assuming left and right power rails are connected (they are completely isolated).
> - Not bridging the GND rails (devices share no common ground and nothing works).
> - Wires that look inserted but are not making contact (push firmly until seated).

---

## Section 16 — ESP32-S3 Placement

### 16.1 Placement Requirements

The ESP32-S3 development board must span the center gap of the breadboard. This is not optional — it is the only correct placement. Failure to straddle the gap shorts both sides of the chip together.

```
Correct ESP32-S3 Placement — 400-point breadboard:

Row  A B C D E | GAP | F G H I J
              -+-----+-
  2  [- - - - P1] | | [P1 - - - -]  <- USB connector end (top)
  3  [- - - - P2] | | [P2 - - - -]
  4  [- - - - P3] | | [P3 - - - -]
  ...
 22  [- - - - Pn] | | [Pn - - - -]  <- Bottom pin row

- Place USB connector end toward row 1 (top of breadboard)
- Left pins seat in column E, right pins seat in column F
- Rows 1 and 23–30 remain free for peripheral wiring
- Leave rows 25–30 empty for future expansion
```

### 16.2 Placement Verification

1. Visually confirm the chip body bridges the center gap — you should see gap beneath the chip.
2. Press the board firmly and evenly. All pins must seat fully.
3. Count pins on each side — both sides should have equal pin count.
4. No pin should share a row hole with another component at this stage.

---

## Section 16A — ESP32-S3 Pin Validation

> **⚠ SAFETY RULE**
>
> Different ESP32-S3 boards have different reserved pins. Camera-integrated boards have specific GPIO pins permanently reserved for the camera CSI interface. Assigning I2S or I2C peripherals to those pins will cause conflicts, unexpected resets, or silent failures.

### 16A.1 How to Verify Your Board's Pinout

1. Find the exact model name printed on your ESP32-S3 board.
2. Search for the official pinout diagram for that specific board variant on the manufacturer's documentation page or GitHub.
3. Identify which GPIO pins are reserved for camera (typically D0–D7, VSYNC, HREF, PCLK, XCLK, SIOC, SIOD).
4. Cross-check GPIO assignments in Section 17 against your board's reserved pins. If any conflict exists, select an alternative GPIO from available free pins.
5. Mark your verified pin list in the Personal Build Notes before proceeding.

### 16A.2 Camera-Reserved Pin Ranges by Common Board Variant

| Board Variant | Typical Camera-Reserved GPIOs | Notes |
|---|---|---|
| ESP32-S3-CAM (generic) | GPIO 32–39 + 1, 2 | Verify exact map in board schematic |
| XIAO ESP32S3 Sense | GPIO 1–10 (partial) | Check Seeed Studio docs |
| Freenove ESP32-S3-CAM | GPIO 11–18 (camera data) | Check Freenove GitHub pinout |
| AI-Thinker ESP32-S3 | GPIO 32–39 | Most similar to ESP32-CAM pinout |

> Do not rely on this table alone. Always verify against your board's official pinout diagram. Board revisions can change pin assignments without changing the board name.

### 16A.3 Pins to Avoid on ALL ESP32-S3 Variants

| GPIO | Reason to Avoid |
|---|---|
| GPIO 0 | Boot mode select — using it can prevent firmware upload |
| GPIO 1, 3 | UART0 (USB serial) — needed for firmware flashing |
| GPIO 32–39 (camera boards) | Reserved for camera CSI interface |
| Any pin marked FLASH or PSRAM | Using these causes permanent damage |

### 16A.4 GPIO Verification Before Final Wiring

- Set each target GPIO in firmware as INPUT (high-impedance) and confirm no conflict at boot.
- Run the GPIO validation firmware — it reports any pin conflicts on the serial monitor.
- Only proceed to physical wiring after firmware confirms pins are free and conflict-free.

---

## Section 17 — GPIO Pin Map

> **⚡ ENGINEERING RULE**
>
> Verify these assignments against your board's pinout (Section 16A) BEFORE wiring. Do not reassign pins without checking your specific board's datasheet.

### 17.1 Recommended Pin Allocation

| GPIO | Module | Signal | Protocol | Reason for Choice |
|---|---|---|---|---|
| GPIO 8 | OLED Display | SDA (Data) | I2C | Default I2C SDA on most ESP32-S3 boards |
| GPIO 9 | OLED Display | SCL (Clock) | I2C | Default I2C SCL — pair with GPIO 8 |
| GPIO 41 | INMP441 Mic | WS (Word Select) | I2S | I2S0 WS — avoids camera pin conflicts |
| GPIO 42 | INMP441 Mic | SCK (Bit Clock) | I2S | I2S0 SCK |
| GPIO 40 | INMP441 Mic | SD (Serial Data) | I2S | I2S0 SD data in |
| GPIO 45 | MAX98357A Amp | BCLK (Bit Clock) | I2S | I2S1 BCLK — separate bus from mic |
| GPIO 46 | MAX98357A Amp | LRC (Left/Right) | I2S | I2S1 LRC |
| GPIO 47 | MAX98357A Amp | DIN (Data In) | I2S | I2S1 data output to amp |
| GPIO 2 | Touch Sensor | TOUCH signal | GPIO | Capacitive touch capable pin |
| GPIO 12 | Future Servo | PWM signal | PWM | LEDC PWM channel 0 — leave free |
| GPIO 13 | Future I/O | Reserved | — | Leave free for Phase 6+ expansion |

---

## Section 18 — Power Architecture (ESP32 Layer)

### 18.1 Beginner Power Strategy

> **ℹ INFORMATION**
>
> Use only the right-side 3.3V rail for your first build. Connect ALL 3.3V peripherals (OLED, mic, amplifier, touch sensor) exclusively to the right-side red power rail. This rail is fed from the ESP32-S3's onboard 3.3V LDO output.
>
> The left-side red rail carries 5V from USB — connecting a 3.3V device to this rail destroys it instantly. Do not bridge the left (+) rail to the right (+) rail. Only the GND (−) rails are bridged.

### 18.2 Rail Separation Reference

| Rail | Voltage | Location | Used For |
|---|---|---|---|
| Left red (+) | 5V | Left side of breadboard | ESP32-S3 VIN input ONLY |
| Left blue (−) | GND | Left side | GND connection |
| Right red (+) | 3.3V | Right side of breadboard | ALL 3.3V peripherals |
| Right blue (−) | GND | Right side | GND connection |
| GND bridge | GND | Wire across center | Connects left and right GND rails |

---

## Section 19 — Exact Wiring Map

See `docs/HARDWARE_SETUP.md` for the complete GPIO pin allocation table, testing sequence, and power architecture diagram for the Greta OS firmware layer.

---

## Section 20 — Build Sequence and Phase Testing

Follow the hardware testing sequence in `docs/HARDWARE_SETUP.md`, Section 7 (Testing Sequence). Each phase must pass its success criteria before the next phase begins.

---

## Section 21 — System Success Definition

The Greta V2 hardware build is considered complete when:

- Robot connects to dashboard within 10 seconds of boot
- Dashboard commands reach `command_processor` within 20 ms of transmission
- Telemetry frames delivered to dashboard at 10 Hz
- `STATE_PANIC` entered and motors disabled within 200 ms of health fault detection
- System runs for 30 minutes without scheduler overrun or heap warning
- All seven hardware testing steps in `docs/HARDWARE_SETUP.md` pass

---

## Section 22 — Final Checklist

Before declaring the hardware build complete, verify:

- [ ] All GND rails bridged and continuity verified with multimeter
- [ ] 3.3V on right rail measured — not assumed
- [ ] 5V and 3.3V rails NOT bridged (only GND rails are bridged)
- [ ] INMP441 microphone connected to 3.3V rail — not 5V
- [ ] Speaker connected to MAX98357A output terminals — not GPIO
- [ ] ESP32-S3 straddling breadboard center gap
- [ ] Camera ribbon cable routed flat with no sharp bends
- [ ] Motor wires bundled and routed along chassis frame perimeter
- [ ] Audio wires routed on opposite side from motor wires
- [ ] All inter-layer wiring harness secured alongside rear standoff
- [ ] Greta OS firmware flashed and boot diagnostics confirmed in serial monitor

---

## Section 23 — Personal Build Notes

Use this template to document each build session:

```
Date:
Firmware version:
Phase completed:
Phase result:
Hardware changes made:
Issues encountered:
Root cause identified:
Fix applied:
Issue fully resolved:
Serial monitor output notes:
5-minute stability check result:
Breadboard photo taken: Yes / No
```

---

*Greta V2 Hardware Specification — Rev 4.0*
*Copyright (c) 2026 Shrivardhan Jadhav*
*Licensed under Apache License 2.0*
