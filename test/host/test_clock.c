/* test_clock.c — the shell's time to calendar fields (design.md §13.2, §15).
 *
 * clock_civil against an independent model that walks the calendar a day
 * at a time from 1900 to 2200, then a table of instants: offsets either
 * side of UTC and across midnight, month ends, leap years including 2000
 * and 2100, before 1970, and the fallback. Then the FAT time stamp, each
 * field read back out of the packed word, and both ends of FAT's range.
 * From picocalc-quest (D8).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <string.h>

#include "clock.h"

static int failures;

static void expect(const char *what, const clock_tm_t *got, int year, int mon, int mday,
                   int hour, int min, int sec, int wday) {
    if (got->tm_year + 1900 == year && got->tm_mon + 1 == mon && got->tm_mday == mday &&
        got->tm_hour == hour && got->tm_min == min && got->tm_sec == sec &&
        got->tm_wday == wday) {
        return;
    }
    failures++;
    fprintf(stderr, "FAIL %s: got %04d-%02d-%02d %02d:%02d:%02d wday %d, "
                    "want %04d-%02d-%02d %02d:%02d:%02d wday %d\n",
            what, got->tm_year + 1900, got->tm_mon + 1, got->tm_mday, got->tm_hour,
            got->tm_min, got->tm_sec, got->tm_wday, year, mon, mday, hour, min, sec, wday);
}

static int is_leap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int month_days(int y, int m) {
    static const int d[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return m == 2 && is_leap(y) ? 29 : d[m - 1];
}

/* Every day from 1900-01-01 (a Monday, day -25567) to 2199-12-31, at a
 * time of day that moves with the day so every field is exercised. */
static void sweep(void) {
    int y = 1900, m = 1, d = 1, wday = 1;
    for (long long day = -25567; y < 2200; day++) {
        long long tod = (day * 7919) % 86400;
        if (tod < 0) tod += 86400;
        clock_tm_t t;
        clock_civil(day * 86400 + tod, &t);
        char what[32];
        snprintf(what, sizeof what, "day %lld", day);
        expect(what, &t, y, m, d, (int)(tod / 3600), (int)(tod / 60 % 60), (int)(tod % 60), wday);
        if (failures > 10) return;
        wday = (wday + 1) % 7;
        if (++d > month_days(y, m)) {
            d = 1;
            if (++m > 12) {
                m = 1;
                y++;
            }
        }
    }
}

/* A FAT time stamp, unpacked by hand. */
static void expect_fat(const char *what, uint32_t got, int year, int mon, int mday, int hour,
                       int min, int sec) {
    int gy = (int)(got >> 25) + 1980, gm = (int)(got >> 21 & 15u), gd = (int)(got >> 16 & 31u);
    int gh = (int)(got >> 11 & 31u), gmin = (int)(got >> 5 & 63u), gs = (int)(got & 31u) * 2;
    if (gy == year && gm == mon && gd == mday && gh == hour && gmin == min && gs == sec) return;
    failures++;
    fprintf(stderr, "FAIL %s: got %04d-%02d-%02d %02d:%02d:%02d, "
                    "want %04d-%02d-%02d %02d:%02d:%02d\n",
            what, gy, gm, gd, gh, gmin, gs, year, mon, mday, hour, min, sec);
}

static void fat(void) {
    clock_tm_t t;
    clock_local(1793284215000LL, -240, &t);
    expect_fat("FAT, 2026-10-29 10:30:15", clock_fattime(&t), 2026, 10, 29, 10, 30, 14);
    clock_civil(4102444799LL, &t);
    expect_fat("FAT, the end of 2099", clock_fattime(&t), 2099, 12, 31, 23, 59, 58);
    clock_civil(315532800, &t);
    expect_fat("FAT, its first second", clock_fattime(&t), 1980, 1, 1, 0, 0, 0);
    clock_civil(315532799, &t);
    expect_fat("FAT, before 1980", clock_fattime(&t), 1980, 1, 1, 0, 0, 0);
    clock_civil(0, &t);
    expect_fat("FAT, the epoch", clock_fattime(&t), 1980, 1, 1, 0, 0, 0);
    /* 2107-12-31 23:59:59 UTC is 4354819199 s. */
    clock_civil(4354819199LL, &t);
    expect_fat("FAT, its last second", clock_fattime(&t), 2107, 12, 31, 23, 59, 58);
    clock_civil(4354819200LL, &t);
    expect_fat("FAT, after 2107", clock_fattime(&t), 2107, 12, 31, 23, 59, 58);
}

int main(void) {
    sweep();

    clock_tm_t t;
    clock_civil(0, &t);
    expect("epoch", &t, 1970, 1, 1, 0, 0, 0, 4);
    clock_civil(-1, &t);
    expect("a second before the epoch", &t, 1969, 12, 31, 23, 59, 59, 3);

    /* 2026-10-29 14:30:15 UTC is 1793284215 s. */
    clock_local(1793284215000LL, 0, &t);
    expect("UTC", &t, 2026, 10, 29, 14, 30, 15, 4);
    clock_local(1793284215999LL, 0, &t);
    expect("milliseconds truncate", &t, 2026, 10, 29, 14, 30, 15, 4);
    clock_local(1793284215000LL, -240, &t);
    expect("UTC-4", &t, 2026, 10, 29, 10, 30, 15, 4);
    clock_local(1793284215000LL, 570, &t);
    expect("UTC+9:30 across midnight", &t, 2026, 10, 30, 0, 0, 15, 5);
    /* 2026-10-29 02:00:00 UTC. */
    clock_local(1793239200000LL, -300, &t);
    expect("UTC-5 back across midnight", &t, 2026, 10, 28, 21, 0, 0, 3);
    /* 2024-12-31 23:30:00 UTC. */
    clock_local(1735687800000LL, 60, &t);
    expect("UTC+1 into a new year", &t, 2025, 1, 1, 0, 30, 0, 3);
    clock_local(1735689600000LL, -60, &t);
    expect("UTC-1 back into the old year", &t, 2024, 12, 31, 23, 0, 0, 2);

    /* 2000-02-29 12:00:00 and 2100-02-28 23:59:59 UTC. */
    clock_civil(951825600, &t);
    expect("2000 is a leap year", &t, 2000, 2, 29, 12, 0, 0, 2);
    clock_civil(4107542399LL, &t);
    expect("2100-02-28", &t, 2100, 2, 28, 23, 59, 59, 0);
    clock_civil(4107542400LL, &t);
    expect("2100 is not a leap year", &t, 2100, 3, 1, 0, 0, 0, 1);
    /* 2024-02-29 and 2023-04-30 23:59:59 UTC. */
    clock_civil(1709164800, &t);
    expect("2024-02-29", &t, 2024, 2, 29, 0, 0, 0, 4);
    clock_civil(1682899199, &t);
    expect("2023-04-30", &t, 2023, 4, 30, 23, 59, 59, 0);

    clock_fallback(0, &t);
    expect("the fallback at boot", &t, 2025, 1, 1, 0, 0, 0, 3);
    clock_fallback(90061999u, &t);
    expect("the fallback a day on", &t, 2025, 1, 2, 1, 1, 1, 4);

    fat();

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_clock: ok\n");
    return 0;
}
