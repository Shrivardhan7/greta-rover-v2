/*
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SENSOR_PERSON_ID_MAX_LEN 24

typedef struct {
    bool     valid;
    bool     human_presence;
    bool     motion_detected;
    float    confidence;
    char     person[SENSOR_PERSON_ID_MAX_LEN];
    int8_t   local_hour;
    bool     has_local_hour;
    bool     door_closed;
    bool     silence_detected;
    bool     activity_detected;
} SensorObservation;

class SensorManager {
public:
    void init(void);
    void update(void);

    void publishObservation(const SensorObservation& observation);
    bool consumeObservation(SensorObservation* out);
    bool peekObservation(SensorObservation* out) const;

private:
    SensorObservation m_latest;
    uint32_t          m_sequence;
    uint32_t          m_consumed_sequence;
};

extern SensorManager sensorManager;

void sensor_manager_init(void);
void sensor_manager_update(void);
