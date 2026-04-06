// Greta Rover OS
// Copyright (c) 2026 Shrivardhan Jadhav
// Licensed under Apache License 2.0

// ══════════════════════════════════════════════════════════════════════════════
//  GRETA V2 — mission_log.js
//  Mission log: timestamped record of commands, mode changes, system events
//
//  Load order: AFTER app.js and mode_manager.js
//  Dependencies: none — other modules call mission_log_add() to append entries
//
//  Design:
//    Separate from the event log in app.js (which is session-only and ephemeral).
//    Mission log is persisted in sessionStorage so it survives page refreshes
//    within the same browser session, but clears on tab close.
//
//    Only significant events are mirrored here (not routine ACKs). The set of
//    mirrored categories is defined in MISSION_CATEGORIES below.
//
//    Entries have: timestamp (HH:MM:SS), message, CSS class.
//
//  Entry CSS classes and their meaning:
//    ev-command  — movement command sent
//    ev-mode     — operating mode change
//    ev-voice    — voice command matched
//    ev-obstacle — obstacle event from hardware
//    ev-system   — connection, boot, or safety system events
//    ev-error    — errors
// ══════════════════════════════════════════════════════════════════════════════

'use strict';

// ─── Config ───────────────────────────────────────────────────────────────────
const MISSION_LOG_MAX     = 100;
const MISSION_STORAGE_KEY = 'greta_mission_log';

// Event log categories that are significant enough to appear in the mission log.
// Routine ACKs (ev-ack) are intentionally excluded.
const MISSION_CATEGORIES = new Set([
  'ev-command', 'ev-obstacle', 'ev-error', 'ev-connect', 'ev-mode', 'ev-voice',
]);

// ─── State ────────────────────────────────────────────────────────────────────
let _missionEntries = [];

// ─── Public API ───────────────────────────────────────────────────────────────

// Append an entry to the mission log.
// Called directly by other modules for mission-significant events.
function mission_log_add(msg, cssClass) {
  const ts = new Date().toLocaleTimeString('en-GB', { hour12: false });
  _missionEntries.unshift({ ts, msg, cssClass: cssClass || 'ev-system' });

  if (_missionEntries.length > MISSION_LOG_MAX) {
    _missionEntries.length = MISSION_LOG_MAX;
  }

  _mission_persist();
  _mission_render();
}

// Export mission log as plain text in chronological order.
// Useful for post-session review or debugging.
function mission_log_export() {
  return _missionEntries
    .slice()
    .reverse()
    .map(e => `[${e.ts}] ${e.msg}`)
    .join('\n');
}

// Clear all mission log entries
function mission_log_clear() {
  _missionEntries = [];
  sessionStorage.removeItem(MISSION_STORAGE_KEY);
  _mission_render();
}

// ─── Session persistence ──────────────────────────────────────────────────────

function _mission_persist() {
  try {
    sessionStorage.setItem(MISSION_STORAGE_KEY, JSON.stringify(_missionEntries));
  } catch (_) {
    // Storage quota exceeded — not critical; log continues in memory
  }
}

function _mission_restore() {
  try {
    const raw = sessionStorage.getItem(MISSION_STORAGE_KEY);
    if (raw) {
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed)) {
        _missionEntries = parsed.slice(0, MISSION_LOG_MAX);
      }
    }
  } catch (_) {
    _missionEntries = [];
  }
}

// ─── Render ───────────────────────────────────────────────────────────────────
function _mission_render() {
  const scroll = document.getElementById('missionScroll');
  const empty  = document.getElementById('missionEmpty');
  if (!scroll) return;

  scroll.querySelectorAll('.log-entry').forEach(el => el.remove());

  if (_missionEntries.length === 0) {
    if (empty) empty.style.display = '';
    return;
  }
  if (empty) empty.style.display = 'none';

  const frag = document.createDocumentFragment();
  _missionEntries.forEach(entry => {
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

  scroll.appendChild(frag);
  scroll.scrollTop = scroll.scrollHeight;
}

// ─── Mirror significant events from the main event log ───────────────────────
// Wraps app.js's log_add to capture mission-significant categories.
// This runs after app.js loads, so log_add is already defined.
(function _patch_log_add() {
  const _orig = window.log_add;
  if (typeof _orig !== 'function') return;

  window.log_add = function (msg, cssClass) {
    _orig(msg, cssClass);

    if (MISSION_CATEGORIES.has(cssClass)) {
      _missionEntries.unshift({
        ts:       new Date().toLocaleTimeString('en-GB', { hour12: false }),
        msg,
        cssClass: cssClass || 'ev-system',
      });
      if (_missionEntries.length > MISSION_LOG_MAX) {
        _missionEntries.length = MISSION_LOG_MAX;
      }
      _mission_persist();
      _mission_render();
    }
  };
})();

// ─── Export button (optional) ─────────────────────────────────────────────────
// Attach to a button with id="exportMission" in index.html to enable export.
function _bind_export() {
  const btn = document.getElementById('exportMission');
  if (!btn) return;

  btn.addEventListener('click', () => {
    const text = mission_log_export();
    const blob = new Blob([text], { type: 'text/plain' });
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href     = url;
    a.download = `greta_mission_${Date.now()}.txt`;
    a.click();
    URL.revokeObjectURL(url);
  });
}

// ─── Clear button ─────────────────────────────────────────────────────────────
function _bind_clear() {
  const btn = document.getElementById('clearMission');
  if (!btn) return;
  btn.addEventListener('click', mission_log_clear);
}

// ─── Boot ─────────────────────────────────────────────────────────────────────
function _mission_init() {
  _mission_restore();   // restore entries from previous page load in this session
  _bind_clear();
  _bind_export();
  _mission_render();
  mission_log_add('Mission session started', 'ev-system');
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', _mission_init);
} else {
  _mission_init();
}
