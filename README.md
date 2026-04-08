# Greta Rover V2

A self-engineered modular robotics platform combining embedded systems, AI processing, and mobile control.  
Built as a learning and portfolio project in embedded systems architecture and robotics engineering.

See also: [First Motor Bring-Up Procedure](docs/first_motor_bringup.md)

---

## Overview

Greta V2 is a two-layer rover platform:

- **Arduino Uno** handles low-level motor control and sensor reading.
- **ESP32-S3** acts as the AI and communication brain.
- A **web dashboard** provides real-time control and telemetry.

The architecture is intentionally modular — each subsystem is independent and can be developed or replaced without breaking the rest.

---

## Key Features

- Dual-microcontroller architecture (Arduino + ESP32-S3)
- Bluetooth-based mobile control
- Obstacle avoidance via ultrasonic sensor
- Real-time web dashboard for monitoring and control
- Modular codebase designed for incremental upgrades
- Voice activation capability (in development)

---

## System Architecture

```
[ Mobile / Browser Dashboard ]
           |
      (Bluetooth / WiFi)
           |
     [ ESP32-S3 Brain ]
      AI | Comms | Logic
           |
     [ Arduino Uno ]
     Motor | Sensor Control
           |
   [ L298N Motor Driver ]
     Left Motor | Right Motor
```

Communication between ESP32 and Arduino uses a structured serial command protocol with acknowledgement (ACK) and heartbeat monitoring.

---

## Hardware Stack

| Component | Role |
|---|---|
| Arduino Uno | Motor control, sensor interface |
| ESP32-S3 | AI processing, Bluetooth/WiFi comms |
| L298N Motor Driver | Dual DC motor control |
| HC-SR04 Ultrasonic Sensor | Obstacle detection |
| DC Geared Motors (x2) | Differential drive |
| Portable Power Supply | System power |
| Breadboard | Prototyping and connections |

---

## Software Stack

| Layer | Technology |
|---|---|
| Arduino firmware | C++ / Arduino framework |
| ESP32 firmware | C++ / Arduino framework (PlatformIO) |
| Web dashboard | HTML, CSS, JavaScript |
| Serial protocol | Custom command + ACK protocol |

---

## Repository Structure

```
greta-rover-v2/
├── arduino-motion-layer/   # Arduino Uno motor + sensor firmware
├── esp32-brain/            # ESP32-S3 AI + comms firmware
├── dashboard/        # Web control interface
├── docs/             # Technical documentation
├── version           # Current version tag
├── LICENSE           # Apache License 2.0
├── NOTICE            # Third-party notices
├── safety.md               # Safety guidelines
└── readme.md               # This file
```

---

## Development Status

**Active Development — Work in Progress**

| Module | Status |
|---|---|
| Arduino motor control | ✅ Working |
| Bluetooth control | ✅ Working |
| Obstacle avoidance | ✅ Working |
| Serial command protocol | ✅ Working |
| Web dashboard | 🔧 In Progress |
| ESP32 AI integration | 🔧 In Progress |
| Voice activation | 📋 Planned |

---

## Future Roadmap

- [ ] Computer vision (ESP32-S3 camera module)
- [ ] Autonomous navigation with path planning
- [ ] Sensor fusion (IMU + ultrasonic)
- [ ] AI voice assistant integration
- [ ] Custom PCB design (replacing breadboard prototype)
- [ ] OTA firmware updates via ESP32

---

## Safety Notice

This project involves moving mechanical parts and electrical components.

- Always test firmware changes in a controlled environment.
- Ensure motors are powered off before uploading new firmware.
- Do not operate the rover near people or fragile objects during development.
- Implement and test emergency stop logic before autonomous mode.
- Review `safety.md` before running the rover.

This project is intended for **educational use only**. The author is not responsible for any damage or injury arising from use of this codebase.

---

## Author

**Shrivardhan Jadhav**  
Mechanical Engineer | Embedded Systems | Robotics | AI  
GitHub: [Shrivardhan7](https://github.com/Shrivardhan7)

---

## License

Licensed under the **Apache License 2.0**.  
See [LICENSE](./LICENSE) for details.  
See [NOTICE](./NOTICE) for third-party acknowledgements.


