/**
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 * https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MODE_IDLE       = 0,
    MODE_MANUAL     = 1,
    MODE_AUTONOMOUS = 2,
    MODE_SAFE       = 3,
    MODE_ERROR      = 4,
    MODE_COUNT
} RoverMode;

void mode_init(void);
void mode_update(void);

bool mode_request(RoverMode requested, const char* reason);
bool mode_request_from_string(const char* mode_str, const char* reason);
void mode_force(RoverMode next, const char* reason);

RoverMode   mode_get(void);
const char* mode_name(void);
const char* mode_last_reason(void);
bool        mode_is_motion_permitted(void);
