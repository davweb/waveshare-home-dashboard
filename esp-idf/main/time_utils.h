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

// Formats a bus arrival due label into buf given a due epoch and the current time.
// epoch=0 or past → "due". <61s → "1 min". ≤60 mins → "X mins" (ceiling). >60 mins → "H:MM" local time.
inline void format_due_label(char *buf, size_t buf_size, time_t due_epoch, time_t now) {
    int due_seconds = due_epoch > 0 ? (int)(due_epoch - now) : 0;

    if (due_seconds <= 0) {
        strlcpy(buf, "due", buf_size);
    } else if (due_seconds < 61) {
        strlcpy(buf, "1 min", buf_size);
    } else {
        int minutes = (due_seconds + 59) / 60;

        if (minutes <= 60) {
            snprintf(buf, buf_size, "%d mins", minutes);
        } else {
            struct tm t;
            localtime_r(&due_epoch, &t);
            snprintf(buf, buf_size, "%d:%02d", t.tm_hour, t.tm_min);
        }
    }
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
