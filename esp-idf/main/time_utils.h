#pragma once

#include <time.h>
#include <stdio.h>

// Formats a struct tm as "Weekday D Month", e.g. "Monday 3 March".
// Uses snprintf for the day number to avoid leading zeros/spaces from strftime.
inline void format_long_date(char *buf, size_t buf_size, const struct tm *t) {
    char weekday[24], month[24];
    strftime(weekday, sizeof(weekday), "%A", t);
    strftime(month, sizeof(month), "%B", t);
    snprintf(buf, buf_size, "%s %d %s", weekday, t->tm_mday, month);
}

// Returns the number of whole days between two time_t values (a - b),
// calculated using midnight-normalised local times to avoid DST/partial-day issues.
inline int days_between(time_t a, time_t b) {
    struct tm a_midnight, b_midnight;
    localtime_r(&a, &a_midnight);
    localtime_r(&b, &b_midnight);
    a_midnight.tm_hour = 0; a_midnight.tm_min = 0; a_midnight.tm_sec = 0;
    b_midnight.tm_hour = 0; b_midnight.tm_min = 0; b_midnight.tm_sec = 0;
    return (int)((mktime(&a_midnight) - mktime(&b_midnight)) / 86400);
}
