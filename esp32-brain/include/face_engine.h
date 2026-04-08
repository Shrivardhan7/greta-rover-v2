/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once
// ============================================================================
//  face_engine.h  -  GRETA Expression System  (STUB - interface only)
//
//  This module is part of Greta's behavior and personality layer.
//  It does not own pixels, displays, graphics libraries, or rendering policy.
//  It only resolves Greta's current expression state from system truth.
//
//  Expression authority:
//    - state_manager provides the authoritative robot state
//    - behavior_manager provides decision and safety intent
//    - face_engine translates that combined system condition into a
//      deterministic Greta expression
//
//  This keeps expression output aligned with actual GRETA OS behavior rather
//  than raw commands or transport events. The module is intentionally Greta-
//  specific and is not a generic display abstraction.
//
//  Rendering remains a downstream concern and is intentionally excluded here.
// ============================================================================

#include <Arduino.h>
#include "state_manager.h"

// Greta expression states are compact, deterministic, and derived from
// the current control and safety condition of the robot.
enum GretaExpression : uint8_t {
    GRETA_EXPRESSION_OFFLINE = 0,
    GRETA_EXPRESSION_BOOT    = 1,
    GRETA_EXPRESSION_READY   = 2,
    GRETA_EXPRESSION_ACTIVE  = 3,
    GRETA_EXPRESSION_SAFE    = 4,
    GRETA_EXPRESSION_ERROR   = 5,
    GRETA_EXPRESSION_ALERT   = 6,
    GRETA_EXPRESSION_COUNT   = 7
};

typedef struct {
    GretaExpression active;
    RobotState      source_state;
    bool            safety_latched;
} GretaExpressionStatus;

// Lifecycle
void face_init(void);
void face_update(void);    // Non-blocking expression tick

// System-driven expression observers
void face_on_state_change(RobotState prev_state, RobotState new_state);
void face_on_behavior_update(void);
void face_sync_from_system(void);

// Status
GretaExpression       face_current_expression(void);
GretaExpressionStatus face_status(void);
