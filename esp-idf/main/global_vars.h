#pragma once

#include <time.h>
#include "time_utils.h"

const char *get_var_time() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    static char time_str[10];
    snprintf(time_str, sizeof(time_str), "%d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    return time_str;
}

void set_var_time(const char *value) {
    // Do nothing
}

const char *get_var_date() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    static char date_str[64];
    format_long_date(date_str, sizeof(date_str), &timeinfo);
    return date_str;
}

void set_var_date(const char *value) {
    // Do nothing
}
