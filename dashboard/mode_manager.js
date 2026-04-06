// Greta Rover OS
// Copyright (c) 2026 Shrivardhan Jadhav
// Licensed under Apache License 2.0

// ══════════════════════════════════════════════════════════════════════════════
//  GRETA V2 — mode_manager.js
//  Operating mode: MANUAL | AUTONOMOUS | VOICE | SAFE
//
//  Load order: AFTER app.js, BEFORE voice_control.js and mission_log.js
//
//  Design:
//    - Mode state is owned entirely by this module. app.js does not know about modes.
//    - Mode changes are sent to the ESP32 as "MODE <name>" via ws_send().
//    - The ESP32 can reject a mode change by sending {"event":"MODE_REJECTED"}.
//    - cmd_send (from app.js) is wrapped here to enforce the mode gate.
//      STOP and ESTOP always bypass the gate regardless of mode.
//    - Voice panel visibility is also controlled here.
//
//  Safety:
//    MANUAL mode: D-pad active.
//    AUTONOMOUS mode: D-pad blocked (robot navigates independently).
//    VOICE mode: D-pad remains available as manual override.
//    SAFE mode: D-pad blocked, only STOP passes through.
// ══════════════════════════════════════════════════════════════════════════════

'use strict';

// ─── Mode definitions ─────────────────────────────────────────────────────────
const MODES = Object.freeze({
  MANUAL: {
    label:       'MANUAL',
    desc:        'Direct control active',
    dpadAllowed: true,
  },
  AUTONOMOUS: {
    label:       'AUTONOMOUS',
    desc:        'Autonomous navigation active',
    dpadAllowed: false,
  },
  VOICE: {
    label:       'VOICE',
    desc:        'Voice command control active',
    dpadAllowed: true,   // D-pad stays available as override in VOICE mode
  },
  SAFE: {
    label:       'SAFE',
    desc:        'All movement blocked',
    dpadAllowed: false,
  },
});

// ─── State ────────────────────────────────────────────────────────────────────
let _currentMode = 'MANUAL';

// ─── Public API ───────────────────────────────────────────────────────────────

// Returns the active mode name string
function mode_get() {
  return _currentMode;
}

// Request a mode change.
// source: 'user' | 'system' | 'voice' — used for log labelling only.
// UI is updated optimistically; ESP32 rejection reverts to MANUAL.
function mode_set(newMode, source) {
  if (!MODES[newMode]) {
    console.warn('[MODE] Attempted to set unknown mode:', newMode);
    return;
  }
  if (newMode === _currentMode) return;

  const prev = _currentMode;
  _currentMode = newMode;

  // Inform ESP32 — it may reject; rejection is handled below
  ws_send(`MODE ${newMode}`);

  _mode_update_ui(newMode);

  // Log to mission log if available
  if (typeof mission_log_add === 'function') {
    mission_log_add(`Mode: ${prev} → ${newMode}`, 'ev-mode');
  }

  // Log to event log
  if (typeof log_add === 'function') {
    const src = source ? ` [${source}]` : '';
    log_add(`Mode → ${newMode}${src}`, 'ev-mode');
  }
}

// Returns true if D-pad movement commands should be allowed in the current mode
function mode_dpad_allowed() {
  return MODES[_currentMode] ? MODES[_currentMode].dpadAllowed : false;
}

// ─── UI update ────────────────────────────────────────────────────────────────
function _mode_update_ui(mode) {
  const def = MODES[mode];
  if (!def) return;

  // Mode badge text and colour class
  const badge = document.getElementById('modeBadge');
  if (badge) {
    badge.textContent = def.label;
    badge.className   = `mode-badge ${mode}`;
  }

  // Mode description text
  const desc = document.getElementById('modeDesc');
  if (desc) desc.textContent = def.desc;

  // Mode button active state
  document.querySelectorAll('.mode-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.mode === mode);
  });

  // Apply body class to dim/disable D-pad buttons via CSS
  document.body.classList.toggle('mode-blocked', !def.dpadAllowed);

  // Voice panel is only visible in VOICE mode
  const voicePanel = document.getElementById('voicePanel');
  if (voicePanel) {
    voicePanel.style.display = (mode === 'VOICE') ? '' : 'none';
  }
}

// ─── Handle MODE events from the ESP32 ───────────────────────────────────────
// Wrap ws_on_message (defined in app.js) to intercept mode-related events
// before the default handler sees them.
(function _patch_ws_on_message() {
  const _original = window.ws_on_message;
  if (typeof _original !== 'function') return;

  window.ws_on_message = function (raw) {
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
      } catch (_) {
        // Not valid JSON — fall through to original handler
      }
    }
    _original(raw);
  };
})();

// Revert to MANUAL after ESP32 rejects a mode change
function _mode_rollback() {
  if (typeof log_add === 'function') {
    log_add('Mode change rejected by ESP32 — reverting to MANUAL', 'ev-error');
  }
  _currentMode = 'MANUAL';
  _mode_update_ui('MANUAL');
}

// Sync local mode to what the ESP32 confirms (in case of drift)
function _mode_confirm(mode) {
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

// ─── Wrap cmd_send to enforce mode gate ──────────────────────────────────────
// STOP and ESTOP always bypass the gate — safety commands must never be blocked.
(function _patch_cmd_send() {
  const _original = window.cmd_send;
  if (typeof _original !== 'function') return;

  window.cmd_send = function (cmd, btnEl) {
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
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', _mode_init_dom);
  } else {
    _mode_init_dom();
  }
})();

function _mode_init_dom() {
  _mode_bind_buttons();
  _mode_update_ui('MANUAL');

  // Voice panel hidden by default — only shown when VOICE mode is selected
  const voicePanel = document.getElementById('voicePanel');
  if (voicePanel) voicePanel.style.display = 'none';
}
