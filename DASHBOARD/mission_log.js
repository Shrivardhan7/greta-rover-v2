// ════════════════════════════════════════════════════════════════════════════
//  GRETA V2 — mission_log.js
//  Mission log: timestamped record of commands, mode changes, system events
//
//  Integration:
//    Load AFTER app.js and mode_manager.js
//    Depends on: nothing (self-contained, other modules call mission_log_add)
//
//  Design:
//    Separate from the existing event log (which is ephemeral / session-only).
//    Mission log is persisted in sessionStorage so it survives page refreshes
//    within the same browser session but clears on tab close.
//    Entries have: timestamp, type, message.
//    Exportable as plain text for post-session review.
//
//  Types and their CSS classes:
//    ev-command  — movement command sent
//    ev-mode     — mode change
//    ev-voice    — voice command
//    ev-obstacle — obstacle event
//    ev-system   — connection, boot, safety events
//    ev-error    — errors
// ════════════════════════════════════════════════════════════════════════════

'use strict';

// ─── Config ───────────────────────────────────────────────────────────────────
const MISSION_LOG_MAX      = 100;    // Max entries kept in memory
const MISSION_STORAGE_KEY  = 'greta_mission_log';

// ─── State ────────────────────────────────────────────────────────────────────
let _missionEntries = [];

// ─── Public API ───────────────────────────────────────────────────────────────

// Add an entry to the mission log.
// This is the function other modules call.
function mission_log_add(msg, cssClass) {
  const ts = new Date().toLocaleTimeString('en-GB', { hour12: false });
  const entry = { ts, msg, cssClass: cssClass || 'ev-system' };

  _missionEntries.unshift(entry);
  if (_missionEntries.length > MISSION_LOG_MAX) {
    _missionEntries.length = MISSION_LOG_MAX;
  }

  _mission_persist();
  _mission_render();
}

// Export mission log as a plain text string
function mission_log_export() {
  return _missionEntries
    .slice()
    .reverse()    // Chronological order for export
    .map(e => `[${e.ts}] ${e.msg}`)
    .join('\n');
}

// Clear the mission log
function mission_log_clear() {
  _missionEntries = [];
  sessionStorage.removeItem(MISSION_STORAGE_KEY);
  _mission_render();
}

// ─── Persistence ──────────────────────────────────────────────────────────────
function _mission_persist() {
  try {
    sessionStorage.setItem(
      MISSION_STORAGE_KEY,
      JSON.stringify(_missionEntries)
    );
  } catch (_) {
    // Storage quota exceeded — not critical
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

  // Remove existing entries
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

// ─── Auto-capture from existing event system ──────────────────────────────────
// Mirror selected app.js log_add calls into the mission log automatically.
// We extend log_add non-destructively.
(function _patch_log_add() {
  // Categories that are mission-significant (not just routine ACKs)
  const MISSION_CATEGORIES = new Set([
    'ev-command', 'ev-obstacle', 'ev-error', 'ev-connect', 'ev-mode', 'ev-voice',
  ]);

  const _orig = window.log_add;
  if (typeof _orig !== 'function') return;

  window.log_add = function(msg, cssClass) {
    _orig(msg, cssClass);
    // Mirror to mission log if it's a significant category
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

// ─── Export button (optional — add a button with id="exportMission" to HTML) ──
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

// ─── Init ─────────────────────────────────────────────────────────────────────
function _mission_init() {
  _mission_restore();   // Load previous session entries
  _bind_clear();
  _bind_export();
  _mission_render();

  // Log session start
  mission_log_add('Mission session started', 'ev-system');
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', _mission_init);
} else {
  _mission_init();
}
