/* clock.c — the date and time, from the clock the shell keeps (design.md
 * §13.2).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "clock.h"

#ifndef PICOCALC_DOOM_HOST
#include "pico/time.h"

#include "ff.h"
#include "picocalc_shell.h"
#endif

/* Floor division, so times before 1970 land on the right day. */
static int64_t floor_div(int64_t a, int64_t b) {
    int64_t q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

void clock_civil(int64_t s, clock_tm_t *t) {
    int64_t days = floor_div(s, 86400);
    int64_t rem = s - days * 86400;
    t->tm_hour = (int)(rem / 3600);
    t->tm_min = (int)(rem / 60 % 60);
    t->tm_sec = (int)(rem % 60);
    /* 1970-01-01 was a Thursday. */
    t->tm_wday = (int)(((days + 4) % 7 + 7) % 7);

    /* Days to the Gregorian date: Howard Hinnant's civil_from_days, with
     * the year counted from March so that the leap day falls last. */
    int64_t z = days + 719468;
    int64_t era = floor_div(z, 146097);
    int64_t doe = z - era * 146097;                                  /* [0, 146096] */
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;   /* [0, 399] */
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);           /* [0, 365] */
    int64_t mp = (5 * doy + 2) / 153;                                /* [0, 11], March first */
    int64_t mday = doy - (153 * mp + 2) / 5 + 1;
    int64_t mon = mp < 10 ? mp + 2 : mp - 10;                        /* [0, 11], January first */
    int64_t year = yoe + era * 400 + (mon <= 1);
    t->tm_mday = (int)mday;
    t->tm_mon = (int)mon;
    t->tm_year = (int)(year - 1900);
}

void clock_local(int64_t utc_ms, int offset_min, clock_tm_t *t) {
    clock_civil(floor_div(utc_ms, 1000) + (int64_t)offset_min * 60, t);
}

void clock_fallback(uint64_t uptime_ms, clock_tm_t *t) {
    clock_civil(CLOCK_FALLBACK_EPOCH + (int64_t)(uptime_ms / 1000u), t);
}

uint32_t clock_fattime(const clock_tm_t *t) {
    int year = t->tm_year + 1900;
    if (year < 1980) return (uint32_t)(1u << 21 | 1u << 16);
    if (year > 2107) return (uint32_t)(127u << 25 | 12u << 21 | 31u << 16 | 23u << 11 | 59u << 5 | 29u);
    return (uint32_t)(year - 1980) << 25 | (uint32_t)(t->tm_mon + 1) << 21 |
           (uint32_t)t->tm_mday << 16 | (uint32_t)t->tm_hour << 11 |
           (uint32_t)t->tm_min << 5 | (uint32_t)(t->tm_sec / 2);
}

#ifndef PICOCALC_DOOM_HOST
bool picocalc_clock_now(clock_tm_t *t) {
    int64_t utc_ms;
    int offset_min;
    if (picocalc_shell_time(&utc_ms, &offset_min)) {
        clock_local(utc_ms, offset_min, t);
        return true;
    }
    clock_fallback(to_ms_since_boot(get_absolute_time()), t);
    return false;
}

/* FatFs's clock, in place of the SD driver's, which gave 0, an invalid
 * date (SDCARD_EXTERNAL_FATTIME). With no clock set, FatFs's own fixed
 * date, as with FF_FS_NORTC. */
DWORD get_fattime(void) {
    clock_tm_t t;
    if (!picocalc_clock_now(&t)) {
        t = (clock_tm_t){ .tm_year = FF_NORTC_YEAR - 1900, .tm_mon = FF_NORTC_MON - 1,
                          .tm_mday = FF_NORTC_MDAY };
    }
    return (DWORD)clock_fattime(&t);
}
#endif
