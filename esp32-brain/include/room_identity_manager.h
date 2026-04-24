/*
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 */

#pragma once

#include <ArduinoJson.h>
#include <stdbool.h>
#include <stdint.h>

#define ROOM_HOME_ID_MAX_LEN      16
#define ROOM_NODE_ID_MAX_LEN      24
#define ROOM_TYPE_MAX_LEN         24
#define ROOM_OWNER_MAX_LEN        24
#define ROOM_SIGNAL_MAX_LEN       48
#define ROOM_TIMESTAMP_MAX_LEN    24
#define PERSON_ID_MAX_LEN         24

typedef struct {
    bool     valid;
    bool     human_detected;
    bool     motion_detected;
    float    confidence;
    char     detected_person[PERSON_ID_MAX_LEN];
    int8_t   local_hour;
    bool     has_local_hour;
    bool     door_closed;
    bool     silence_detected;
    bool     activity_detected;
} RoomObservation;

typedef struct {
    char     node[ROOM_NODE_ID_MAX_LEN];
    char     room_type[ROOM_TYPE_MAX_LEN];
    char     owner[ROOM_OWNER_MAX_LEN];
    float    confidence;
    bool     restricted;
} RoomProfile;

typedef struct {
    char      home_id[ROOM_HOME_ID_MAX_LEN];
    char      node[ROOM_NODE_ID_MAX_LEN];
    char      room_type[ROOM_TYPE_MAX_LEN];
    char      owner[ROOM_OWNER_MAX_LEN];
    float     confidence;
    char      alternative_room_type[ROOM_TYPE_MAX_LEN];
    float     alternative_confidence;
    bool      needs_review;
    bool      restricted;
    char      signals[ROOM_SIGNAL_MAX_LEN];
    char      last_verified[ROOM_TIMESTAMP_MAX_LEN];
} RoomIdentityStatus;

class RoomIdentityManager {
public:
    void init(void);
    void updateNode(const char* node_id);
    void observe(const RoomObservation& obs);
    RoomProfile getRoomProfile(void) const;
    void update(uint32_t now_ms);
    void loadRoomIdentity(void);
    void queueRoomIdentitySave(void);
    void processPersistence(void);

    bool handleCommand(const char* cmd);
    void buildTelemetry(JsonDocument& doc) const;
    bool getStatus(RoomIdentityStatus* out) const;

private:
    void recompute(uint32_t now_ms);
    void applyDecay(uint32_t now_ms);
    void refreshProfile(uint32_t now_ms);
};

extern RoomIdentityManager roomIdentityManager;

void room_identity_manager_init(void);
void room_identity_manager_update(void);

bool room_identity_handle_command(const char* cmd);
void room_identity_build_telemetry(JsonDocument& doc);
bool room_identity_status(RoomIdentityStatus* out);
