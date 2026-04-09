#pragma once

#include <cJSON.h>

// Helpers to safely extract values from a cJSON object with defaults

inline const char *cjson_str(cJSON *obj, const char *key, const char *def = "") {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    return (cJSON_IsString(item) && item->valuestring) ? item->valuestring : def;
}

inline int cjson_int(cJSON *obj, const char *key, int def = 0) {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    return cJSON_IsNumber(item) ? item->valueint : def;
}

inline long cjson_long(cJSON *obj, const char *key, long def = 0L) {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    return cJSON_IsNumber(item) ? (long)item->valuedouble : def;
}

inline bool cjson_bool(cJSON *obj, const char *key, bool def = false) {
    cJSON *item = cJSON_GetObjectItem(obj, key);
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : def;
}
