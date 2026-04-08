// Greta Rover OS
// Copyright (c) 2026 Shrivardhan Jadhav
// SPDX-License-Identifier: Apache-2.0
// Licensed under Apache License 2.0

// ══════════════════════════════════════════════════════════════════════════════
//  GRETA V2 — app.js
//  Dashboard core — WebSocket, telemetry, face engine, controls, event log
//
//  Architecture:
//    ws          — WebSocket lifecycle (connect, auto-reconnect, send)
//    telem       — Telemetry frame rendering
//    face        — Robot face expression engine
//    controls    — D-pad buttons + keyboard input + safety ESTOP hooks
//    log         — Event log (ring buffer, max LOG_MAX entries)
//    theme       — Dark / light with localStorage persistence
//
//  Dependencies: none (vanilla JS, ES2017, no frameworks)
//  Load order:   app.js → mode_manager.js → voice_control.js → mission_log.js
//
//  Safety notes:
//    - ESTOP is sent automatically on tab hide and page unload (CFG.ESTOP_ON_BLUR)
//    - All commands are dropped (not queued) when the WebSocket is not open
//    - cmd_send and ws_send are exposed globally so other modules can call them
// ══════════════════════════════════════════════════════════════════════════════

'use strict';

// ─── Config ───────────────────────────────────────────────────────────────────
// All tunable values live here — never use magic numbers in the code below.
const CFG = Object.freeze({
  DEFAULT_HOST:     'greta.local',   // mDNS hostname; user can override via input
  WS_PORT:          81,              // WebSocket port — must match ESP32 config
  RECONNECT_DELAY:  3000,            // ms between auto-reconnect attempts
  LOG_MAX:          30,              // max event log entries kept in memory
  OBSTACLE_HIDE_MS: 5000,           // ms before obstacle banner auto-hides
  ESTOP_ON_BLUR:    true,           // send ESTOP when tab loses focus
});

const STORAGE_KEYS = Object.freeze({
  HOST:  'greta_host',
  THEME: 'greta_theme',
});

const THEMES = Object.freeze({
  DARK:     'Greta Dark',
  LIGHT:    'Greta Light',
  ADAPTIVE: 'Greta Adaptive',
});

const THEME_SEQUENCE = Object.freeze([
  THEMES.DARK,
  THEMES.LIGHT,
  THEMES.ADAPTIVE,
]);

const THEME_CLASSES = Object.freeze([
  'theme-greta-dark',
  'theme-greta-light',
]);

const THEME_META_COLORS = Object.freeze({
  [THEMES.DARK]:  '#0a0c10',
  [THEMES.LIGHT]: '#f0f2f5',
});

// ─── WebSocket state ──────────────────────────────────────────────────────────
let _ws             = null;
let _wsConnected    = false;
let _reconnectTimer = null;
let _manualHost     = '';           // last host entered by user; overrides DEFAULT_HOST

// ─── UI state ─────────────────────────────────────────────────────────────────
let _obstacleTimer = null;
let _logEntries    = [];
let _robotState    = 'OFFLINE';
let _themeChoice   = THEMES.DARK;
let _themeQuery    = null;
let _lastFaceClass = 'offline';

// ─── DOM refs — resolved once at init to avoid repeated getElementById calls ──
let dom = {};

function _resolve_dom() {
  const ids = [
    'themeToggle', 'wsDot', 'wsLabel', 'wsAddr',
    'ipInput', 'connectBtn',
    'telState', 'telWifi', 'telBt', 'telUptime', 'telLastCmd',
    'telRssi', 'telLatency',
    'robotFace', 'faceStateLabel', 'eyeL', 'eyeR', 'faceM',
    'logScroll', 'logEmpty', 'clearLog',
    'obstacleBanner',
    'btnF', 'btnB', 'btnL', 'btnR', 'btnStop',
  ];
  ids.forEach(id => {
    const el = document.getElementById(id);
    if (el) dom[id] = el;
  });
}

// ══════════════════════════════════════════════════════════════════════════════
//  THEME
// ══════════════════════════════════════════════════════════════════════════════

function theme_toggle() {
  const idx = THEME_SEQUENCE.indexOf(_themeChoice);
  const nextIdx = idx === -1 ? 0 : (idx + 1) % THEME_SEQUENCE.length;
  theme_apply(THEME_SEQUENCE[nextIdx]);
}

function _storage_get(key) {
  try {
    return localStorage.getItem(key);
  } catch (_) {
    return null;
  }
}

function _storage_set(key, value) {
  try {
    localStorage.setItem(key, value);
  } catch (_) {
    // Storage may be unavailable in privacy-restricted contexts.
  }
}

function _theme_normalize(raw) {
  switch (raw) {
    case 'dark':
    case THEMES.DARK:
      return THEMES.DARK;
    case 'light':
    case THEMES.LIGHT:
      return THEMES.LIGHT;
    case 'adaptive':
    case THEMES.ADAPTIVE:
      return THEMES.ADAPTIVE;
    default:
      return THEMES.DARK;
  }
}

function _theme_resolve(choice) {
  if (choice !== THEMES.ADAPTIVE || !_themeQuery) return choice;
  return _themeQuery.matches ? THEMES.DARK : THEMES.LIGHT;
}

function _theme_class_name(choice) {
  return choice === THEMES.LIGHT ? 'theme-greta-light' : 'theme-greta-dark';
}

function theme_apply(choice) {
  _themeChoice = _theme_normalize(choice || _themeChoice);

  const resolvedTheme = _theme_resolve(_themeChoice);
  document.body.classList.remove(...THEME_CLASSES);
  document.body.classList.add(_theme_class_name(resolvedTheme));
  document.body.dataset.themeChoice = _themeChoice;

  const metaThemeColor = document.querySelector('meta[name="theme-color"]');
  if (metaThemeColor) {
    metaThemeColor.setAttribute('content', THEME_META_COLORS[resolvedTheme]);
  }

  _storage_set(STORAGE_KEYS.THEME, _themeChoice);
}

function theme_restore() {
  theme_apply(_storage_get(STORAGE_KEYS.THEME));
}

function theme_bind_system() {
  if (!window.matchMedia) return;

  _themeQuery = window.matchMedia('(prefers-color-scheme: dark)');
  const onThemeChange = () => {
    if (_themeChoice === THEMES.ADAPTIVE) {
      theme_apply(_themeChoice);
    }
  };

  if (typeof _themeQuery.addEventListener === 'function') {
    _themeQuery.addEventListener('change', onThemeChange);
  } else if (typeof _themeQuery.addListener === 'function') {
    _themeQuery.addListener(onThemeChange);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  WEBSOCKET
// ══════════════════════════════════════════════════════════════════════════════

function ws_connect() {
  const host = _manualHost || (dom.ipInput ? dom.ipInput.value.trim() : '') || CFG.DEFAULT_HOST;
  const url  = `ws://${host}:${CFG.WS_PORT}/ws`;

  _ws_set_status('connecting', 'Connecting…');
  if (dom.wsAddr) dom.wsAddr.textContent = url;

  // Tear down any existing socket before opening a new one
  if (_ws) {
    _ws.onclose = null;
    _ws.onerror = null;
    try { _ws.close(); } catch (_) {}
    _ws = null;
  }

  _ws = new WebSocket(url);

  _ws.onopen = () => {
    _wsConnected = true;
    _ws_set_status('connected', 'Connected');
    log_add('WebSocket connected', 'ev-connect');
    clearTimeout(_reconnectTimer);
    _reconnectTimer = null;
    if (typeof cmd_send === 'function') cmd_send('STOP', dom.btnStop);
  };

  // Route all inbound messages through ws_on_message.
  // mode_manager.js wraps this function after load to intercept MODE events.
  _ws.onmessage = evt => ws_on_message(evt.data);

  _ws.onerror = () => {
    log_add('WebSocket error', 'ev-error');
  };

  _ws.onclose = () => {
    _wsConnected = false;
    _ws_set_status('disconnected', 'Disconnected — retrying…');
    log_add('Disconnected', 'ev-error');
    face_set('OFFLINE');
    _schedule_reconnect();
  };
}

function _schedule_reconnect() {
  clearTimeout(_reconnectTimer);
  _reconnectTimer = setTimeout(ws_connect, CFG.RECONNECT_DELAY);
}

function _ws_set_status(state, label) {
  if (dom.wsLabel) dom.wsLabel.textContent = label;
  if (dom.wsDot) {
    dom.wsDot.className = 'conn-status-dot'
      + (state === 'connected'  ? ' connected'  : '')
      + (state === 'connecting' ? ' connecting' : '');
  }
}

// ws_send — global so mode_manager and voice_control can send commands.
// Returns true if sent, false if socket not open.
// Commands are dropped (not queued) when disconnected — this is intentional:
// a queued command delivered seconds later could cause unexpected robot motion.
function ws_send(msg) {
  if (_ws && _ws.readyState === WebSocket.OPEN) {
    _ws.send(msg);
    return true;
  }
  return false;
}

// ══════════════════════════════════════════════════════════════════════════════
//  MESSAGE HANDLER
//  ws_on_message is exposed globally so mode_manager.js can wrap it.
// ══════════════════════════════════════════════════════════════════════════════

function ws_on_message(raw) {
  if (typeof raw !== 'string') return;

  const text = raw.trim();
  if (!text) return;

  if (text.startsWith('{')) {
    try {
      const d = JSON.parse(text);

      // ── Event frames (robot → dashboard) ──────────────────────────────────
      if (d.event) {
        switch (d.event) {
          case 'OBSTACLE':
            obstacle_show();
            log_add('OBSTACLE DETECTED — motors stopped', 'ev-obstacle');
            break;
          case 'PING':
            // Server heartbeat probe — reply immediately to avoid SAFE state
            ws_send('PONG');
            break;
          case 'PONG':
            // Reply to a client-initiated ping — reserved for future latency tracking
            break;
          default:
            // Unknown event types are silently ignored.
            // mode_manager.js handles MODE_REJECTED / MODE_ACK before this point.
            break;
        }
        return;
      }

      // ── Telemetry frame (has a 'state' field) ─────────────────────────────
      if (d.state) {
        telem_update(d);
        return;
      }

    } catch (_) {
      log_add('Malformed JSON received', 'ev-error');
      return;
    }
  }

  // Plain-text frames — ACKs from the Arduino via the ESP32 bridge
  log_add(text, 'ev-ack');
}

// ══════════════════════════════════════════════════════════════════════════════
//  TELEMETRY
// ══════════════════════════════════════════════════════════════════════════════

function telem_update(d) {
  _robotState = (d.state || 'UNKNOWN').toUpperCase();

  _set_text('telState',   _robotState,    'accent');
  _set_text('telWifi',    d.wifi   || '—', d.wifi   === 'OK' ? 'ok' : 'error');
  _set_text('telBt',      d.bt     || '—', d.bt     === 'OK' ? 'ok' : 'error');
  _set_text('telLastCmd', d.lastCmd || '—', '');

  // Format uptime as HH:MM:SS
  const up = parseInt(d.uptime, 10);
  if (!isNaN(up)) {
    const hh = String(Math.floor(up / 3600)).padStart(2, '0');
    const mm = String(Math.floor((up % 3600) / 60)).padStart(2, '0');
    const ss = String(up % 60).padStart(2, '0');
    _set_text('telUptime', `${hh}:${mm}:${ss}`, '');
  }

  // RSSI — colour-coded by signal quality
  if (d.rssi !== undefined && dom.telRssi) {
    const rssiClass = d.rssi > -60 ? 'ok' : d.rssi > -75 ? 'warn' : 'error';
    _set_text('telRssi', `${d.rssi} dBm`, rssiClass);
  }

  // Round-trip latency — colour-coded by threshold
  if (d.latencyMs !== undefined && dom.telLatency) {
    const latClass = d.latencyMs < 100 ? 'ok' : d.latencyMs < 300 ? 'warn' : 'error';
    _set_text('telLatency', `${d.latencyMs} ms`, latClass);
  }

  face_set(_robotState);
}

// Set text content and colour class on a telemetry value element
function _set_text(id, text, colorClass) {
  const el = dom[id];
  if (!el) return;

  const nextText = String(text);
  const nextClass = 'telem-value' + (colorClass ? ` ${colorClass}` : '');

  if (el.textContent !== nextText) el.textContent = nextText;
  if (el.className !== nextClass) el.className = nextClass;
}

// ══════════════════════════════════════════════════════════════════════════════
//  FACE ENGINE
//  The robot face reflects the FSM state reported in telemetry.
//  CSS handles all animation — face_set only adds/removes class names.
// ══════════════════════════════════════════════════════════════════════════════

// All valid face state CSS classes — used to clear previous state
const FACE_STATES = ['ready', 'moving', 'safe', 'error', 'offline', 'connecting'];

function face_set(state) {
  if (!dom.robotFace) return;

  // Map FSM state name → CSS class
  const stateClassMap = {
    'READY':      'ready',
    'MOVING':     'moving',
    'SAFE':       'safe',
    'ERROR':      'error',
    'CONNECTING': 'connecting',
  };

  const nextState = state || 'OFFLINE';
  const cssClass = stateClassMap[nextState] || 'offline';

  if (dom.faceStateLabel && dom.faceStateLabel.textContent !== nextState) {
    dom.faceStateLabel.textContent = nextState;
  }

  if (_lastFaceClass === cssClass && dom.robotFace.classList.contains(cssClass)) return;

  dom.robotFace.classList.remove(...FACE_STATES);
  dom.robotFace.classList.add(cssClass);
  _lastFaceClass = cssClass;
}

// ══════════════════════════════════════════════════════════════════════════════
//  MOVEMENT CONTROLS
// ══════════════════════════════════════════════════════════════════════════════

// cmd_send — global so mode_manager.js can wrap it to enforce mode gating.
// STOP and ESTOP always bypass mode gating (handled in mode_manager).
function cmd_send(cmd, btnEl) {
  if (!cmd) return false;

  if (!ws_send(cmd)) {
    log_add('Not connected — command dropped', 'ev-error');
    return false;
  }
  log_add(cmd, 'ev-command');
  _btn_flash(btnEl);
  return true;
}

function _btn_flash(btnEl) {
  if (!btnEl) return;
  btnEl.classList.add('pressed');
  setTimeout(() => btnEl.classList.remove('pressed'), 180);
}

function controls_bind_dpad() {
  document.querySelectorAll('.dpad-btn').forEach(btn => {
    const cmd = btn.dataset.cmd;
    if (!cmd) return;

    // pointerdown rather than click for lower latency on touch devices
    btn.addEventListener('pointerdown', e => {
      e.preventDefault();
      cmd_send(cmd, btn);
    });

    // Keyboard activation for users navigating with Tab + Enter/Space
    btn.addEventListener('keydown', e => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        cmd_send(cmd, btn);
      }
    });
  });
}

// Keyboard command map — WASD + arrow keys + space + E (ESTOP)
const KEY_MAP = {
  'ArrowUp':    { cmd: 'MOVE F', id: 'btnF'    },
  'ArrowDown':  { cmd: 'MOVE B', id: 'btnB'    },
  'ArrowLeft':  { cmd: 'MOVE L', id: 'btnL'    },
  'ArrowRight': { cmd: 'MOVE R', id: 'btnR'    },
  ' ':          { cmd: 'STOP',   id: 'btnStop' },
  'w':          { cmd: 'MOVE F', id: 'btnF'    },
  's':          { cmd: 'MOVE B', id: 'btnB'    },
  'a':          { cmd: 'MOVE L', id: 'btnL'    },
  'd':          { cmd: 'MOVE R', id: 'btnR'    },
  'e':          { cmd: 'ESTOP',  id: 'btnStop' }, // Emergency stop
};

function controls_bind_keyboard() {
  document.addEventListener('keydown', e => {
    // Do not intercept keystrokes while the user is typing in the IP input
    if (document.activeElement === dom.ipInput) return;
    const m = KEY_MAP[e.key];
    if (!m) return;
    e.preventDefault();
    cmd_send(m.cmd, dom[m.id]);
  });
}

// Safety: send ESTOP when the browser tab is hidden or the page is unloaded.
// This prevents the robot from continuing to move if the operator loses the page.
function controls_bind_visibility() {
  if (!CFG.ESTOP_ON_BLUR) return;

  // Page Visibility API — fires when tab is backgrounded or phone screen locks
  document.addEventListener('visibilitychange', () => {
    if (document.hidden && _wsConnected) {
      ws_send('ESTOP');
      log_add('Tab hidden — ESTOP sent', 'ev-error');
    }
  });

  // beforeunload — fires on tab close or page navigation
  window.addEventListener('beforeunload', () => {
    if (_wsConnected) ws_send('ESTOP');
  });
}

// ══════════════════════════════════════════════════════════════════════════════
//  OBSTACLE BANNER
// ══════════════════════════════════════════════════════════════════════════════

function obstacle_show() {
  if (!dom.obstacleBanner) return;
  dom.obstacleBanner.classList.add('visible');
  clearTimeout(_obstacleTimer);
  _obstacleTimer = setTimeout(() => {
    dom.obstacleBanner.classList.remove('visible');
  }, CFG.OBSTACLE_HIDE_MS);
}

// ══════════════════════════════════════════════════════════════════════════════
//  EVENT LOG
//  log_add is exposed globally — mode_manager.js and mission_log.js wrap it.
// ══════════════════════════════════════════════════════════════════════════════

function log_add(msg, cssClass) {
  const ts = new Date().toLocaleTimeString('en-GB', { hour12: false });
  _logEntries.unshift({ ts, msg, cssClass });
  if (_logEntries.length > CFG.LOG_MAX) _logEntries.length = CFG.LOG_MAX;
  log_render();
}

function log_render() {
  if (!dom.logScroll) return;

  // Remove existing rendered entries before re-rendering
  dom.logScroll.querySelectorAll('.log-entry').forEach(el => el.remove());

  if (_logEntries.length === 0) {
    if (dom.logEmpty) dom.logEmpty.style.display = '';
    return;
  }
  if (dom.logEmpty) dom.logEmpty.style.display = 'none';

  // Build all entries in a fragment to minimise DOM reflows
  const frag = document.createDocumentFragment();
  _logEntries.forEach(entry => {
    const row = document.createElement('div');
    row.className = `log-entry ${entry.cssClass || ''}`;

    const ts = document.createElement('span');
    ts.className   = 'log-ts';
    ts.textContent = entry.ts;

    const msg = document.createElement('span');
    msg.className   = 'log-msg';
    msg.textContent = entry.msg;

    row.appendChild(ts);
    row.appendChild(msg);
    frag.appendChild(row);
  });

  dom.logScroll.appendChild(frag);
  dom.logScroll.scrollTop = dom.logScroll.scrollHeight;
}

// ══════════════════════════════════════════════════════════════════════════════
//  INIT
// ══════════════════════════════════════════════════════════════════════════════

function init() {
  _resolve_dom();
  theme_bind_system();
  theme_restore();

  // Restore last-used host so the operator does not need to retype it
  const savedHost = _storage_get(STORAGE_KEYS.HOST);
  if (savedHost && dom.ipInput) dom.ipInput.value = savedHost;

  // Theme toggle button
  if (dom.themeToggle) {
    dom.themeToggle.addEventListener('click', theme_toggle);
  }

  // Connect button — saves host for next session
  if (dom.connectBtn) {
    dom.connectBtn.addEventListener('click', () => {
      const h = dom.ipInput ? dom.ipInput.value.trim() : '';
      if (h) {
        _manualHost = h;
        _storage_set(STORAGE_KEYS.HOST, h);
      }
      clearTimeout(_reconnectTimer);
      ws_connect();
    });
  }

  // Clear event log
  if (dom.clearLog) {
    dom.clearLog.addEventListener('click', () => {
      _logEntries = [];
      log_render();
    });
  }

  controls_bind_dpad();
  controls_bind_keyboard();
  controls_bind_visibility();

  face_set('OFFLINE');
  log_add('Dashboard ready — Greta V2', 'ev-connect');

  // Auto-connect on load using last saved host
  ws_connect();
}

document.addEventListener('DOMContentLoaded', init);
