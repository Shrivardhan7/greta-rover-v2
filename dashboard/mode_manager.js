// Greta Rover OS
// Copyright (c) 2026 Shrivardhan Jadhav
// SPDX-License-Identifier: Apache-2.0
// Licensed under Apache License 2.0

// ══════════════════════════════════════════════════════════════════════════════
//  GRETA V2 — mode_manager.js
//  Operating mode: IDLE | MANUAL | AUTONOMOUS | SAFE | ERROR
//
//  Load order: AFTER app.js, BEFORE voice_control.js and mission_log.js
//
//  Design:
//    - Mode state is owned entirely by this module. app.js does not know about modes.
//    - Mode changes are sent to the ESP32 as "MODE <name>" via ws_send().
//    - The ESP32 can reject a mode change by sending {"event":"MODE_REJECTED"}.
//    - cmd_send (from app.js) is wrapped here to enforce the mode gate.
//      STOP and ESTOP always bypass the gate regardless of mode.
//    - Voice input is treated as a MANUAL control path, not a rover mode.
//    - Voice panel visibility is also controlled here.
//
//  Safety:
//    IDLE mode: operator must select a motion-capable mode first.
//    MANUAL mode: D-pad and browser voice controls are active.
//    AUTONOMOUS mode: dashboard motion input is blocked.
//    SAFE / ERROR: motion input is blocked, only STOP passes through.
// ══════════════════════════════════════════════════════════════════════════════

'use strict';

// ─── Mode definitions ─────────────────────────────────────────────────────────
const MODES = Object.freeze({
  IDLE: {
    label:       'IDLE',
    desc:        'Awaiting operator mode selection',
    dpadAllowed: false,
  },
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
  SAFE: {
    label:       'SAFE',
    desc:        'All movement blocked',
    dpadAllowed: false,
  },
  ERROR: {
    label:       'ERROR',
    desc:        'Fault state active',
    dpadAllowed: false,
  },
});

// ─── State ────────────────────────────────────────────────────────────────────
const VOICE_INPUT_MODE = 'MANUAL';

let _currentMode = 'IDLE';
let _pendingMode = null;
let _confirmedMode = 'IDLE';

// ─── Public API ───────────────────────────────────────────────────────────────

// Returns the active mode name string
function mode_get() {
  return _currentMode;
}

// Request a mode change.
// source: 'user' | 'system' — used for log labelling only.
// UI is updated optimistically; ESP32 rejection reverts to the last confirmed mode.
function mode_set(newMode, source) {
  if (!MODES[newMode]) {
    console.warn('[MODE] Attempted to set unknown mode:', newMode);
    return false;
  }
  if (newMode === 'ERROR') return false;
  if (_pendingMode) {
    if (typeof log_add === 'function') {
      log_add(`Mode change pending — waiting for ${_pendingMode.requested}`, 'ev-error');
    }
    return false;
  }
  if (newMode === _currentMode) return true;

  const prev = _currentMode;
  if (typeof ws_send !== 'function' || !ws_send(`MODE ${newMode}`)) {
    if (typeof log_add === 'function') {
      log_add('Not connected — mode request dropped', 'ev-error');
    }
    return false;
  }

  _pendingMode = { previous: _confirmedMode, requested: newMode };
  _currentMode = newMode;

  _mode_update_ui(newMode);

  // Log to event log
  if (typeof log_add === 'function') {
    const src = source ? ` [${source}]` : '';
    log_add(`Mode -> ${prev} to ${newMode}${src}`, 'ev-mode');
  }

  return true;
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

  // Voice input is available as a MANUAL-mode control path.
  const voicePanel = document.getElementById('voicePanel');
  if (voicePanel) {
    voicePanel.hidden = (mode !== VOICE_INPUT_MODE);
  }
}

// ─── Handle MODE events from the ESP32 ───────────────────────────────────────
// Wrap ws_on_message (defined in app.js) to intercept mode-related events
// before the default handler sees them.
(function _patch_ws_on_message() {
  const _original = window.ws_on_message;
  if (typeof _original !== 'function') return;

  window.ws_on_message = function (raw) {
    if (typeof raw === 'string' && raw.trim().startsWith('{')) {
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

        if (d.mode) {
          _mode_confirm(d.mode);
        }
      } catch (_) {
        // Not valid JSON — fall through to original handler
      }
    }
    _original(raw);
  };
})();

// Revert to the last confirmed mode after ESP32 rejects a mode change
function _mode_rollback() {
  const fallbackMode = (_pendingMode && _pendingMode.previous) || _confirmedMode || 'IDLE';
  if (typeof log_add === 'function') {
    log_add(`Mode change rejected by ESP32 — reverting to ${fallbackMode}`, 'ev-error');
  }
  _pendingMode = null;
  _currentMode = fallbackMode;
  _mode_update_ui(fallbackMode);
}

// Sync local mode to what the ESP32 confirms (in case of drift)
function _mode_confirm(mode) {
  if (MODES[mode]) {
    const prevConfirmed = _confirmedMode;
    const uiNeedsUpdate = _currentMode !== mode;

    _pendingMode = null;
    _confirmedMode = mode;
    _currentMode = mode;

    if (uiNeedsUpdate) {
      _mode_update_ui(mode);
    }

    if (mode !== prevConfirmed && typeof mission_log_add === 'function') {
      mission_log_add(`Mode: ${prevConfirmed} → ${mode}`, 'ev-mode');
    }
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
      return false;
    }
    return _original(cmd, btnEl);
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
  _mode_update_ui(_currentMode);

  // Voice panel visibility is driven by the active rover mode.
}
