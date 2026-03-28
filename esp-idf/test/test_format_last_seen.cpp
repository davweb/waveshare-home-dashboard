// Host-compilable unit tests for format_last_seen().
// Build and run:  c++ -std=c++17 test_format_last_seen.cpp -o test_format_last_seen && ./test_format_last_seen

#include <clocale>
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

static time_t make_datetime(int year, int month, int day, int hour, int minute) {
    struct tm t = {};
    t.tm_year  = year - 1900;
    t.tm_mon   = month - 1;
    t.tm_mday  = day;
    t.tm_hour  = hour;
    t.tm_min   = minute;
    t.tm_sec   = 0;
    t.tm_isdst = -1;
    return mktime(&t);
}

static std::string fmt(time_t epoch, time_t now) {
    char buf[32];
    format_last_seen(buf, sizeof(buf), epoch, now);
    return buf;
}

int main() {
    setlocale(LC_TIME, "C"); // ensure English month names

    // Fixed reference: 2025-06-10 15:00
    time_t now = make_datetime(2025, 6, 10, 15, 0);

    printf("format_last_seen tests\n");

    // epoch == 0 → empty string
    CHECK("epoch 0 → empty",                fmt(0, now) == "");

    // Within 24h → H:MM (no leading zero on hour, leading zero on minute)
    CHECK("single-digit hour, zero min → 9:00",  fmt(make_datetime(2025, 6, 10,  9,  0), now) == "9:00");
    CHECK("single-digit hour, non-zero min → 9:05", fmt(make_datetime(2025, 6, 10, 9, 5), now) == "9:05");
    CHECK("double-digit hour → 12:30",       fmt(make_datetime(2025, 6, 10, 12, 30), now) == "12:30");
    CHECK("midnight → 0:00",                 fmt(make_datetime(2025, 6, 10,  0,  0), now) == "0:00");
    CHECK("just before 24h boundary → time", fmt(now - 86399, now).find(":") != std::string::npos);

    // Exactly 24h ago → date (now - epoch == 86400, not < 86400)
    CHECK("exactly 24h ago → date",          fmt(now - 86400, now).find(":") == std::string::npos);

    // Older than 24h → D Mon (no leading zero on day)
    CHECK("yesterday → 9 Jun",               fmt(make_datetime(2025, 6,  9, 12,  0), now) == "9 Jun");
    CHECK("single-digit day → 3 Jun",        fmt(make_datetime(2025, 6,  3, 12,  0), now) == "3 Jun");
    CHECK("double-digit day → 10 May",       fmt(make_datetime(2025, 5, 10, 12,  0), now) == "10 May");
    CHECK("month boundary → 31 May",         fmt(make_datetime(2025, 5, 31, 12,  0), now) == "31 May");
    CHECK("far past → 1 Jan",                fmt(make_datetime(2025, 1,  1, 12,  0), now) == "1 Jan");
    CHECK("year boundary → 31 Dec",          fmt(make_datetime(2024, 12, 31, 12,  0), now) == "31 Dec");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
