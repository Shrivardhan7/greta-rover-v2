# Greta V2 — Upgrade Integration Guide

## Section 5: Safe Testing Steps

Test each module independently before combining. If anything breaks, you
have isolated which addition caused it because nothing in the existing files
was changed.

---

### Step 1 — CSS integration (no functional risk)

Append `css_additions.css` content to the end of `style.css`.
Open `index.html` in a browser with no WebSocket connection.
Verify: page still loads, existing panels unchanged, no layout break.

---

### Step 2 — HTML additions

Copy the three panel blocks from `html_additions.html` into `index.html`
at the marked insertion points. Do NOT add the script tags yet.
Verify: panels appear with correct styling, no JS errors in console.

---

### Step 3 — Mission log only

Add only `<script src="mission_log.js"></script>` before `</body>`.
Verify:
  - Mission log panel populates when events fire
  - Existing event log still works independently
  - CLEAR button works
  - No errors in browser console

---

### Step 4 — Mode manager

Add `<script src="mode_manager.js"></script>` BEFORE mission_log.js.
Verify:
  - Mode buttons switch the badge and description
  - MANUAL mode: D-pad works normally
  - AUTONOMOUS mode: D-pad movement buttons are visually blocked
  - SAFE mode: D-pad blocked, STOP still works
  - Mode changes appear in both logs
  - Sending "MODE MANUAL" via WebSocket inspector succeeds

**ESP32 note:** The `mode_manager.cpp` module can be integrated first with
mode_receive() as a no-op stub. Mode enforcement only becomes active once
`mode_manager.h` is included in `command_processor.cpp`. Add it there last.

---

### Step 5 — Voice control

Add `<script src="voice_control.js"></script>` AFTER mode_manager.js.
Voice panel is only visible when VOICE mode is active.

Test sequence:
  1. Switch to VOICE mode
  2. Click mic button
  3. Say "forward" — verify "MOVE F" appears in log
  4. Say "stop" — verify "STOP" is sent
  5. Switch back to MANUAL — confirm D-pad still works

Browser requirements:
  - Chrome / Edge on desktop: full support
  - Safari iOS 14.5+: supported but requires HTTPS
  - Firefox: NOT supported (no Web Speech API)

---

### Step 6 — ESP32 firmware

Integration order for existing firmware files:

**main.cpp** — add two lines:
```cpp
// In setup(), after command_init():
mode_init();

// In loop(), after state_update():
mode_update();
```

**network_manager.cpp** — in _ws_event(), WStype_TEXT case, add before _cmdCb:
```cpp
if (length >= 5 && strncmp(p, "MODE ", 5) == 0) {
    mode_receive(p + 5);
    return;
}
```

**command_processor.cpp** — in command_receive(), add before state gate:
```cpp
// Block movement in SAFE mode
if (mode_get() == MODE_SAFE && !_is_stop_cmd(cmd)) return;
// Block manual movement in AUTONOMOUS mode
if (mode_get() == MODE_AUTONOMOUS && _is_move_cmd(cmd)) return;
```

**telemetry.cpp** — in telemetry_build(), add one field:
```cpp
doc["mode"] = mode_name();
```

**telem_update in app.js** — add one line to handle the new field:
```js
if (d.mode) _set_text('telMode', d.mode, '');  // add a telMode telem item to HTML
```

---

## Section 6: OTA Firmware Update — Preparation

### Architecture recommendation

Do not implement OTA as a monolithic feature. Structure it in three phases:

**Phase 1 — Foundation (implement now)**

The `platformio.ini` already specifies `min_spiffs.csv` partitions which
include two OTA app slots. No partition change needed.

Add to `network_manager.cpp` after WiFi connects:
```cpp
#include <ArduinoOTA.h>

// In _on_wifi_up():
ArduinoOTA.setHostname(MDNS_HOSTNAME);
ArduinoOTA.setPassword("greta_ota_pass");  // Move to config.h
ArduinoOTA.begin();

// In network_update(), after _ws.loop():
ArduinoOTA.handle();
```

This enables PlatformIO CLI OTA (`pio run -t upload --upload-port greta.local`)
with zero dashboard changes.

**Phase 2 — Dashboard OTA trigger (future)**

Add a firmware upload panel to the dashboard that posts a binary via HTTP.
The ESP32 runs a simple `WebServer` on port 80 alongside the WebSocket on 81.
Endpoint: `POST /ota` receives the binary, calls `Update.begin()` / `Update.write()`.
State machine transitions to `STATE_ERROR` during flash to block all commands.

**Phase 3 — Staged rollout (future)**

Add a version check: on boot, ESP32 broadcasts current version in telemetry.
Dashboard compares against a `version.json` hosted on GitHub Pages.
If newer: show an update available badge. User initiates manually.

### OTA safety rules

1. OTA must be rejected in `STATE_MOVING`. Only allowed in `STATE_READY`.
2. Bluetooth bridge must be stopped before flashing (UART2 interference risk).
3. After flash, send `{"event":"OTA_DONE"}` before reboot so dashboard can
   display a "Rebooting…" message instead of showing a disconnection error.
4. Failed flash must not brick the device. Use `Update.hasError()` check and
   fall back to current partition automatically (handled by ESP-IDF bootloader).

---

## Future Upgrade Roadmap

| Priority | Feature                         | Effort | Prerequisite        |
|----------|---------------------------------|--------|---------------------|
| High     | ESP32 mode_manager integration  | 1 day  | Firmware build env  |
| High     | Telemetry `mode` field          | 1 hr   | mode_manager        |
| Medium   | OTA Phase 1 (CLI upload)        | 2 hrs  | WiFi stable         |
| Medium   | Distance sensor telem field     | 2 hrs  | Arduino → ESP32 ACK |
| Medium   | Voice language toggle (Marathi) | 1 hr   | voice_control.js    |
| Low      | Autonomous obstacle avoidance   | 1 week | mode_manager        |
| Low      | OTA Phase 2 (dashboard upload)  | 3 days | OTA Phase 1         |
| Low      | Mission log export to file      | 2 hrs  | mission_log.js      |
| Low      | OLED face display driver        | 3 days | face_engine.h stub  |
