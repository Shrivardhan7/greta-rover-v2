#pragma once
// ══════════════════════════════════════════════════════════════════════════════
//  voice_engine.h  —  Greta V2  (STUB — not yet implemented)
//
//  Design rationale:
//    The voice engine owns all audio output personality. It is deliberately
//    decoupled from the safety state machine — it observes state transitions
//    via voice_on_state_change() callbacks registered from main.cpp, and
//    queues phrases asynchronously so audio output never blocks the control loop.
//
//    Phrase selection follows a personality profile: Little Krishna style —
//    gentle, warm, emotionally calming, bilingual Marathi/English.
//    The language setting is runtime-configurable without a recompile.
//
//  Hardware target: I2S DAC + speaker, or PWM buzzer as fallback.
//  Implementation: placeholder until I2S driver and audio buffer are integrated.
// ══════════════════════════════════════════════════════════════════════════════

#include <Arduino.h>

// ─── Language selection ───────────────────────────────────────────────────────
enum VoiceLanguage : uint8_t {
    VOICE_LANG_ENGLISH = 0,
    VOICE_LANG_MARATHI = 1,
};

// ─── Phrase categories ────────────────────────────────────────────────────────
enum VoicePhrase : uint8_t {
    PHRASE_READY,           // "I am ready"
    PHRASE_MOVING,          // "Moving"
    PHRASE_OBSTACLE,        // "Obstacle ahead, stopping"
    PHRASE_SAFE_MODE,       // "Entering safe mode"
    PHRASE_LINK_LOST,       // "Connection lost"
    PHRASE_LINK_RESTORED,   // "Connection restored"
    PHRASE_GREETING,        // Morning / arrival greeting
    PHRASE_GOODBYE,
    PHRASE_ATTENTION,       // Cat-calling / attention-getting
    PHRASE_COUNT
};

// ─── Lifecycle ───────────────────────────────────────────────────────────────
void voice_init();
void voice_update();    // Non-blocking audio pump — call every loop()

// ─── Control ─────────────────────────────────────────────────────────────────
void voice_set_language(VoiceLanguage lang);
void voice_speak(VoicePhrase phrase);   // Queues phrase; non-blocking
void voice_set_volume(uint8_t vol);     // 0–100

// ─── Observer ────────────────────────────────────────────────────────────────
// Register this with the state manager to get automatic voice cues.
void voice_on_state_change(uint8_t prevState, uint8_t newState);
