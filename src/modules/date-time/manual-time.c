// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/manual-time.h"

#include <infiltratr/temporal.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define US_PER_SECOND INT64_C(1000000)
#define US_PER_DAY (INT64_C(86400) * US_PER_SECOND)

static bool ascii_equal_ci(const char *left, const char *right)
{
    size_t index;

    if (left == NULL || right == NULL) {
        return false;
    }
    if (strlen(left) != strlen(right)) {
        return false;
    }
    for (index = 0U; left[index] != '\0'; ++index) {
        const unsigned char l = (unsigned char)left[index];
        const unsigned char r = (unsigned char)right[index];
        if (tolower(l) != tolower(r)) {
            return false;
        }
    }
    return true;
}

static bool profile_for_mode(const char *clock_mode,
                             bool desktop_use_24h,
                             InfiltratrClockProfile *profile)
{
    if (clock_mode == NULL || profile == NULL) {
        return false;
    }

    if (strcmp(clock_mode, "standard") == 0) {
        *profile = desktop_use_24h
            ? INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24
            : INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12;
        return true;
    }
    if (strcmp(clock_mode, "standard-24") == 0) {
        *profile = INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24;
        return true;
    }
    if (strcmp(clock_mode, "standard-12") == 0) {
        *profile = INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12;
        return true;
    }
    if (strcmp(clock_mode, "decimal") == 0) {
        *profile = INFILTRATR_CLOCK_PROFILE_DECIMAL_10;
        return true;
    }
    return false;
}

bool ss_manual_time_mode_supported(const char *clock_mode)
{
    InfiltratrClockProfile profile;
    return profile_for_mode(clock_mode, true, &profile);
}

bool ss_manual_time_format(const char *clock_mode,
                           bool desktop_use_24h,
                           int64_t unix_microseconds,
                           int32_t utc_offset_seconds,
                           bool show_seconds,
                           char *buffer,
                           size_t capacity)
{
    InfiltratrClockProfile profile;

    if (!profile_for_mode(clock_mode, desktop_use_24h, &profile)) {
        return false;
    }
    return infiltratr_temporal_format_clock(
        profile,
        unix_microseconds,
        utc_offset_seconds,
        show_seconds,
        buffer,
        capacity,
        NULL);
}

static bool parse_24_hour(const char *text, int64_t *microseconds_of_day)
{
    int hour = 0;
    int minute = 0;
    int second = 0;
    char trailing = '\0';

    if (sscanf(text, "%d:%d:%d%c",
               &hour, &minute, &second, &trailing) != 3) {
        second = 0;
        if (sscanf(text, "%d:%d%c",
                   &hour, &minute, &trailing) != 2) {
            return false;
        }
    }

    if (hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 ||
        second < 0 || second > 59) {
        return false;
    }

    *microseconds_of_day =
        ((int64_t)hour * INT64_C(3600) +
         (int64_t)minute * INT64_C(60) +
         (int64_t)second) * US_PER_SECOND;
    return true;
}

static bool parse_12_hour(const char *text, int64_t *microseconds_of_day)
{
    int hour = 0;
    int minute = 0;
    int second = 0;
    char suffix[3] = {0};
    char trailing = '\0';
    int fields;

    fields = sscanf(text, "%d:%d:%d %2s%c",
                    &hour, &minute, &second, suffix, &trailing);
    if (fields != 4) {
        second = 0;
        memset(suffix, 0, sizeof(suffix));
        fields = sscanf(text, "%d:%d %2s%c",
                        &hour, &minute, suffix, &trailing);
        if (fields != 3) {
            return false;
        }
    }

    if (hour < 1 || hour > 12 ||
        minute < 0 || minute > 59 ||
        second < 0 || second > 59 ||
        (!ascii_equal_ci(suffix, "AM") &&
         !ascii_equal_ci(suffix, "PM"))) {
        return false;
    }

    if (hour == 12) {
        hour = 0;
    }
    if (ascii_equal_ci(suffix, "PM")) {
        hour += 12;
    }

    *microseconds_of_day =
        ((int64_t)hour * INT64_C(3600) +
         (int64_t)minute * INT64_C(60) +
         (int64_t)second) * US_PER_SECOND;
    return true;
}

static bool parse_decimal(const char *text, int64_t *microseconds_of_day)
{
    int hour = 0;
    int minute = 0;
    int second = 0;
    char trailing = '\0';
    int64_t decimal_seconds;

    if (sscanf(text, "%d:%d:%d%c",
               &hour, &minute, &second, &trailing) != 3) {
        second = 0;
        if (sscanf(text, "%d:%d%c",
                   &hour, &minute, &trailing) != 2) {
            return false;
        }
    }

    if (hour < 0 || hour > 9 ||
        minute < 0 || minute > 99 ||
        second < 0 || second > 99) {
        return false;
    }

    decimal_seconds =
        (int64_t)hour * INT64_C(10000) +
        (int64_t)minute * INT64_C(100) +
        (int64_t)second;

    /*
     * One decimal second is exactly 0.864 SI seconds, so integer decimal
     * seconds map exactly to microseconds with no floating-point rounding.
     */
    *microseconds_of_day = decimal_seconds * INT64_C(864000);
    return *microseconds_of_day >= 0 &&
           *microseconds_of_day < US_PER_DAY;
}

bool ss_manual_time_parse(const char *clock_mode,
                          bool desktop_use_24h,
                          const char *text,
                          int64_t *microseconds_of_day)
{
    InfiltratrClockProfile profile;
    int64_t parsed = 0;

    if (text == NULL || microseconds_of_day == NULL ||
        !profile_for_mode(clock_mode, desktop_use_24h, &profile)) {
        return false;
    }

    switch (profile) {
    case INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12:
        if (!parse_12_hour(text, &parsed)) {
            return false;
        }
        break;
    case INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24:
        if (!parse_24_hour(text, &parsed)) {
            return false;
        }
        break;
    case INFILTRATR_CLOCK_PROFILE_DECIMAL_10:
        if (!parse_decimal(text, &parsed)) {
            return false;
        }
        break;
    default:
        return false;
    }

    *microseconds_of_day = parsed;
    return true;
}
