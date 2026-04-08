/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  voice_engine.h  —  Greta V2  (STUB — interface defined, driver TBD)
//
//  The voice engine owns all audio output and personality.
//  It observes state transitions via voice_on_state_change() and queues
//  phrases asynchronously so audio output never blocks the control loop.
//
//  Personality profile: gentle, warm, emotionally calming.
//  Language: bilingual Marathi / English — runtime-configurable.
//
//  Hardware target: I2S DAC + speaker, or PWM buzzer as a fallback.
//  Implementation: pending I2S driver and audio buffer integration.
// ============================================================================

#include <Arduino.h>
#include "state_manager.h"   // For RobotState — ensures type consistency

// ── Language selection ────────────────────────────────────────────────────────
enum VoiceLanguage : uint8_t {
    VOICE_LANG_ENGLISH = 0,
    VOICE_LANG_MARATHI = 1,
};

// ── Phrase catalogue ──────────────────────────────────────────────────────────
enum VoicePhrase : uint8_t {
    PHRASE_READY,           // "I am ready"
    PHRASE_MOVING,          // "Moving"
    PHRASE_OBSTACLE,        // "Obstacle ahead, stopping"
    PHRASE_SAFE_MODE,       // "Entering safe mode"
    PHRASE_LINK_LOST,       // "Connection lost"
    PHRASE_LINK_RESTORED,   // "Connection restored"
    PHRASE_GREETING,        // Morning / arrival greeting
    PHRASE_GOODBYE,
    PHRASE_ATTENTION,       // Attention-getting sound
    PHRASE_COUNT
};

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void voice_init();
void voice_update();    // Non-blocking audio pump — call every loop()

// ── Control ───────────────────────────────────────────────────────────────────
void voice_set_language(VoiceLanguage lang);
void voice_speak(VoicePhrase phrase);    // Queues phrase for playback; non-blocking
void voice_set_volume(uint8_t vol);     // 0 = silent, 100 = full volume

// ── Observer ──────────────────────────────────────────────────────────────────
// Register this from main.cpp to get automatic voice cues on state changes.
// Called by state_manager (or main.cpp adapter) on each FSM transition.
void voice_on_state_change(RobotState prevState, RobotState newState);
