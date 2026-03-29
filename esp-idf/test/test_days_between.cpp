// Host-compilable unit tests for days_between().
// Build and run:  c++ -std=c++17 test_days_between.cpp -o test_days_between && ./test_days_between

#include <cassert>
#include <cstdio>
#include <cstring>

// Pull in the function under test
#include "../main/time_utils.h"

// Build a time_t for a given local date/time
static time_t make_time(int year, int month, int day, int hour = 0, int min = 0, int sec = 0) {
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = min;
    t.tm_sec  = sec;
    t.tm_isdst = -1;
    return mktime(&t);
}

static int passed = 0;
static int failed = 0;

#define CHECK(desc, expr) \
    do { \
        if (expr) { printf("  PASS  %s\n", desc); passed++; } \
        else      { printf("  FAIL  %s\n", desc); failed++; } \
    } while (0)

int main() {
    printf("days_between tests\n");

    // Same day, both at midnight
    CHECK("Same day, both at midnight",
        days_between(make_time(2025, 6, 10), make_time(2025, 6, 10)) == 0);

    // Same day, different times — should still be 0 because of midnight normalisation
    CHECK("same day, different times",
        days_between(make_time(2025, 6, 10, 23, 59, 59), make_time(2025, 6, 10, 0, 0, 0)) == 0);

    // One day ahead
    CHECK("One day ahead",
        days_between(make_time(2025, 6, 11), make_time(2025, 6, 10)) == 1);

    // One day behind (negative result)
    CHECK("One day behind",
        days_between(make_time(2025, 6, 10), make_time(2025, 6, 11)) == -1);

    // One week
    CHECK("Seven days ahead",
        days_between(make_time(2025, 6, 17), make_time(2025, 6, 10)) == 7);

    // Crossing a month boundary
    CHECK("Crossing a month boundary (Jan→Feb)",
        days_between(make_time(2025, 2, 1), make_time(2025, 1, 31)) == 1);

    // Crossing a year boundary
    CHECK("Crossing a year boundary (Dec→Jan)",
        days_between(make_time(2026, 1, 1), make_time(2025, 12, 31)) == 1);

    // Leap year: 2024 has Feb 29
    CHECK("Crossing a leap day",
        days_between(make_time(2024, 3, 1), make_time(2024, 2, 28)) == 2);

    // Non-leap year: 2025 Feb has 28 days
    CHECK("Non-leap year",
        days_between(make_time(2025, 3, 1), make_time(2025, 2, 28)) == 1);

    // Large gap
    CHECK("365 days apart",
        days_between(make_time(2026, 1, 1), make_time(2025, 1, 1)) == 365);

    CHECK("DST Boundary",
        days_between(make_time(2026, 3, 29, 9, 13, 59), make_time(2026, 3, 27, 17, 13, 0)) == 2);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
