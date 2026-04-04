#pragma once

#include <time.h>
#include <stdio.h>
#include <string.h>

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
    a_midnight.tm_hour = 0; a_midnight.tm_min = 0; a_midnight.tm_sec = 0; a_midnight.tm_isdst = -1;
    b_midnight.tm_hour = 0; b_midnight.tm_min = 0; b_midnight.tm_sec = 0; b_midnight.tm_isdst = -1;
    return (int)((mktime(&a_midnight) - mktime(&b_midnight)) / 86400);
}

// Formats a short date relative to now: "Today", "Tomorrow", day-of-week (within 7 days),
// or "D Mon" (e.g. "15 Apr") for dates further away or in the past.
// epoch=0 → empty string.
inline void format_short_date(char *buf, size_t buf_size, time_t epoch, time_t now) {
    if (epoch == 0) {
        buf[0] = '\0';
        return;
    }
    struct tm t;
    localtime_r(&epoch, &t);
    int diff = days_between(epoch, now);
    if (diff == 0) {
        strlcpy(buf, "Today", buf_size);
    } else if (diff == 1) {
        strlcpy(buf, "Tomorrow", buf_size);
    } else if (diff > 1 && diff < 7) {
        strftime(buf, buf_size, "%A", &t);
    } else {
        char month_abbr[8];
        strftime(month_abbr, sizeof(month_abbr), "%b", &t);
        snprintf(buf, buf_size, "%d %s", t.tm_mday, month_abbr);
    }
}

// Formats a last-seen timestamp:
//   same calendar day       → "H:MM"         e.g. "19:34"
//   1 calendar day ago      → "Yesterday"
//   2–7 calendar days ago   → "Last Weekday"  e.g. "Last Wednesday"
//   >7 calendar days ago    → "D Mon"         e.g. "24 Mar"
// epoch=0 → empty string.
inline void format_last_seen(char *buf, size_t buf_size, time_t epoch, time_t now) {
    if (epoch == 0) {
        buf[0] = '\0';
        return;
    }
    int days_ago = -days_between(epoch, now);
    struct tm t;
    localtime_r(&epoch, &t);
    if (days_ago == 0) {
        snprintf(buf, buf_size, "%d:%02d", t.tm_hour, t.tm_min);
    } else if (days_ago == 1) {
        strlcpy(buf, "Yesterday", buf_size);
    } else if (days_ago >= 2 && days_ago <= 7) {
        strftime(buf, buf_size, "%A", &t);
    } else {
        char month_abbr[8];
        strftime(month_abbr, sizeof(month_abbr), "%b", &t);
        snprintf(buf, buf_size, "%d %s", t.tm_mday, month_abbr);
    }
}

// Formats a time_t as "HH:MM:SS DD/MM/YYYY". epoch=0 → empty string.
inline void format_date_time(char *buf, size_t buf_size, time_t epoch) {
    if (epoch == 0) {
        buf[0] = '\0';
        return;
    }
    struct tm t;
    localtime_r(&epoch, &t);
    char month[16];
    strftime(month, sizeof(month), "%B", &t);
    snprintf(buf, buf_size, "%d %s %04d %02d:%02d",
        t.tm_mday, month, t.tm_year + 1900, t.tm_hour, t.tm_min);
}

// Formats a  lead time relative to now: "Today", "Tomorrow", or the number of days in the future (e.g. "3 days"). lead_epoch=0 → empty string.
inline void format_lead_time(char *buf, size_t buf_size, time_t epoch, time_t now) {
    if (epoch == 0) {
        buf[0] = '\0';
        return;
    }
    struct tm t;
    localtime_r(&epoch, &t);
    int diff = days_between(epoch, now);
    if (diff == 0) {
        strlcpy(buf, "Today", buf_size);
    } else if (diff == 1) {
        strlcpy(buf, "Tomorrow", buf_size);
    } else if (diff > 1) {
        snprintf(buf, buf_size, "%d days", diff);
    } else {
        buf[0] = '\0'; // Past date: empty string
    }
}