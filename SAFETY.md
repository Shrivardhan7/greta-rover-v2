# safety.md – Greta Rover V2

This document outlines basic safety guidelines for developing, testing, and operating the Greta V2 rover platform.

See also: [First Motor Bring-Up Procedure](docs/first_motor_bringup.md)

---

## General Safety Rules

- **Always power off motors before uploading firmware.**  
  Uploading new code while the rover is moving can cause unexpected behaviour and hardware damage.

- **Test in a controlled, open area.**  
  Keep the rover away from people, pets, and fragile objects during active development.

- **Never leave the rover unattended while powered on.**  
  Especially during early testing of new code or autonomous modes.

- **Disconnect the motor driver before bench testing sensor or communication code.**  
  This prevents accidental motor activation during debug sessions.

---

## Safe Startup Logic

Before the rover begins moving, firmware should:

1. Confirm serial communication is established between ESP32 and Arduino.
2. Confirm ultrasonic sensor is returning valid readings (not zero or max-range).
3. Apply a short startup delay (e.g. 2 seconds) before enabling motor output.
4. Set all motor outputs to STOP state on boot.

---

## Sensor Failure Handling

- If the ultrasonic sensor returns `0` or an out-of-range value consistently, treat it as a sensor fault.
- On sensor fault: halt motor output and raise a fault flag.
- Do not allow autonomous navigation to continue without a valid sensor reading.

---

## Communication Failure Handling

- The ESP32–Arduino serial link uses a heartbeat system.
- If the Arduino does not receive a heartbeat from the ESP32 within the defined timeout window, it should:
  - Stop all motors immediately.
  - Enter a safe idle state.
  - Wait for communication to resume before accepting new commands.

---

## Motor Safety

- Always use the L298N enable pins to perform a hardware-level stop if needed.
- Avoid running motors at full PWM continuously for extended periods without a heat check.
- If the rover gets physically stuck, implement a motor stall timeout in firmware.

---

## Electrical Safety

- Double-check polarity before connecting power supply.
- Use a fused power line where possible during prototyping.
- Keep breadboard connections tidy — loose jumper wires cause intermittent faults.

---

## Educational Use Disclaimer

Greta V2 is an educational robotics project. The code and hardware designs are provided **as-is** for learning purposes.

The author is **not responsible** for any damage, injury, or loss arising from the use, misuse, or modification of this project.

Always exercise proper engineering judgement when operating physical hardware.

---

*Last updated: 2026*

