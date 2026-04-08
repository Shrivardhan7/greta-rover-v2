# Development Setup

## Project

**Project Name:** Greta Rover V2  
**Architecture:** Greta OS robotics architecture  
**Status:** Long-term robotics platform under active development

This document defines the recommended development setup for the Greta Rover V2 repository. It covers the firmware and dashboard workspaces, toolchain requirements, flashing order, and safe first-boot procedure.

## 1. Project Structure Overview

```text
greta-rover-v2/
|-- arduino-motion-layer/
|   `-- motion_controller.ino
|-- esp32-brain/
|   |-- platformio.ini
|   |-- include/
|   `-- src/
|       `-- main.cpp
|-- dashboard/
|   |-- index.html
|   |-- app.js
|   |-- mode_manager.js
|   |-- mission_log.js
|   |-- voice_control.js
|   `-- style.css
`-- docs/
```

### Layer Mapping

| Path | Role |
|---|---|
| `arduino-motion-layer/` | Low-level motion control firmware |
| `esp32-brain/` | High-level control, communication, and coordination firmware |
| `dashboard/` | Operator interface for browser-based control and monitoring |
| `docs/` | Engineering procedures and technical documentation |

## 2. Recommended IDE / Runtime Per Folder

| Folder | Tool |
|---|---|
| `esp32-brain/` | VS Code with PlatformIO extension |
| `arduino-motion-layer/` | Arduino IDE |
| `dashboard/` | Web browser |

## 3. Required Software Installation

Install the following tools on the development machine:

1. **Git**  
   Required for repository cloning, branch management, and version control.
2. **Visual Studio Code**  
   Recommended editor for the ESP32 firmware workspace.
3. **PlatformIO IDE extension for VS Code**  
   Required to build, upload, and monitor the ESP32 firmware.
4. **Arduino IDE**  
   Required to build and upload the Arduino motion-layer firmware.

## 4. PlatformIO Workflow

### Open the Correct Folder

Open the `esp32-brain/` folder directly in VS Code, not the repository root, when working in PlatformIO.

### `platformio.ini` Purpose

The file `esp32-brain/platformio.ini` is the PlatformIO project configuration. In this repository it defines:

- target environment: `esp32-s3-devkitc-1`
- platform: `espressif32`
- framework: `arduino`
- serial monitor speed: `115200`
- serial monitor decoding via `esp32_exception_decoder`
- partition layout for application image handling
- library dependencies
- build flags
- upload behavior

Example configuration reference:

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
```

### Source Layout

- `esp32-brain/src/main.cpp`  
  Firmware entry point.
- `esp32-brain/src/`  
  Module implementation files.
- `esp32-brain/include/`  
  Header files for architecture modules and shared interfaces.

### Build and Upload Firmware

From the `esp32-brain/` folder:

```bash
pio run
pio run --target upload
```

Equivalent PlatformIO toolbar actions in VS Code:

- `Build`
- `Upload`

If automatic port detection fails, define `upload_port` in `platformio.ini`.

### Open the Serial Monitor

From the `esp32-brain/` folder:

```bash
pio device monitor
```

The configured monitor speed in this repository is `115200`.

## 5. Arduino Workflow

### Open the Sketch

Open the following file in Arduino IDE:

```text
arduino-motion-layer/motion_controller.ino
```

### Board and Port Selection

In Arduino IDE:

1. Select **Arduino Uno** as the board.
2. Select the correct **COM port** for the Arduino device.

### Upload Process

1. Connect the Arduino controller over USB.
2. Open `motion_controller.ino`.
3. Verify the board is set to **Arduino Uno**.
4. Verify the selected COM port matches the Arduino device.
5. Click **Upload**.
6. Wait for compile and flash completion before disconnecting power or cables.

## 6. Dashboard Workflow

### Primary Files

The browser interface is contained in:

```text
dashboard/index.html
dashboard/app.js
dashboard/mode_manager.js
dashboard/mission_log.js
dashboard/voice_control.js
dashboard/style.css
```

### Open for Local Use

For basic UI checks, open:

```text
dashboard/index.html
```

in a browser.

### GitHub Pages Usage

The dashboard is a static web interface and is compatible with GitHub Pages hosting. Use GitHub Pages when a stable browser-accessible dashboard URL is required for remote interface testing or review.

### Local Testing Options

- Open `index.html` directly in a browser for simple layout and interaction checks.
- Use a local static file server if browser security restrictions affect script loading or network behavior.

Example static server option, if Python is already installed:

```bash
python -m http.server 8000
```

Then open:

```text
http://localhost:8000/dashboard/
```

## 7. Firmware Flashing Order

Use the following sequence during initial setup or full reflash:

1. **Arduino first**  
   Flash `arduino-motion-layer/motion_controller.ino`.
2. **ESP32 second**  
   Flash the `esp32-brain/` PlatformIO project.
3. **Dashboard last**  
   Open or deploy the dashboard after firmware targets are programmed.

This order brings up the low-level motion layer before the higher-level control layer.

## 8. First Boot Connection Steps

Use a controlled bench setup for the first integrated boot.

1. Confirm motors cannot drive the rover unexpectedly. Remove wheel load or isolate the drivetrain before first power-up.
2. Flash the Arduino motion layer.
3. Flash the ESP32 brain layer.
4. Power the Arduino and verify it reaches an idle, non-moving state.
5. Power the ESP32 and confirm it boots cleanly.
6. Open the dashboard in the browser.
7. Confirm the dashboard can establish the intended control or telemetry session with the ESP32.
8. Verify that no motion command is active at boot.
9. Test the emergency stop path before any motion command is issued.
10. Perform only short, controlled manual tests after link stability is confirmed.

## 9. OTA Update Concept

Over-the-air update support is an architectural concept for the ESP32 brain layer. The purpose of OTA is to allow firmware updates without a direct USB flashing session.

General OTA model:

1. The ESP32 receives a new firmware image over a network link.
2. The image is written to an available flash slot.
3. The firmware is validated.
4. The ESP32 reboots into the updated image.

In this repository, OTA should be treated as a future update path for the ESP32 layer. It does not replace the Arduino upload workflow.

## 10. Development Workflow Diagram

```text
[Dashboard]
Interface Layer
      |
      v
[ESP32]
Brain Layer
      |
      v
[Arduino]
Motion Layer
```

## 11. Troubleshooting

### ESP32 Not Connecting

- Verify the USB cable supports data, not power-only operation.
- Confirm the correct ESP32 board environment is selected in `platformio.ini`.
- Check that the correct serial port is available and not already open in another tool.
- Reset the ESP32 and retry upload or monitor connection.

### WebSocket Failure

- Confirm the ESP32 firmware is running and reachable by the dashboard.
- Verify the dashboard is using the correct endpoint or connection target.
- Check browser developer tools for connection errors.
- If testing locally, use a static server rather than relying only on direct file opening.

### Upload Failure

- Recheck the selected COM port.
- Disconnect and reconnect the device, then retry.
- Close any open serial monitor before uploading.
- For PlatformIO, set `upload_port` manually if automatic detection is unreliable.

### Wrong COM Port

- Disconnect the device and note which port disappears.
- Reconnect the device and note which port reappears.
- Select that port in Arduino IDE or PlatformIO.

## 12. Safety Notes

- Do not run motors during flashing.
- Disconnect wheels from active drive testing during initial bench validation.
- Test the emergency stop path before any functional motion test.
- Keep the rover in a controlled area during first boot and firmware bring-up.
- Stop testing immediately if motion occurs without command.

## Quick Reference

```text
ESP32 firmware   -> Open esp32-brain/ in VS Code + PlatformIO
Arduino firmware -> Open arduino-motion-layer/motion_controller.ino in Arduino IDE
Dashboard        -> Open dashboard/index.html in a browser
Flash order      -> Arduino -> ESP32 -> Dashboard
```
