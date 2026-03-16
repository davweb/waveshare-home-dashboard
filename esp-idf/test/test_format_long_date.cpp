// Host-compilable unit tests for format_long_date().
// Build and run:  c++ -std=c++17 test_format_long_date.cpp -o test_format_long_date && ./test_format_long_date

#include <cassert>
#include <cstdio>
#include <cstring>
#include <clocale>
#include <string>

#include "../main/time_utils.h"

static int passed = 0;
static int failed = 0;

#define CHECK(desc, expr) \
    do { \
        if (expr) { printf("  PASS  %s\n", desc); passed++; } \
        else      { printf("  FAIL  %s\n", desc); failed++; } \
    } while (0)

static struct tm make_tm(int year, int month, int day) {
    struct tm t = {};
    t.tm_year  = year - 1900;
    t.tm_mon   = month - 1;
    t.tm_mday  = day;
    t.tm_isdst = -1;
    mktime(&t); // normalise — fills in tm_wday etc.
    return t;
}

static std::string format(int year, int month, int day) {
    struct tm t = make_tm(year, month, day);
    char buf[64];
    format_long_date(buf, sizeof(buf), &t);
    return buf;
}

int main() {
    setlocale(LC_TIME, "C"); // ensure English day/month names

    printf("format_long_date tests\n");

    CHECK("Single-digit day has no leading zero or space",
        format(2025, 3,  3) == "Monday 3 March");

    CHECK("Double-digit day",
        format(2025, 3, 10) == "Monday 10 March");

    CHECK("End of month",
        format(2025, 1, 31) == "Friday 31 January");

    CHECK("Start of month",
        format(2025, 2,  1) == "Saturday 1 February");

    CHECK("Leap day",
        format(2024, 2, 29) == "Thursday 29 February");

    CHECK("Year boundary - Dec 31",
        format(2025, 12, 31) == "Wednesday 31 December");

    CHECK("Year boundary - Jan 1",
        format(2026, 1, 1) == "Thursday 1 January");

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
