// Greta Rover OS
// Copyright (c) 2026 Shrivardhan Jadhav
// Licensed under Apache License 2.0

// ══════════════════════════════════════════════════════════════════════════════
//  GRETA V2 — voice_control.js
//  Browser microphone → robot commands via Web Speech API
//
//  Load order: AFTER mode_manager.js
//  Dependencies: ws_send, log_add (app.js), mode_get (mode_manager.js)
//
//  Design:
//    Push-to-listen model — user taps the mic button to start recognition.
//    Single utterance per button press (continuous=false) to avoid runaway
//    recognition and battery drain on mobile.
//    Commands are matched against VOICE_MAP using substring matching.
//    Matched command is sent immediately on interim results for low latency.
//    Only active when mode is VOICE (also enforced by mode_manager cmd_send gate).
//
//  Browser support:
//    Chrome / Edge desktop:       full support
//    Safari iOS 14.5+:            supported, requires HTTPS
//    Firefox:                     NOT supported (no Web Speech API)
//
//  Supported voice commands (case-insensitive, substring match):
//    "forward" / "move forward" / "go"   → MOVE F
//    "back" / "backward" / "reverse"     → MOVE B
//    "left" / "turn left"                → MOVE L
//    "right" / "turn right"              → MOVE R
//    "stop" / "halt" / "brake"           → STOP
//    "emergency" / "estop"               → ESTOP
// ══════════════════════════════════════════════════════════════════════════════

'use strict';

// ─── Voice command map ────────────────────────────────────────────────────────
// Each entry: [ triggerWords[], wsCommand ]
// Evaluated in order — put more specific phrases before generic ones.
const VOICE_MAP = [
  [['move forward', 'go forward', 'forward', 'ahead', 'go'],  'MOVE F'],
  [['backward', 'backwards', 'reverse', 'back'],              'MOVE B'],
  [['turn left', 'go left', 'left'],                          'MOVE L'],
  [['turn right', 'go right', 'right'],                       'MOVE R'],
  [['emergency stop', 'emergency', 'estop'],                  'ESTOP'],
  [['stop', 'halt', 'brake', 'pause'],                        'STOP'],
];

// D-pad button IDs to flash on voice match — visual feedback for the operator
const CMD_BUTTON_MAP = {
  'MOVE F': 'btnF',
  'MOVE B': 'btnB',
  'MOVE L': 'btnL',
  'MOVE R': 'btnR',
  'STOP':   'btnStop',
  'ESTOP':  'btnStop',
};

// ─── State ────────────────────────────────────────────────────────────────────
let _recognition    = null;
let _isListening    = false;
let _supported      = false;
let _resetTimer     = null;   // timer to reset transcript display after match

// ─── DOM refs ─────────────────────────────────────────────────────────────────
let _voiceBtn         = null;
let _voiceTranscript  = null;
let _voiceStatus      = null;
let _voiceUnsupported = null;

// ─── Init ─────────────────────────────────────────────────────────────────────
function voice_init() {
  _voiceBtn         = document.getElementById('voiceBtn');
  _voiceTranscript  = document.getElementById('voiceTranscript');
  _voiceStatus      = document.getElementById('voiceStatusLabel');
  _voiceUnsupported = document.getElementById('voiceUnsupported');

  const SpeechRecognition =
    window.SpeechRecognition || window.webkitSpeechRecognition;

  if (!SpeechRecognition) {
    _supported = false;
    if (_voiceBtn)         _voiceBtn.disabled = true;
    if (_voiceUnsupported) _voiceUnsupported.style.display = '';
    return;
  }

  _supported = true;
  _build_recognition(SpeechRecognition);

  if (_voiceBtn) {
    _voiceBtn.addEventListener('pointerdown', e => {
      e.preventDefault();
      voice_toggle();
    });
  }

  _set_status('INACTIVE', false);
}

function _build_recognition(SpeechRecognition) {
  _recognition = new SpeechRecognition();
  _recognition.continuous     = false;   // one utterance per button press
  _recognition.interimResults = true;    // show partial transcript while speaking
  _recognition.lang           = 'en-US'; // change to 'mr-IN' for Marathi support

  _recognition.onstart  = _on_start;
  _recognition.onresult = _on_result;
  _recognition.onerror  = _on_error;
  _recognition.onend    = _on_end;
}

// ─── Toggle listening ─────────────────────────────────────────────────────────
function voice_toggle() {
  if (!_supported) return;

  // Guard: voice recognition is only meaningful in VOICE mode
  if (typeof mode_get === 'function' && mode_get() !== 'VOICE') {
    if (typeof log_add === 'function') {
      log_add('Switch to VOICE mode to use microphone', 'ev-error');
    }
    return;
  }

  if (_isListening) {
    _recognition.stop();
  } else {
    _transcript_set('Listening…', '');
    _recognition.start();
  }
}

// ─── Recognition event handlers ───────────────────────────────────────────────
function _on_start() {
  _isListening = true;
  if (_voiceBtn) _voiceBtn.classList.add('listening');
  _set_status('LISTENING', true);
  if (typeof log_add === 'function') log_add('Voice — listening…', 'ev-voice');
}

function _on_result(evt) {
  // Concatenate all result segments into one string
  let transcript = '';
  for (let i = evt.resultIndex; i < evt.results.length; i++) {
    transcript += evt.results[i][0].transcript;
  }

  const text = transcript.trim().toLowerCase();
  _transcript_set(transcript.trim(), '');

  // Match on interim results so the command fires as soon as the word is spoken
  const matched = _match_command(text);
  if (matched) {
    _execute_voice_command(matched, transcript.trim());
    _recognition.stop();
  }
}

function _on_error(evt) {
  _isListening = false;
  if (_voiceBtn) _voiceBtn.classList.remove('listening');
  _set_status('INACTIVE', false);

  const msg = evt.error === 'no-speech'
    ? 'No speech detected'
    : `Voice error: ${evt.error}`;

  _transcript_set(msg, 'no-match');
  if (typeof log_add === 'function') log_add(msg, 'ev-error');
}

function _on_end() {
  _isListening = false;
  if (_voiceBtn) _voiceBtn.classList.remove('listening');
  _set_status('INACTIVE', false);
}

// ─── Command matching ─────────────────────────────────────────────────────────
// Returns the ws command string if any trigger phrase is found in the text.
// Returns null if no match.
function _match_command(text) {
  for (const [triggers, cmd] of VOICE_MAP) {
    for (const trigger of triggers) {
      if (text.includes(trigger)) {
        return cmd;
      }
    }
  }
  return null;
}

// ─── Execute matched command ──────────────────────────────────────────────────
function _execute_voice_command(cmd, transcript) {
  _transcript_set(transcript, 'matched');

  if (typeof ws_send === 'function') ws_send(cmd);

  const logMsg = `Voice: "${transcript}" → ${cmd}`;
  if (typeof log_add         === 'function') log_add(logMsg, 'ev-voice');
  if (typeof mission_log_add === 'function') mission_log_add(logMsg, 'ev-voice');

  // Flash the corresponding D-pad button for visual confirmation
  const btnId = CMD_BUTTON_MAP[cmd];
  if (btnId) {
    const btn = document.getElementById(btnId);
    if (btn) {
      btn.classList.add('pressed');
      setTimeout(() => btn.classList.remove('pressed'), 300);
    }
  }

  // Reset transcript display after a short pause
  clearTimeout(_resetTimer);
  _resetTimer = setTimeout(() => {
    _transcript_set('Say a command…', '');
  }, 2000);
}

// ─── UI helpers ───────────────────────────────────────────────────────────────
function _transcript_set(text, cssClass) {
  if (!_voiceTranscript) return;
  _voiceTranscript.textContent = text;
  _voiceTranscript.className   = 'voice-transcript' + (cssClass ? ` ${cssClass}` : '');
}

function _set_status(label, isListening) {
  if (!_voiceStatus) return;
  _voiceStatus.textContent = label;
  _voiceStatus.className   = 'voice-status-label' + (isListening ? ' listening' : '');
}

// ─── Public API ───────────────────────────────────────────────────────────────

// Change recognition language at runtime.
// e.g. voice_set_language('mr-IN') for Marathi
function voice_set_language(langCode) {
  if (_recognition) _recognition.lang = langCode;
}

// Returns true if Web Speech API is supported in this browser
function voice_is_supported() {
  return _supported;
}

// ─── Boot ─────────────────────────────────────────────────────────────────────
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', voice_init);
} else {
  voice_init();
}
