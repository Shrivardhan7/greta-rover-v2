/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once

#include <stdbool.h>

typedef enum {
    COMMAND_SOURCE_DASHBOARD = 0,
    COMMAND_SOURCE_AUTONOMY  = 1
} CommandSource;

typedef struct {
    bool        accepted;
    bool        is_motion;
    bool        is_stop;
    const char* reason;
} BehaviorCommandDecision;

void behavior_manager_init(void);
void behavior_manager_update(void);

bool behavior_handle_mode_request(const char* mode_str);
BehaviorCommandDecision behavior_evaluate_command(const char* cmd, CommandSource source);
bool behavior_dispatch_command(const char* cmd, CommandSource source, const char** reason_out);

void behavior_note_motion_command(CommandSource source, const char* cmd);
void behavior_note_stop_command(const char* reason);
void behavior_note_command_rejected(const char* cmd, const char* reason);

void behavior_force_safe(const char* reason);
bool behavior_is_safety_latched(void);
const char* behavior_last_safety_reason(void);
