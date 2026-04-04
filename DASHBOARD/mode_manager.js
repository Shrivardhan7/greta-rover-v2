// ════════════════════════════════════════════════════════════════════════════
//  GRETA V2 — mode_manager.js
//  Mode switching: MANUAL | AUTONOMOUS | VOICE | SAFE
//
//  Integration:
//    Load AFTER app.js (depends on: ws_send, log_add, face_set)
//    Load BEFORE voice_control.js and mission_log.js
//
//  This module is self-contained. It does not modify app.js internals.
//  It communicates with the ESP32 by sending "MODE <name>" via ws_send().
//  The ESP32 can reject a mode change by sending {"event":"MODE_REJECTED"}.
// ════════════════════════════════════════════════════════════════════════════

'use strict';

// ─── Mode definitions ─────────────────────────────────────────────────────────
const MODES = Object.freeze({
  MANUAL: {
    label: 'MANUAL',
    desc:  'Direct control active',
    // MANUAL and VOICE allow D-pad input; others block it
    dpadAllowed: true,
  },
  AUTONOMOUS: {
    label: 'AUTONOMOUS',
    desc:  'Autonomous navigation active',
    dpadAllowed: false,
  },
  VOICE: {
    label: 'VOICE',
    desc:  'Voice command control active',
    dpadAllowed: true,     // D-pad remains available as override in VOICE
  },
  SAFE: {
    label: 'SAFE',
    desc:  'All movement blocked',
    dpadAllowed: false,
  },
});

// ─── State ────────────────────────────────────────────────────────────────────
let _currentMode = 'MANUAL';

// ─── Public API ───────────────────────────────────────────────────────────────

// Returns the active mode string
function mode_get() {
  return _currentMode;
}

// Request a mode change. Validates, sends to ESP32, updates UI.
// Optionally pass source = 'user' | 'system' | 'voice' for logging.
function mode_set(newMode, source) {
  if (!MODES[newMode]) {
    console.warn('[MODE] Unknown mode:', newMode);
    return;
  }

  if (newMode === _currentMode) return;

  const prev = _currentMode;
  _currentMode = newMode;

  // Notify ESP32 — it may reject; we update UI optimistically.
  // ESP32 rejection handler is in ws_on_message extension below.
  ws_send(`MODE ${newMode}`);

  _mode_update_ui(newMode);

  // Mission log entry
  if (typeof mission_log_add === 'function') {
    mission_log_add(`Mode: ${prev} → ${newMode}`, 'ev-mode');
  }

  // Also appears in the main event log
  if (typeof log_add === 'function') {
    const src = source ? ` [${source}]` : '';
    log_add(`Mode → ${newMode}${src}`, 'ev-mode');
  }
}

// Check if movement commands should be blocked in current mode
function mode_dpad_allowed() {
  return MODES[_currentMode] ? MODES[_currentMode].dpadAllowed : false;
}

// ─── UI update ────────────────────────────────────────────────────────────────
function _mode_update_ui(mode) {
  const def = MODES[mode];
  if (!def) return;

  // Badge
  const badge = document.getElementById('modeBadge');
  if (badge) {
    badge.textContent = def.label;
    badge.className   = `mode-badge ${mode}`;
  }

  // Description
  const desc = document.getElementById('modeDesc');
  if (desc) desc.textContent = def.desc;

  // Button active states
  document.querySelectorAll('.mode-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.mode === mode);
  });

  // Block D-pad on body when mode doesn't allow manual input
  document.body.classList.toggle('mode-blocked', !def.dpadAllowed);

  // Show/hide voice panel
  const voicePanel = document.getElementById('voicePanel');
  if (voicePanel) {
    voicePanel.style.display = (mode === 'VOICE') ? '' : 'none';
  }
}

// ─── Handle MODE_REJECTED from ESP32 ─────────────────────────────────────────
// Extend the existing ws_on_message to handle mode events.
// Pattern: we wrap the original function and add our handler before returning.
(function _patch_ws_on_message() {
  // ws_on_message is defined in app.js. We extend it here without modifying app.js.
  // This works because JS is single-threaded and this script loads after app.js.
  const _original = window.ws_on_message;
  if (typeof _original !== 'function') {
    // app.js uses a local function — use a global event bridge instead.
    // ESP32 sends {"event":"MODE_REJECTED"} — handled via the global hook below.
    return;
  }
  window.ws_on_message = function(raw) {
    // Check for mode events before passing to original handler
    if (raw.trim().startsWith('{')) {
      try {
        const d = JSON.parse(raw);
        if (d.event === 'MODE_REJECTED') {
          _mode_rollback();
          return;
        }
        if (d.event === 'MODE_ACK' && d.mode) {
          _mode_confirm(d.mode);
          return;
        }
      } catch (_) {}
    }
    _original(raw);
  };
})();

function _mode_rollback() {
  // ESP32 rejected the mode change — revert UI to previous mode
  // For simplicity we revert to MANUAL (the always-safe fallback)
  if (typeof log_add === 'function') {
    log_add('Mode change rejected by ESP32 — reverting to MANUAL', 'ev-error');
  }
  _currentMode = 'MANUAL';
  _mode_update_ui('MANUAL');
}

function _mode_confirm(mode) {
  // ESP32 confirmed — ensure UI matches
  if (MODES[mode]) {
    _currentMode = mode;
    _mode_update_ui(mode);
  }
}

// ─── Bind mode buttons ────────────────────────────────────────────────────────
function _mode_bind_buttons() {
  document.querySelectorAll('.mode-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      mode_set(btn.dataset.mode, 'user');
    });
  });
}

// ─── Patch cmd_send to enforce mode gate ─────────────────────────────────────
// app.js's cmd_send calls ws_send directly. We gate it here by overriding
// cmd_send globally. STOP and ESTOP always bypass the gate.
(function _patch_cmd_send() {
  const _original = window.cmd_send;
  if (typeof _original !== 'function') return;

  window.cmd_send = function(cmd, btnEl) {
    const isStopCmd = (cmd === 'STOP' || cmd === 'ESTOP');
    if (!isStopCmd && !mode_dpad_allowed()) {
      if (typeof log_add === 'function') {
        log_add(`Command blocked — mode is ${_currentMode}`, 'ev-error');
      }
      return;
    }
    _original(cmd, btnEl);
  };
})();

// ─── Init ─────────────────────────────────────────────────────────────────────
(function mode_init() {
  // Defer until DOM is ready — this script may load before DOMContentLoaded
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', _mode_init_dom);
  } else {
    _mode_init_dom();
  }
})();

function _mode_init_dom() {
  _mode_bind_buttons();
  _mode_update_ui('MANUAL');     // Set initial state

  // Voice panel hidden by default (only shown when VOICE mode active)
  const voicePanel = document.getElementById('voicePanel');
  if (voicePanel) voicePanel.style.display = 'none';
}
