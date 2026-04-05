// ══════════════════════════════════════════════════════════════════════════════
//  GRETA V2 — Dashboard  app.js
//  Vanilla JS — no frameworks — ES2017 — works on iOS / Android / Desktop
//
//  Architecture:
//    ws          — WebSocket lifecycle (connect, reconnect, send)
//    telem       — Telemetry state rendering
//    face        — Robot face expression engine
//    controls    — D-pad + keyboard input
//    heartbeat   — PING/PONG watchdog (responds to server PING)
//    log         — Event log (ring buffer, max LOG_MAX entries)
//    theme       — Dark/light with localStorage persistence
// ══════════════════════════════════════════════════════════════════════════════

'use strict';

// ─── Config ───────────────────────────────────────────────────────────────────
const CFG = Object.freeze({
  DEFAULT_HOST:     'greta.local',
  WS_PORT:          81,
  RECONNECT_DELAY:  3000,
  LOG_MAX:          30,
  OBSTACLE_HIDE_MS: 5000,
  ESTOP_ON_BLUR:    true,    // Send ESTOP when tab loses focus
});

// ─── WebSocket state ──────────────────────────────────────────────────────────
let _ws             = null;
let _wsConnected    = false;
let _reconnectTimer = null;
let _manualHost     = '';    // Set by user; overrides DEFAULT_HOST

// ─── UI state ─────────────────────────────────────────────────────────────────
let _obstacleTimer  = null;
let _logEntries     = [];
let _robotState     = 'OFFLINE';
let _rssi           = null;
let _latency        = null;

// ─── DOM refs (resolved once at init) ────────────────────────────────────────
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
  const isDark = document.body.classList.contains('dark');
  document.body.classList.toggle('dark',  !isDark);
  document.body.classList.toggle('light',  isDark);
  localStorage.setItem('greta_theme', isDark ? 'light' : 'dark');
}

function theme_apply() {
  const t = localStorage.getItem('greta_theme') || 'dark';
  document.body.className = t;
}

// ══════════════════════════════════════════════════════════════════════════════
//  WEBSOCKET
// ══════════════════════════════════════════════════════════════════════════════

function ws_connect() {
  const host = _manualHost || dom.ipInput.value.trim() || CFG.DEFAULT_HOST;
  const url  = `ws://${host}:${CFG.WS_PORT}/ws`;

  _ws_set_status('connecting', `Connecting…`);
  dom.wsAddr.textContent = url;

  // Tear down previous socket cleanly
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
  };

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
      + (state === 'connected'   ? ' connected'   : '')
      + (state === 'connecting'  ? ' connecting'  : '');
  }
}

function ws_send(msg) {
  if (_ws && _ws.readyState === WebSocket.OPEN) {
    _ws.send(msg);
    return true;
  }
  return false;
}

// ══════════════════════════════════════════════════════════════════════════════
//  MESSAGE HANDLER
// ══════════════════════════════════════════════════════════════════════════════

function ws_on_message(raw) {
  const text = raw.trim();

  if (text.startsWith('{')) {
    try {
      const d = JSON.parse(text);

      // ── Event frames ──────────────────────────────────────────────────────
      if (d.event) {
        switch (d.event) {
          case 'OBSTACLE':
            obstacle_show();
            log_add('OBSTACLE DETECTED — motors stopped', 'ev-obstacle');
            break;
          case 'PING':
            // Server heartbeat — reply immediately
            ws_send('PONG');
            break;
          case 'PONG':
            // Server replied to our ping (future: latency measurement)
            break;
        }
        return;
      }

      // ── Telemetry frame ───────────────────────────────────────────────────
      if (d.state) telem_update(d);
      return;
    } catch (_) {
      log_add('Malformed JSON received', 'ev-error');
      return;
    }
  }

  // Plain-text frames (ACKs) — log only, not used for state decisions
  const cls = text.startsWith('ACK') ? 'ev-ack' : 'ev-ack';
  log_add(text, cls);
}

// ══════════════════════════════════════════════════════════════════════════════
//  TELEMETRY
// ══════════════════════════════════════════════════════════════════════════════

function telem_update(d) {
  _robotState = (d.state || 'UNKNOWN').toUpperCase();

  _set_text('telState',   _robotState,         'accent');
  _set_text('telWifi',    d.wifi  || '—',       d.wifi  === 'OK' ? 'ok' : 'error');
  _set_text('telBt',      d.bt    || '—',       d.bt    === 'OK' ? 'ok' : 'error');
  _set_text('telLastCmd', d.lastCmd || '—',     '');

  // Uptime — format as HH:MM:SS
  const up = parseInt(d.uptime, 10);
  if (!isNaN(up)) {
    const hh = String(Math.floor(up / 3600)).padStart(2, '0');
    const mm = String(Math.floor((up % 3600) / 60)).padStart(2, '0');
    const ss = String(up % 60).padStart(2, '0');
    _set_text('telUptime', `${hh}:${mm}:${ss}`, '');
  }

  // Optional fields
  if (d.rssi !== undefined && dom.telRssi) {
    const rssiClass = d.rssi > -60 ? 'ok' : d.rssi > -75 ? 'warn' : 'error';
    _set_text('telRssi', `${d.rssi} dBm`, rssiClass);
  }

  if (d.latencyMs !== undefined && dom.telLatency) {
    const latClass = d.latencyMs < 100 ? 'ok' : d.latencyMs < 300 ? 'warn' : 'error';
    _set_text('telLatency', `${d.latencyMs} ms`, latClass);
  }

  face_set(_robotState);
}

// ── Helper: set telem-value text and colour class ────────────────────────────
function _set_text(id, text, colorClass) {
  const el = dom[id];
  if (!el) return;
  el.textContent = text;
  el.className   = 'telem-value' + (colorClass ? ` ${colorClass}` : '');
}

// ══════════════════════════════════════════════════════════════════════════════
//  FACE ENGINE
// ══════════════════════════════════════════════════════════════════════════════

const FACE_STATES = ['ready', 'moving', 'safe', 'error', 'offline', 'connecting'];

function face_set(state) {
  if (!dom.robotFace) return;
  dom.robotFace.classList.remove(...FACE_STATES);
  if (dom.faceStateLabel) dom.faceStateLabel.textContent = state;

  switch (state) {
    case 'READY':      dom.robotFace.classList.add('ready');      break;
    case 'MOVING':     dom.robotFace.classList.add('moving');     break;
    case 'SAFE':       dom.robotFace.classList.add('safe');       break;
    case 'ERROR':      dom.robotFace.classList.add('error');      break;
    case 'CONNECTING': dom.robotFace.classList.add('connecting'); break;
    default:           dom.robotFace.classList.add('offline');    break;
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  MOVEMENT CONTROLS
// ══════════════════════════════════════════════════════════════════════════════

function cmd_send(cmd, btnEl) {
  if (!ws_send(cmd)) {
    log_add('Not connected — command dropped', 'ev-error');
    return;
  }
  log_add(cmd, 'ev-command');
  _btn_flash(btnEl);
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

    btn.addEventListener('pointerdown', e => {
      e.preventDefault();
      cmd_send(cmd, btn);
    });

    btn.addEventListener('keydown', e => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        cmd_send(cmd, btn);
      }
    });
  });
}

// ── Keyboard map ──────────────────────────────────────────────────────────────
const KEY_MAP = {
  'ArrowUp':    { cmd: 'MOVE F', id: 'btnF' },
  'ArrowDown':  { cmd: 'MOVE B', id: 'btnB' },
  'ArrowLeft':  { cmd: 'MOVE L', id: 'btnL' },
  'ArrowRight': { cmd: 'MOVE R', id: 'btnR' },
  ' ':          { cmd: 'STOP',   id: 'btnStop' },
  'w':          { cmd: 'MOVE F', id: 'btnF' },
  's':          { cmd: 'MOVE B', id: 'btnB' },
  'a':          { cmd: 'MOVE L', id: 'btnL' },
  'd':          { cmd: 'MOVE R', id: 'btnR' },
  'e':          { cmd: 'ESTOP',  id: 'btnStop' },   // Emergency stop
};

function controls_bind_keyboard() {
  document.addEventListener('keydown', e => {
    if (document.activeElement === dom.ipInput) return;
    const m = KEY_MAP[e.key];
    if (!m) return;
    e.preventDefault();
    cmd_send(m.cmd, dom[m.id]);
  });
}

// ── Tab visibility ESTOP ──────────────────────────────────────────────────────
function controls_bind_visibility() {
  if (!CFG.ESTOP_ON_BLUR) return;

  // Page visibility API — fires when tab is backgrounded, phone locks, etc.
  document.addEventListener('visibilitychange', () => {
    if (document.hidden && _wsConnected) {
      ws_send('ESTOP');
      log_add('Tab hidden — ESTOP sent', 'ev-error');
    }
  });

  // beforeunload — tab close / navigation
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
// ══════════════════════════════════════════════════════════════════════════════

function log_add(msg, cssClass) {
  const ts = new Date().toLocaleTimeString('en-GB', { hour12: false });
  _logEntries.unshift({ ts, msg, cssClass });
  if (_logEntries.length > CFG.LOG_MAX) _logEntries.length = CFG.LOG_MAX;
  log_render();
}

function log_render() {
  if (!dom.logScroll) return;

  // Remove existing entries
  dom.logScroll.querySelectorAll('.log-entry').forEach(el => el.remove());

  if (_logEntries.length === 0) {
    if (dom.logEmpty) dom.logEmpty.style.display = '';
    return;
  }
  if (dom.logEmpty) dom.logEmpty.style.display = 'none';

  const frag = document.createDocumentFragment();
  _logEntries.forEach(entry => {
    const row = document.createElement('div');
    row.className = `log-entry ${entry.cssClass || ''}`;

    const ts  = document.createElement('span');
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
  theme_apply();

  // Restore last used host
  const savedHost = localStorage.getItem('greta_host');
  if (savedHost && dom.ipInput) dom.ipInput.value = savedHost;

  // Theme toggle
  if (dom.themeToggle) dom.themeToggle.addEventListener('click', theme_toggle);

  // Connect button
  if (dom.connectBtn) {
    dom.connectBtn.addEventListener('click', () => {
      const h = dom.ipInput ? dom.ipInput.value.trim() : '';
      if (h) {
        _manualHost = h;
        localStorage.setItem('greta_host', h);
      }
      clearTimeout(_reconnectTimer);
      ws_connect();
    });
  }

  // Clear log
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
  log_add('Dashboard initialised — v2', 'ev-connect');

  ws_connect();
}

document.addEventListener('DOMContentLoaded', init);
