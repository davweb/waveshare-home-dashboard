// Host-compilable unit tests for format_lead_time().
// Build and run:  c++ -std=c++17 test_format_lead_time.cpp -o test_format_lead_time && ./test_format_lead_time

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

// Build a time_t for a given local date at noon (avoids DST midnight edge cases).
static time_t make_date(int year, int month, int day) {
    struct tm t = {};
    t.tm_year  = year - 1900;
    t.tm_mon   = month - 1;
    t.tm_mday  = day;
    t.tm_hour  = 12;
    t.tm_isdst = -1;
    return mktime(&t);
}

static std::string fmt(time_t epoch, time_t test_date) {
    char buf[32];
    format_lead_time(buf, sizeof(buf), epoch, test_date);
    return buf;
}

int main() {
    setlocale(LC_TIME, "C"); // ensure English day/month names

    // Use 2025-06-10 (Tuesday) as the fixed reference date.
    time_t test_date = make_date(2025, 6, 10);

    printf("format_lead_time tests\n");

    // epoch == 0 → empty string
    CHECK("epoch 0 → empty",        fmt(0, test_date) == "");

    // Same day → Today
    CHECK("same day → Today",       fmt(test_date, test_date) == "Today");

    // 1 day ahead → Tomorrow
    CHECK("1 day → Tomorrow",       fmt(make_date(2025, 6, 11), test_date) == "Tomorrow");

    // 2 days ahead → "2 days"
    CHECK("2 days → 2 days",        fmt(make_date(2025, 6, 12), test_date) == "2 days");

    // 3 days ahead → "3 days"
    CHECK("3 days → 3 days",        fmt(make_date(2025, 6, 13), test_date) == "3 days");

    // 7 days ahead → "7 days"
    CHECK("7 days → 7 days",        fmt(make_date(2025, 6, 17), test_date) == "7 days");

    // 14 days ahead → "14 days"
    CHECK("14 days → 14 days",      fmt(make_date(2025, 6, 24), test_date) == "14 days");

    // Past date → empty string
    CHECK("yesterday → empty",      fmt(make_date(2025, 6,  9), test_date) == "");

    // Far past → empty string
    CHECK("far past → empty",       fmt(make_date(2025, 1,  1), test_date) == "");

    // Month boundary → "21 days"
    CHECK("month boundary → 21 days", fmt(make_date(2025, 7, 1), test_date) == "21 days");

    // Year boundary: Dec 31 reference, target is Jan 1 → Tomorrow
    CHECK("year boundary → Tomorrow", fmt(make_date(2026, 1, 1), make_date(2025, 12, 31)) == "Tomorrow");

    // Large future date → "365 days"
    CHECK("365 days → 365 days",    fmt(make_date(2026, 6, 10), test_date) == "365 days");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
