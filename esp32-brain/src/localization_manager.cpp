/*
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 */

#include "localization_manager.h"

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

LocalizationManager localizationManager;

void LocalizationManager::init(void) {
    m_current_node[0] = '\0';
    m_has_current_node = false;
}

void LocalizationManager::update(void) {
    // Placeholder for future graph-based localization updates.
}

void LocalizationManager::setCurrentNode(const char* node_id) {
    if (!node_id || node_id[0] == '\0') return;

    copy_str(m_current_node, sizeof(m_current_node), node_id);
    m_has_current_node = true;
}

const char* LocalizationManager::getCurrentNode(void) const {
    return m_has_current_node ? m_current_node : "";
}

bool LocalizationManager::hasCurrentNode(void) const {
    return m_has_current_node;
}

void localization_manager_init(void) {
    localizationManager.init();
}

void localization_manager_update(void) {
    localizationManager.update();
}
