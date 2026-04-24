/*
 * Greta Rover OS
 * Copyright (c) 2026 Shrivardhan Jadhav
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under Apache License 2.0
 */

#include "room_identity_manager.h"

#include "config.h"
#include "network_manager.h"
#include "state_manager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

static const size_t ROOM_IDENTITY_MAX_ROOMS = 12;
static const uint32_t ROOM_IDENTITY_REVALIDATE_AFTER_S = 24UL * 60UL * 60UL;
static const uint32_t ROOM_IDENTITY_PERSIST_DELAY_MS = 5000;

enum RoomKind {
    ROOM_KIND_ROOM = 0,
    ROOM_KIND_MASTER_BEDROOM,
    ROOM_KIND_SHARED_BEDROOM,
    ROOM_KIND_LIVING_AREA,
    ROOM_KIND_KITCHEN_DINING,
    ROOM_KIND_ENTRY,
    ROOM_KIND_TERRACE,
    ROOM_KIND_BALCONY,
    ROOM_KIND_UTILITY,
    ROOM_KIND_TOILET,
    ROOM_KIND_RESTRICTED,
    ROOM_KIND_COUNT
};

static const char* const ROOM_KIND_NAMES[ROOM_KIND_COUNT] = {
    "room",
    "master_bedroom",
    "shared_bedroom",
    "living_area",
    "kitchen_dining",
    "entry",
    "terrace",
    "balcony",
    "utility",
    "toilet",
    "restricted"
};

struct RoomTemplate {
    const char* node;
    const char* semantic_type;
    const char* owner;
    bool        attached_toilet;
    bool        blocked;
    bool        night_restricted;
};

struct RoomRecord {
    char     node[24];
    char     room_type[24];
    char     owner[24];
    float    confidence;
    char     alternative_room_type[24];
    float    alternative_confidence;
    char     signals[48];
    char     last_verified[24];
    uint32_t last_verified_epoch;
    bool     manual_override;
};

struct RoomObservation {
    char     current_node[24];
    char     detected_person[24];
    float    person_confidence;
    int8_t   local_hour;
    bool     door_closed;
    bool     silence_detected;
    bool     activity_detected;
    char     observed_at[24];
    uint32_t observed_at_epoch;
    bool     has_observation;
};

static const RoomTemplate KOLHAPUR_TEMPLATES[] = {
    { "bedroom_owner",   "bedroom",         "owner",        false, false, false },
    { "bedroom_sister",  "bedroom",         "sister",       false, false, false },
    { "bedroom_parents", "bedroom",         "parents",      true,  false, true  },
    { "living_room",     "living_room",     "shared",       false, false, false },
    { "kitchen_dining",  "kitchen_dining",  "shared",       false, false, false },
    { "toilets",         "toilet",          "restricted",   false, true,  true  },
    { "balcony",         "balcony",         "restricted",   false, false, false },
    { "utility",         "utility",         "restricted",   false, false, false }
};

static const RoomTemplate NAVI_MUMBAI_TEMPLATES[] = {
    { "N1", "entry",            "shared",        false, false, false },
    { "N2", "living_room",      "shared",        false, false, false },
    { "N3", "terrace",          "restricted",    false, false, false },
    { "N4", "kitchen_dining",   "shared",        false, false, false },
    { "N5", "bedroom",          "parents",       true,  false, true  },
    { "N6", "bedroom",          "owner_sister",  false, false, false },
    { "N7", "toilet",           "restricted",    false, true,  true  },
    { "N8", "toilet",           "restricted",    false, true,  true  }
};

static char s_home_id[16] = "kolhapur";
static const RoomTemplate* s_templates = nullptr;
static size_t s_template_count = 0;
static RoomRecord s_records[ROOM_IDENTITY_MAX_ROOMS];
static RoomObservation s_observation = {};
static RoomIdentityStatus s_status = {};
static bool s_fs_ready = false;
static bool s_dirty = false;
static uint32_t s_dirty_since_ms = 0;
static bool s_persist_pending = false;

static void copy_str(char* dst, size_t dst_len, const char* src) {
    if (!dst || dst_len == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

static bool streq(const char* a, const char* b) {
    return a && b && strcmp(a, b) == 0;
}

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static void normalize_token(char* token) {
    if (!token) return;
    for (; *token; ++token) {
        *token = (char)tolower((unsigned char)*token);
    }
}

static bool is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static bool parse_iso8601_utc(const char* iso, uint32_t* epoch_out) {
    if (!iso || !epoch_out || strlen(iso) < 19) return false;

    char trimmed[20];
    memcpy(trimmed, iso, 19);
    trimmed[19] = '\0';

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    if (sscanf(trimmed, "%4d-%2d-%2dT%2d:%2d:%2d",
               &year, &month, &day, &hour, &minute, &second) != 6) {
        return false;
    }

    static const uint8_t DAYS_IN_MONTH[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    uint32_t days = 0;
    for (int y = 1970; y < year; ++y) {
        days += is_leap_year(y) ? 366U : 365U;
    }
    for (int m = 1; m < month; ++m) {
        days += DAYS_IN_MONTH[m - 1];
        if (m == 2 && is_leap_year(year)) {
            days += 1;
        }
    }
    days += (uint32_t)(day - 1);

    *epoch_out = (days * 86400UL)
               + ((uint32_t)hour * 3600UL)
               + ((uint32_t)minute * 60UL)
               + (uint32_t)second;
    return true;
}

static void copy_iso8601(char* dst, size_t dst_len, const char* src) {
    if (!dst || dst_len == 0) return;
    if (!src || src[0] == '\0') {
        dst[0] = '\0';
        return;
    }

    if (strlen(src) >= 19) {
        size_t copy_len = (dst_len - 1 < 19) ? dst_len - 1 : 19;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
        return;
    }

    copy_str(dst, dst_len, src);
}

static RoomKind room_kind_from_string(const char* room_type) {
    if (!room_type || room_type[0] == '\0') return ROOM_KIND_ROOM;
    for (uint8_t i = 0; i < ROOM_KIND_COUNT; ++i) {
        if (strcmp(room_type, ROOM_KIND_NAMES[i]) == 0) {
            return (RoomKind)i;
        }
    }
    return ROOM_KIND_ROOM;
}

static const char* room_kind_name(RoomKind kind) {
    if (kind >= ROOM_KIND_COUNT) return ROOM_KIND_NAMES[ROOM_KIND_ROOM];
    return ROOM_KIND_NAMES[kind];
}

static const RoomTemplate* find_template(const char* node) {
    if (!node) return nullptr;
    for (size_t i = 0; i < s_template_count; ++i) {
        if (strcmp(s_templates[i].node, node) == 0) {
            return &s_templates[i];
        }
    }
    return nullptr;
}

static RoomRecord* find_record(const char* node) {
    if (!node) return nullptr;
    for (size_t i = 0; i < s_template_count; ++i) {
        if (strcmp(s_records[i].node, node) == 0) {
            return &s_records[i];
        }
    }
    return nullptr;
}

static RoomKind default_kind_from_template(const RoomTemplate* room) {
    if (!room) return ROOM_KIND_ROOM;
    if (streq(room->semantic_type, "living_room")) return ROOM_KIND_LIVING_AREA;
    if (streq(room->semantic_type, "kitchen_dining")) return ROOM_KIND_KITCHEN_DINING;
    if (streq(room->semantic_type, "entry")) return ROOM_KIND_ENTRY;
    if (streq(room->semantic_type, "terrace")) return ROOM_KIND_TERRACE;
    if (streq(room->semantic_type, "balcony")) return ROOM_KIND_BALCONY;
    if (streq(room->semantic_type, "utility")) return ROOM_KIND_UTILITY;
    if (streq(room->semantic_type, "toilet")) return ROOM_KIND_TOILET;
    if (streq(room->semantic_type, "bedroom")) {
        return room->attached_toilet ? ROOM_KIND_MASTER_BEDROOM : ROOM_KIND_SHARED_BEDROOM;
    }
    return ROOM_KIND_ROOM;
}

static float default_confidence_from_template(const RoomTemplate* room) {
    if (!room) return 0.55f;
    if (room->blocked) return 1.0f;
    if (streq(room->semantic_type, "living_room")) return 0.92f;
    if (streq(room->semantic_type, "kitchen_dining")) return 0.90f;
    if (streq(room->semantic_type, "entry")) return 0.88f;
    if (streq(room->semantic_type, "terrace")) return 0.86f;
    if (streq(room->semantic_type, "balcony")) return 0.84f;
    if (streq(room->semantic_type, "utility")) return 0.82f;
    if (streq(room->semantic_type, "bedroom")) return room->attached_toilet ? 0.86f : 0.76f;
    return 0.55f;
}

static void seed_record_from_template(RoomRecord* record, const RoomTemplate* room) {
    if (!record || !room) return;

    memset(record, 0, sizeof(*record));
    copy_str(record->node, sizeof(record->node), room->node);
    copy_str(record->room_type, sizeof(record->room_type), room_kind_name(default_kind_from_template(room)));
    copy_str(record->owner, sizeof(record->owner), room->owner);
    record->confidence = default_confidence_from_template(room);
    record->alternative_confidence = 0.0f;
    record->last_verified_epoch = 0;
    record->manual_override = false;
    if (room->blocked) {
        copy_str(record->signals, sizeof(record->signals), "semantic,restricted");
    } else if (streq(room->semantic_type, "bedroom")) {
        copy_str(record->signals, sizeof(record->signals), "semantic,structural");
    } else {
        copy_str(record->signals, sizeof(record->signals), "semantic");
    }
}

static void mark_dirty(void) {
    s_dirty = true;
    s_dirty_since_ms = millis();
}

static void build_persistence_path(char* dst, size_t dst_len, bool temp_file) {
    if (!dst || dst_len == 0) return;
    snprintf(dst,
             dst_len,
             temp_file
                ? "/pinctada/homes/%s/room_identity.tmp"
                : "/pinctada/homes/%s/room_identity.json",
             s_home_id);
}

static void publish_json(const JsonDocument& doc) {
    char payload[256];
    serializeJson(doc, payload, sizeof(payload));
    network_broadcast(payload);
}

static void publish_room_ack(const char* action, const char* reason) {
    StaticJsonDocument<192> doc;
    doc["event"] = "ROOM_ACK";
    doc["action"] = action ? action : "";
    doc["home"] = s_home_id;
    if (s_status.node[0] != '\0') doc["node"] = s_status.node;
    if (s_status.room_type[0] != '\0') doc["roomType"] = s_status.room_type;
    if (s_status.owner[0] != '\0') doc["owner"] = s_status.owner;
    if (reason && reason[0] != '\0') doc["reason"] = reason;
    publish_json(doc);
}

static void publish_room_rejected(const char* action, const char* reason) {
    StaticJsonDocument<192> doc;
    doc["event"] = "ROOM_REJECTED";
    doc["action"] = action ? action : "";
    doc["reason"] = reason ? reason : "invalid";
    publish_json(doc);
}

static bool record_is_stale(const RoomRecord* record) {
    if (!record) return true;
    if (record->confidence < 0.70f) return true;
    if (!s_observation.has_observation) return false;
    if (record->last_verified_epoch == 0 || s_observation.observed_at_epoch == 0) return false;
    return s_observation.observed_at_epoch > record->last_verified_epoch
        && (s_observation.observed_at_epoch - record->last_verified_epoch) >= ROOM_IDENTITY_REVALIDATE_AFTER_S;
}

static bool resolve_home_profile(const char* home_id) {
    if (!home_id || home_id[0] == '\0') return false;

    if (strcmp(home_id, "kolhapur") == 0) {
        s_templates = KOLHAPUR_TEMPLATES;
        s_template_count = sizeof(KOLHAPUR_TEMPLATES) / sizeof(KOLHAPUR_TEMPLATES[0]);
    } else if (strcmp(home_id, "navi_mumbai") == 0) {
        s_templates = NAVI_MUMBAI_TEMPLATES;
        s_template_count = sizeof(NAVI_MUMBAI_TEMPLATES) / sizeof(NAVI_MUMBAI_TEMPLATES[0]);
    } else {
        return false;
    }

    copy_str(s_home_id, sizeof(s_home_id), home_id);

    for (size_t i = 0; i < s_template_count; ++i) {
        seed_record_from_template(&s_records[i], &s_templates[i]);
    }
    for (size_t i = s_template_count; i < ROOM_IDENTITY_MAX_ROOMS; ++i) {
        memset(&s_records[i], 0, sizeof(s_records[i]));
    }

    s_observation.current_node[0] = '\0';
    s_observation.detected_person[0] = '\0';
    s_observation.person_confidence = 0.0f;
    s_observation.local_hour = -1;
    s_observation.door_closed = false;
    s_observation.silence_detected = false;
    s_observation.activity_detected = false;
    s_observation.observed_at[0] = '\0';
    s_observation.observed_at_epoch = 0;
    s_observation.has_observation = false;

    memset(&s_status, 0, sizeof(s_status));
    copy_str(s_status.home_id, sizeof(s_status.home_id), s_home_id);
    copy_str(s_status.room_type, sizeof(s_status.room_type), "room");
    copy_str(s_status.owner, sizeof(s_status.owner), "unknown");
    s_status.confidence = 0.0f;
    s_status.needs_review = true;
    return true;
}

static void overlay_record_from_json(RoomRecord* record, JsonObjectConst json) {
    if (!record) return;
    copy_str(record->room_type, sizeof(record->room_type), json["room_type"] | record->room_type);
    copy_str(record->owner, sizeof(record->owner), json["owner"] | record->owner);
    copy_str(record->alternative_room_type,
             sizeof(record->alternative_room_type),
             json["alternative_room_type"] | record->alternative_room_type);
    copy_str(record->signals, sizeof(record->signals), json["signals"] | record->signals);
    copy_iso8601(record->last_verified, sizeof(record->last_verified), json["last_verified"] | record->last_verified);
    record->confidence = json["confidence"] | record->confidence;
    record->alternative_confidence = json["alternative_confidence"] | record->alternative_confidence;
    record->last_verified_epoch = json["last_verified_epoch"] | record->last_verified_epoch;
    record->manual_override = json["manual_override"] | record->manual_override;
}

static void load_persisted_records(void) {
    if (!s_fs_ready) return;

    char path[80];
    build_persistence_path(path, sizeof(path), false);

    File file = SPIFFS.open(path, FILE_READ);
    if (!file) return;

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        Serial.printf("[ROOM] Failed to read persisted identity map (%s)\n", error.c_str());
        return;
    }

    JsonArrayConst rooms = doc["rooms"].as<JsonArrayConst>();
    for (JsonObjectConst room : rooms) {
        const char* node = room["node"] | "";
        RoomRecord* record = find_record(node);
        if (record) {
            overlay_record_from_json(record, room);
        }
    }
}

static bool persist_records(void) {
    if (!s_fs_ready) return false;

    StaticJsonDocument<2048> doc;
    doc["home"] = s_home_id;
    JsonArray rooms = doc.createNestedArray("rooms");

    for (size_t i = 0; i < s_template_count; ++i) {
        JsonObject room = rooms.createNestedObject();
        room["node"] = s_records[i].node;
        room["room_type"] = s_records[i].room_type;
        room["owner"] = s_records[i].owner;
        room["confidence"] = s_records[i].confidence;
        room["alternative_room_type"] = s_records[i].alternative_room_type;
        room["alternative_confidence"] = s_records[i].alternative_confidence;
        room["signals"] = s_records[i].signals;
        room["last_verified"] = s_records[i].last_verified;
        room["last_verified_epoch"] = s_records[i].last_verified_epoch;
        room["manual_override"] = s_records[i].manual_override;
    }

    char path[80];
    char temp_path[80];
    build_persistence_path(path, sizeof(path), false);
    build_persistence_path(temp_path, sizeof(temp_path), true);

    SPIFFS.remove(temp_path);
    File temp = SPIFFS.open(temp_path, FILE_WRITE);
    if (!temp) {
        Serial.println(F("[ROOM] Failed to open temp room identity file"));
        return false;
    }

    if (serializeJson(doc, temp) == 0) {
        temp.close();
        SPIFFS.remove(temp_path);
        Serial.println(F("[ROOM] Failed to write room identity file"));
        return false;
    }
    temp.flush();
    temp.close();

    SPIFFS.remove(path);
    if (!SPIFFS.rename(temp_path, path)) {
        SPIFFS.remove(temp_path);
        Serial.println(F("[ROOM] Failed to commit room identity file"));
        return false;
    }

    s_dirty = false;
    Serial.printf("[ROOM] Persisted room identity for %s\n", s_home_id);
    return true;
}

static void append_signal(char* dst, size_t dst_len, const char* signal) {
    if (!dst || !signal || dst_len == 0) return;
    if (dst[0] != '\0') {
        strncat(dst, ",", dst_len - strlen(dst) - 1);
    }
    strncat(dst, signal, dst_len - strlen(dst) - 1);
}

static bool is_night_hour(int8_t hour) {
    return hour >= 21 || hour < 6;
}

static bool is_day_hour(int8_t hour) {
    return hour >= 6 && hour < 21;
}

static void resolve_owner(const RoomTemplate* room, RoomKind kind, char* dst, size_t dst_len) {
    if (!room || !dst || dst_len == 0) return;

    if (kind == ROOM_KIND_MASTER_BEDROOM) {
        copy_str(dst, dst_len, "parents");
        return;
    }

    if (kind == ROOM_KIND_SHARED_BEDROOM && room->owner[0] != '\0') {
        copy_str(dst, dst_len, room->owner);
        return;
    }

    if (room->owner[0] != '\0') {
        copy_str(dst, dst_len, room->owner);
        return;
    }

    copy_str(dst, dst_len, "unknown");
}

static void update_status_from_record(const RoomTemplate* room, const RoomRecord* record) {
    if (!room || !record) return;

    memset(&s_status, 0, sizeof(s_status));
    copy_str(s_status.home_id, sizeof(s_status.home_id), s_home_id);
    copy_str(s_status.node, sizeof(s_status.node), record->node);
    copy_str(s_status.room_type, sizeof(s_status.room_type), record->room_type);
    copy_str(s_status.owner, sizeof(s_status.owner), record->owner);
    copy_str(s_status.alternative_room_type,
             sizeof(s_status.alternative_room_type),
             record->alternative_room_type);
    copy_str(s_status.signals, sizeof(s_status.signals), record->signals);
    copy_str(s_status.last_verified, sizeof(s_status.last_verified), record->last_verified);
    s_status.confidence = clamp01(record->confidence);
    s_status.alternative_confidence = clamp01(record->alternative_confidence);
    s_status.needs_review = record_is_stale(record) || s_status.confidence < 0.60f;
    s_status.restricted = room->blocked
        || room_kind_from_string(record->room_type) == ROOM_KIND_TOILET
        || room_kind_from_string(record->room_type) == ROOM_KIND_BALCONY
        || room_kind_from_string(record->room_type) == ROOM_KIND_RESTRICTED
        || (room_kind_from_string(record->room_type) == ROOM_KIND_MASTER_BEDROOM
            && room->night_restricted
            && s_observation.local_hour >= 0
            && is_night_hour(s_observation.local_hour));
}

static void resolve_current_room(void) {
    if (s_observation.current_node[0] == '\0') return;

    const RoomTemplate* room = find_template(s_observation.current_node);
    RoomRecord* record = find_record(s_observation.current_node);
    if (!room || !record) {
        memset(&s_status, 0, sizeof(s_status));
        copy_str(s_status.home_id, sizeof(s_status.home_id), s_home_id);
        copy_str(s_status.node, sizeof(s_status.node), s_observation.current_node);
        copy_str(s_status.room_type, sizeof(s_status.room_type), "room");
        copy_str(s_status.owner, sizeof(s_status.owner), "unknown");
        copy_str(s_status.signals, sizeof(s_status.signals), "unknown");
        s_status.confidence = 0.0f;
        s_status.needs_review = true;
        s_status.restricted = true;
        return;
    }

    if (record->manual_override) {
        update_status_from_record(room, record);
        return;
    }

    float scores[ROOM_KIND_COUNT] = { 0.0f };
    float total_weight = 0.0f;
    char signals[48] = "";

    const float semantic_weight = 0.35f;
    total_weight += semantic_weight;
    append_signal(signals, sizeof(signals), "semantic");

    RoomKind semantic_kind = default_kind_from_template(room);
    scores[semantic_kind] += semantic_weight;
    if (streq(room->semantic_type, "bedroom")) {
        scores[ROOM_KIND_ROOM] += 0.10f;
    }

    if (streq(room->semantic_type, "bedroom")) {
        const float structural_weight = 0.20f;
        total_weight += structural_weight;
        append_signal(signals, sizeof(signals), "structural");
        scores[room->attached_toilet ? ROOM_KIND_MASTER_BEDROOM : ROOM_KIND_SHARED_BEDROOM] += structural_weight;
    }

    if (s_observation.detected_person[0] != '\0' && !streq(s_observation.detected_person, "unknown")) {
        const float human_weight = 0.20f * clamp01(s_observation.person_confidence);
        if (human_weight > 0.0f) {
            total_weight += human_weight;
            append_signal(signals, sizeof(signals), "human");
            if (streq(s_observation.detected_person, "parents")) {
                scores[ROOM_KIND_MASTER_BEDROOM] += human_weight;
            } else if (streq(s_observation.detected_person, "owner")
                    || streq(s_observation.detected_person, "sister")
                    || streq(s_observation.detected_person, "owner_sister")) {
                scores[ROOM_KIND_SHARED_BEDROOM] += human_weight;
            }
        }
    }

    if (s_observation.local_hour >= 0) {
        const float context_weight = 0.10f;
        total_weight += context_weight;
        append_signal(signals, sizeof(signals), "context");

        if (is_night_hour(s_observation.local_hour) && room->night_restricted
                && s_observation.door_closed && s_observation.silence_detected) {
            scores[ROOM_KIND_MASTER_BEDROOM] += context_weight;
        } else if (streq(room->semantic_type, "bedroom")
                && is_day_hour(s_observation.local_hour)
                && s_observation.activity_detected) {
            scores[ROOM_KIND_SHARED_BEDROOM] += context_weight;
        } else {
            scores[semantic_kind] += context_weight;
        }
    }

    if (record->confidence > 0.0f) {
        const float memory_weight = 0.15f * clamp01(record->confidence);
        total_weight += memory_weight;
        append_signal(signals, sizeof(signals), "memory");
        scores[room_kind_from_string(record->room_type)] += memory_weight;
    }

    RoomKind top_kind = ROOM_KIND_ROOM;
    RoomKind alt_kind = ROOM_KIND_ROOM;
    float top_score = -1.0f;
    float alt_score = -1.0f;
    for (uint8_t i = 0; i < ROOM_KIND_COUNT; ++i) {
        if (scores[i] > top_score) {
            alt_kind = top_kind;
            alt_score = top_score;
            top_kind = (RoomKind)i;
            top_score = scores[i];
        } else if (scores[i] > alt_score) {
            alt_kind = (RoomKind)i;
            alt_score = scores[i];
        }
    }

    float confidence = (total_weight > 0.0f) ? clamp01(top_score / total_weight) : 0.0f;
    float alternative_confidence = (total_weight > 0.0f && alt_score > 0.0f)
        ? clamp01(alt_score / total_weight)
        : 0.0f;

    if (confidence < 0.60f) {
        top_kind = ROOM_KIND_ROOM;
    }

    copy_str(record->room_type, sizeof(record->room_type), room_kind_name(top_kind));
    resolve_owner(room, top_kind, record->owner, sizeof(record->owner));
    copy_str(record->alternative_room_type,
             sizeof(record->alternative_room_type),
             (alternative_confidence > 0.0f) ? room_kind_name(alt_kind) : "");
    record->confidence = confidence;
    record->alternative_confidence = alternative_confidence;
    copy_str(record->signals, sizeof(record->signals), signals);
    if (s_observation.observed_at[0] != '\0') {
        copy_str(record->last_verified, sizeof(record->last_verified), s_observation.observed_at);
        record->last_verified_epoch = s_observation.observed_at_epoch;
    }

    update_status_from_record(room, record);

    if (confidence < 0.60f) {
        copy_str(s_status.room_type, sizeof(s_status.room_type), "room");
        s_status.restricted = true;
    }

    if (confidence > 0.80f) {
        mark_dirty();
    }
}

static bool handle_home_command(char* token_ctx) {
    char* home_id = strtok_r(nullptr, " ", &token_ctx);
    if (!home_id) {
        publish_room_rejected("HOME", "missing home id");
        return false;
    }

    normalize_token(home_id);
    if (!resolve_home_profile(home_id)) {
        publish_room_rejected("HOME", "unknown home");
        return false;
    }

    load_persisted_records();
    publish_room_ack("HOME", "home selected");
    return true;
}

static bool handle_observe_command(char* token_ctx) {
    char* node = strtok_r(nullptr, " ", &token_ctx);
    char* person = strtok_r(nullptr, " ", &token_ctx);
    char* person_conf = strtok_r(nullptr, " ", &token_ctx);
    char* hour = strtok_r(nullptr, " ", &token_ctx);
    char* door = strtok_r(nullptr, " ", &token_ctx);
    char* silent = strtok_r(nullptr, " ", &token_ctx);
    char* active = strtok_r(nullptr, " ", &token_ctx);
    char* observed_at = strtok_r(nullptr, " ", &token_ctx);

    if (!node || !person || !person_conf || !hour || !door || !silent || !active || !observed_at) {
        publish_room_rejected("OBSERVE", "snapshot incomplete");
        return false;
    }

    copy_str(s_observation.current_node, sizeof(s_observation.current_node), node);
    copy_str(s_observation.detected_person, sizeof(s_observation.detected_person), person);

    float parsed_confidence = (float)atof(person_conf);
    if (parsed_confidence > 1.0f) {
        parsed_confidence /= 100.0f;
    }
    s_observation.person_confidence = clamp01(parsed_confidence);
    s_observation.local_hour = (int8_t)atoi(hour);
    s_observation.door_closed = atoi(door) != 0;
    s_observation.silence_detected = atoi(silent) != 0;
    s_observation.activity_detected = atoi(active) != 0;
    copy_iso8601(s_observation.observed_at, sizeof(s_observation.observed_at), observed_at);
    s_observation.observed_at_epoch = 0;
    parse_iso8601_utc(observed_at, &s_observation.observed_at_epoch);
    s_observation.has_observation = true;

    resolve_current_room();
    publish_room_ack("OBSERVE", "snapshot applied");
    return true;
}

static bool handle_set_command(char* token_ctx) {
    char* node = strtok_r(nullptr, " ", &token_ctx);
    char* room_type = strtok_r(nullptr, " ", &token_ctx);
    char* owner = strtok_r(nullptr, " ", &token_ctx);
    if (!node || !room_type || !owner) {
        publish_room_rejected("SET", "override incomplete");
        return false;
    }

    RoomRecord* record = find_record(node);
    const RoomTemplate* room = find_template(node);
    if (!record || !room) {
        publish_room_rejected("SET", "unknown node");
        return false;
    }

    normalize_token(room_type);
    normalize_token(owner);

    copy_str(record->room_type, sizeof(record->room_type), room_type);
    copy_str(record->owner, sizeof(record->owner), owner);
    copy_str(record->signals, sizeof(record->signals), "manual");
    copy_str(record->last_verified, sizeof(record->last_verified), s_observation.observed_at);
    record->last_verified_epoch = s_observation.observed_at_epoch;
    record->confidence = 1.0f;
    record->alternative_room_type[0] = '\0';
    record->alternative_confidence = 0.0f;
    record->manual_override = true;

    if (streq(s_observation.current_node, node)) {
        update_status_from_record(room, record);
    }

    mark_dirty();
    publish_room_ack("SET", "override saved");
    return true;
}

static bool handle_clear_command(char* token_ctx) {
    char* node = strtok_r(nullptr, " ", &token_ctx);
    if (!node) {
        publish_room_rejected("CLEAR", "missing node");
        return false;
    }

    RoomRecord* record = find_record(node);
    const RoomTemplate* room = find_template(node);
    if (!record || !room) {
        publish_room_rejected("CLEAR", "unknown node");
        return false;
    }

    seed_record_from_template(record, room);
    if (streq(s_observation.current_node, node)) {
        resolve_current_room();
    }

    mark_dirty();
    publish_room_ack("CLEAR", "override cleared");
    return true;
}

} // namespace

RoomIdentityManager roomIdentityManager;

void RoomIdentityManager::init(void) {
    s_fs_ready = false;
    s_dirty = false;
    s_dirty_since_ms = 0;
    s_persist_pending = false;
    resolve_home_profile(s_home_id);
    loadRoomIdentity();
    Serial.printf("[ROOM] init -> home=%s\n", s_home_id);
}

void RoomIdentityManager::updateNode(const char* node_id) {
    if (!node_id || node_id[0] == '\0') {
        return;
    }

    if (strcmp(s_observation.current_node, node_id) != 0) {
        copy_str(s_observation.current_node, sizeof(s_observation.current_node), node_id);
        resolve_current_room();
    }
}

void RoomIdentityManager::observe(const ::RoomObservation& obs) {
    if (!obs.valid) {
        return;
    }

    copy_str(s_observation.detected_person, sizeof(s_observation.detected_person), obs.detected_person);
    if (obs.human_detected && s_observation.detected_person[0] == '\0') {
        copy_str(s_observation.detected_person, sizeof(s_observation.detected_person), "unknown");
    }

    s_observation.person_confidence = clamp01(obs.confidence);
    s_observation.local_hour = obs.has_local_hour ? obs.local_hour : -1;
    s_observation.door_closed = obs.door_closed;
    s_observation.silence_detected = obs.silence_detected;
    s_observation.activity_detected = obs.activity_detected || obs.motion_detected;
    s_observation.has_observation = true;

    if (s_observation.current_node[0] != '\0') {
        resolve_current_room();
    }
}

RoomProfile RoomIdentityManager::getRoomProfile(void) const {
    RoomProfile profile = {};
    copy_str(profile.node, sizeof(profile.node), s_status.node);
    copy_str(profile.room_type, sizeof(profile.room_type), s_status.room_type);
    copy_str(profile.owner, sizeof(profile.owner), s_status.owner);
    profile.confidence = s_status.confidence;
    profile.restricted = s_status.restricted;
    return profile;
}

void RoomIdentityManager::loadRoomIdentity(void) {
}

void RoomIdentityManager::queueRoomIdentitySave(void) {
    s_persist_pending = true;
}

void RoomIdentityManager::processPersistence(void) {
    if (s_persist_pending) {
        s_persist_pending = false;
    }
}

void RoomIdentityManager::update(uint32_t now_ms) {
    (void)now_ms;

    if (s_observation.current_node[0] != '\0' && s_status.node[0] == '\0') {
        resolve_current_room();
    }

    if (s_status.confidence > 0.80f) {
        queueRoomIdentitySave();
    }

    processPersistence();
}

bool RoomIdentityManager::handleCommand(const char* cmd) {
    if (!cmd || strncmp(cmd, "ROOM ", 5) != 0) {
        return false;
    }

    char buffer[192];
    copy_str(buffer, sizeof(buffer), cmd);

    char* token_ctx = nullptr;
    char* scope = strtok_r(buffer, " ", &token_ctx);
    char* action = strtok_r(nullptr, " ", &token_ctx);
    if (!scope || !action) {
        publish_room_rejected("", "invalid command");
        return true;
    }

    normalize_token(action);

    if (strcmp(action, "home") == 0) return handle_home_command(token_ctx);
    if (strcmp(action, "observe") == 0) return handle_observe_command(token_ctx);
    if (strcmp(action, "set") == 0) return handle_set_command(token_ctx);
    if (strcmp(action, "clear") == 0) return handle_clear_command(token_ctx);

    publish_room_rejected(action, "unknown room command");
    return true;
}

void RoomIdentityManager::buildTelemetry(JsonDocument& doc) const {
    const RoomProfile profile = getRoomProfile();

    doc[TEL_KEY_HOME] = s_home_id;
    JsonObject room = doc.createNestedObject(TEL_KEY_ROOM);
    room[TEL_KEY_NODE] = profile.node;
    room[TEL_KEY_TYPE] = profile.room_type;
    room[TEL_KEY_OWNER] = profile.owner;
    room[TEL_KEY_CONFIDENCE] = profile.confidence;
    room[TEL_KEY_RESTRICTED] = profile.restricted;
}

bool RoomIdentityManager::getStatus(RoomIdentityStatus* out) const {
    if (!out) return false;
    *out = s_status;
    return true;
}

void room_identity_manager_init(void) {
    roomIdentityManager.init();
}

void room_identity_manager_update(void) {
    roomIdentityManager.update(millis());
}

bool room_identity_handle_command(const char* cmd) {
    return roomIdentityManager.handleCommand(cmd);
}

void room_identity_build_telemetry(JsonDocument& doc) {
    roomIdentityManager.buildTelemetry(doc);
}

bool room_identity_status(RoomIdentityStatus* out) {
    return roomIdentityManager.getStatus(out);
}
