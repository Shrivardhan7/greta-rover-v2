// ════════════════════════════════════════════════════════════════════════════
//  GRETA V2 — voice_control.js
//  Browser microphone → movement commands via Web Speech API
//
//  Integration:
//    Load AFTER mode_manager.js
//    Depends on: ws_send, log_add, mode_get (from mode_manager)
//
//  Design:
//    Push-to-listen model — user presses the mic button to start recognition.
//    No continuous listening — avoids runaway recognition and battery drain.
//    Voice commands are mapped to existing ws_send() command strings.
//    Works only when mode is VOICE (enforced by mode_manager cmd_send gate,
//    but voice_control also checks independently for clarity).
//
//  Supported voice commands (case-insensitive, fuzzy prefix match):
//    "forward" / "move forward" / "go"     → MOVE F
//    "back" / "backward" / "reverse"       → MOVE B
//    "left" / "turn left"                  → MOVE L
//    "right" / "turn right"                → MOVE R
//    "stop" / "halt" / "brake"             → STOP
//    "emergency" / "estop"                 → ESTOP
// ════════════════════════════════════════════════════════════════════════════

'use strict';

// ─── Voice command map ────────────────────────────────────────────────────────
// Each entry: [ triggerWords[], wsCommand ]
// triggerWords are matched against the recognition result (lowercase, trimmed).
const VOICE_MAP = [
  [['forward', 'move forward', 'go', 'ahead'],   'MOVE F'],
  [['back', 'backward', 'backwards', 'reverse'],  'MOVE B'],
  [['left', 'turn left', 'go left'],              'MOVE L'],
  [['right', 'turn right', 'go right'],           'MOVE R'],
  [['stop', 'halt', 'brake', 'pause'],            'STOP'],
  [['emergency', 'estop', 'emergency stop'],      'ESTOP'],
];

// ─── State ────────────────────────────────────────────────────────────────────
let _recognition   = null;
let _isListening   = false;
let _supported     = false;
let _matchTimer    = null;

// ─── DOM refs ─────────────────────────────────────────────────────────────────
let _voiceBtn        = null;
let _voiceTranscript = null;
let _voiceStatus     = null;
let _voiceUnsupported = null;

// ─── Speech recognition init ──────────────────────────────────────────────────
function voice_init() {
  _voiceBtn         = document.getElementById('voiceBtn');
  _voiceTranscript  = document.getElementById('voiceTranscript');
  _voiceStatus      = document.getElementById('voiceStatusLabel');
  _voiceUnsupported = document.getElementById('voiceUnsupported');

  // Check Web Speech API availability
  const SpeechRecognition =
    window.SpeechRecognition || window.webkitSpeechRecognition;

  if (!SpeechRecognition) {
    _supported = false;
    if (_voiceBtn)         _voiceBtn.disabled = true;
    if (_voiceUnsupported) _voiceUnsupported.style.display = '';
    return;
  }

  _supported = true;

  // Build recognition object
  _recognition = new SpeechRecognition();
  _recognition.continuous     = false;    // Single utterance per button press
  _recognition.interimResults = true;     // Show partial results while speaking
  _recognition.lang            = 'en-US'; // Change to 'mr-IN' for Marathi

  _recognition.onstart = _on_recognition_start;
  _recognition.onresult = _on_recognition_result;
  _recognition.onerror  = _on_recognition_error;
  _recognition.onend    = _on_recognition_end;

  // Bind button
  if (_voiceBtn) {
    _voiceBtn.addEventListener('pointerdown', e => {
      e.preventDefault();
      voice_toggle();
    });
  }

  _set_status('INACTIVE', false);
}

// ─── Toggle listening ─────────────────────────────────────────────────────────
function voice_toggle() {
  if (!_supported) return;

  // Only active in VOICE mode — but still warn clearly if wrong mode
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

// ─── Recognition callbacks ────────────────────────────────────────────────────
function _on_recognition_start() {
  _isListening = true;
  if (_voiceBtn)   _voiceBtn.classList.add('listening');
  _set_status('LISTENING', true);
  if (typeof log_add === 'function') log_add('Voice — listening…', 'ev-voice');
}

function _on_recognition_result(evt) {
  // Collect all results into one string
  let transcript = '';
  for (let i = evt.resultIndex; i < evt.results.length; i++) {
    transcript += evt.results[i][0].transcript;
  }
  const text = transcript.trim().toLowerCase();
  _transcript_set(transcript.trim(), '');

  // Match on interim results so response is fast
  const matched = _match_command(text);
  if (matched) {
    // Execute immediately on first match
    _execute_voice_command(matched, transcript.trim());
    _recognition.stop();    // Don't wait for final result
  }
}

function _on_recognition_error(evt) {
  _isListening = false;
  if (_voiceBtn) _voiceBtn.classList.remove('listening');
  _set_status('INACTIVE', false);

  const msg = evt.error === 'no-speech'
    ? 'No speech detected'
    : `Voice error: ${evt.error}`;

  _transcript_set(msg, 'no-match');
  if (typeof log_add === 'function') log_add(msg, 'ev-error');
}

function _on_recognition_end() {
  _isListening = false;
  if (_voiceBtn) _voiceBtn.classList.remove('listening');
  _set_status('INACTIVE', false);
}

// ─── Command matching ─────────────────────────────────────────────────────────
// Returns the ws command string, or null if no match.
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

// ─── Execute ──────────────────────────────────────────────────────────────────
function _execute_voice_command(cmd, transcript) {
  _transcript_set(transcript, 'matched');

  if (typeof ws_send === 'function') ws_send(cmd);

  const logMsg = `Voice: "${transcript}" → ${cmd}`;
  if (typeof log_add         === 'function') log_add(logMsg, 'ev-voice');
  if (typeof mission_log_add === 'function') mission_log_add(logMsg, 'ev-voice');

  // Flash the corresponding D-pad button for visual feedback
  const BTN_MAP = {
    'MOVE F': 'btnF', 'MOVE B': 'btnB',
    'MOVE L': 'btnL', 'MOVE R': 'btnR',
    'STOP': 'btnStop', 'ESTOP': 'btnStop',
  };
  const btnId = BTN_MAP[cmd];
  if (btnId) {
    const btn = document.getElementById(btnId);
    if (btn) {
      btn.classList.add('pressed');
      setTimeout(() => btn.classList.remove('pressed'), 300);
    }
  }

  // Reset transcript display after short delay
  clearTimeout(_matchTimer);
  _matchTimer = setTimeout(() => {
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

// ─── Public API (callable from other modules) ─────────────────────────────────

// Programmatically change recognition language
// e.g. voice_set_language('mr-IN') for Marathi
function voice_set_language(langCode) {
  if (_recognition) _recognition.lang = langCode;
}

// Returns true if voice is supported in this browser
function voice_is_supported() {
  return _supported;
}

// ─── Boot ─────────────────────────────────────────────────────────────────────
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', voice_init);
} else {
  voice_init();
}
