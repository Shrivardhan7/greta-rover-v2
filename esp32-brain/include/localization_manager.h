/*
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LOCALIZATION_NODE_ID_MAX_LEN 24

class LocalizationManager {
public:
    void init(void);
    void update(void);

    void setCurrentNode(const char* node_id);
    const char* getCurrentNode(void) const;
    bool hasCurrentNode(void) const;

private:
    char m_current_node[LOCALIZATION_NODE_ID_MAX_LEN];
    bool m_has_current_node;
};

extern LocalizationManager localizationManager;

void localization_manager_init(void);
void localization_manager_update(void);
