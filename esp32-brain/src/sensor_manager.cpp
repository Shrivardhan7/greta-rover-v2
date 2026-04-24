/*
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 */

#include "sensor_manager.h"

#include <string.h>

namespace {

static void copy_str(char* dst, size_t dst_len, const char* src) {
    if (!dst || dst_len == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

} // namespace

SensorManager sensorManager;

void SensorManager::init(void) {
    memset(&m_latest, 0, sizeof(m_latest));
    m_latest.local_hour = -1;
    m_sequence = 0;
    m_consumed_sequence = 0;
}

void SensorManager::update(void) {
    // Placeholder for future middleware fan-in.
}

void SensorManager::publishObservation(const SensorObservation& observation) {
    m_latest = observation;
    copy_str(m_latest.person, sizeof(m_latest.person), observation.person);
    ++m_sequence;
}

bool SensorManager::consumeObservation(SensorObservation* out) {
    if (!out) return false;
    if (m_sequence == 0 || m_sequence == m_consumed_sequence) return false;

    *out = m_latest;
    m_consumed_sequence = m_sequence;
    return true;
}

bool SensorManager::peekObservation(SensorObservation* out) const {
    if (!out || m_sequence == 0) return false;

    *out = m_latest;
    return true;
}

void sensor_manager_init(void) {
    sensorManager.init();
}

void sensor_manager_update(void) {
    sensorManager.update();
}
