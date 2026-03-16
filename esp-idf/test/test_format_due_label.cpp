// Host-compilable unit tests for format_due_label().
// Build and run:  c++ -std=c++17 test_format_due_label.cpp -o test_format_due_label && ./test_format_due_label

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "../main/time_utils.h"

static int passed = 0;
static int failed = 0;

#define CHECK(desc, expr) \
    do { \
        if (expr) { printf("  PASS  %s\n", desc); passed++; } \
        else      { printf("  FAIL  %s\n", desc); failed++; } \
    } while (0)

// Fixed "now": 2025-06-10 12:00:00 UTC
static time_t make_now() {
    struct tm t = {};
    t.tm_year = 125; t.tm_mon = 5; t.tm_mday = 10;
    t.tm_hour = 12;  t.tm_min = 0; t.tm_sec  = 0;
    t.tm_isdst = 0;
    return timegm(&t);
}

static std::string label(time_t due_epoch, time_t now) {
    char buf[16];
    format_due_label(buf, sizeof(buf), due_epoch, now);
    return buf;
}

int main() {
    setenv("TZ", "UTC", 1);
    tzset();

    time_t now = make_now();

    printf("format_due_label tests\n");

    // epoch = 0
    CHECK("epoch 0 → due",           label(0, now) == "due");

    // in the past
    CHECK("past epoch → due",        label(now - 60, now) == "due");

    // exactly now
    CHECK("epoch == now → due",      label(now, now) == "due");

    // 1 second ahead
    CHECK("1 second → 1 min",        label(now + 1, now) == "1 min");

    // boundary: 60 seconds → still "1 min"
    CHECK("60 seconds → 1 min",      label(now + 60, now) == "1 min");

    // boundary: 61 seconds → "2 mins" (ceiling)
    CHECK("61 seconds → 2 mins",     label(now + 61, now) == "2 mins");

    // 120 seconds → "2 mins" (exact, ceiling of 2.0 = 2)
    CHECK("120 seconds → 2 mins",    label(now + 120, now) == "2 mins");

    // 121 seconds → "3 mins" (ceiling of 2.017 = 3)
    CHECK("121 seconds → 3 mins",    label(now + 121, now) == "3 mins");

    // 5 minutes exactly
    CHECK("300 seconds → 5 mins",    label(now + 300, now) == "5 mins");

    // 60 minutes exactly → "60 mins" (boundary of minutes <= 60)
    CHECK("3600 seconds → 60 mins",  label(now + 3600, now) == "60 mins");

    // 3601 seconds → minutes = ceil(3601/60) = 61 → time format
    // now is 12:00:00 UTC, +3601s = 13:00:01 UTC → "13:00"
    CHECK("3601 seconds → 13:00",    label(now + 3601, now) == "13:00");

    // 90 minutes → 13:30
    CHECK("5400 seconds → 13:30",    label(now + 5400, now) == "13:30");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
