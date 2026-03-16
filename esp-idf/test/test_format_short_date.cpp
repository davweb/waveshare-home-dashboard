// Host-compilable unit tests for format_short_date().
// Build and run:  c++ -std=c++17 test_format_short_date.cpp -o test_format_short_date && ./test_format_short_date

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
    format_short_date(buf, sizeof(buf), epoch, test_date);
    return buf;
}

int main() {
    setlocale(LC_TIME, "C"); // ensure English day/month names

    // Use 2025-06-10 (Tuesday) as the fixed reference date.
    time_t test_date = make_date(2025, 6, 10);

    printf("format_short_date tests\n");

    // epoch == 0 → empty string
    CHECK("epoch 0 → empty",          fmt(0, test_date) == "");

    // Same day → Today
    CHECK("same day → Today",         fmt(test_date, test_date) == "Today");

    // 1 day ahead → Tomorrow
    CHECK("1 day → Tomorrow",         fmt(make_date(2025, 6, 11), test_date) == "Tomorrow");

    // 2 days → day of week (Thursday)
    CHECK("2 days → Thursday",        fmt(make_date(2025, 6, 12), test_date) == "Thursday");

    // 5 days → day of week (Sunday)
    CHECK("5 days → Sunday",          fmt(make_date(2025, 6, 15), test_date) == "Sunday");

    // 6 days → day of week (Monday)
    CHECK("6 days → Monday",          fmt(make_date(2025, 6, 16), test_date) == "Monday");

    // 7 days → fallback "D Mon"
    CHECK("7 days → 17 Jun",          fmt(make_date(2025, 6, 17), test_date) == "17 Jun");

    // 14 days → fallback
    CHECK("14 days → 24 Jun",         fmt(make_date(2025, 6, 24), test_date) == "24 Jun");

    // Month boundary → fallback
    CHECK("next month → 1 Jul",       fmt(make_date(2025, 7,  1), test_date) == "1 Jul");

    // Past date → fallback "D Mon"
    CHECK("yesterday → 9 Jun",        fmt(make_date(2025, 6,  9), test_date) == "9 Jun");

    // Far past → fallback
    CHECK("far past → 1 Jan",         fmt(make_date(2025, 1,  1), test_date) == "1 Jan");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
