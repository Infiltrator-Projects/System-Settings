// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/manual-time.h"

#include <infiltratr/core.h>
#include <infiltratr/temporal.h>
#include <string.h>

#define US_PER_SECOND INT64_C(1000000)
#define US_PER_DAY (INT64_C(86400) * US_PER_SECOND)

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

/* Each field has at most two ASCII digits: reject signs, whitespace,
 * overflow-length numbers and suffix garbage before any arithmetic. */
static bool parse_field(const char **cursor, int *value)
{
    const char *p = *cursor;
    int number = 0;
    unsigned int digits = 0U;
    while (*p >= '0' && *p <= '9' && digits < 2U) {
        number = number * 10 + (*p++ - '0');
        ++digits;
    }
    if (digits == 0U || (*p >= '0' && *p <= '9')) {
        return false;
    }
    *cursor = p;
    *value = number;
    return true;
}

static bool parse_fields(const char **cursor, int *hour, int *minute, int *second)
{
    if (!parse_field(cursor, hour) || **cursor != ':') {
        return false;
    }
    ++*cursor;
    if (!parse_field(cursor, minute)) {
        return false;
    }
    *second = 0;
    if (**cursor == ':') {
        ++*cursor;
        return parse_field(cursor, second);
    }
    return true;
}

static bool parse_24_hour(const char *text, int64_t *microseconds_of_day)
{
    int hour, minute, second;
    if (!parse_fields(&text, &hour, &minute, &second) || *text != '\0' ||
        hour > 23 || minute > 59 || second > 59) {
        return false;
    }
    *microseconds_of_day = ((int64_t)hour * 3600 + minute * 60 + second) * US_PER_SECOND;
    return true;
}

static bool parse_12_hour(const char *text, int64_t *microseconds_of_day)
{
    int hour, minute, second;
    if (!parse_fields(&text, &hour, &minute, &second) ||
        hour < 1 || hour > 12 || minute > 59 || second > 59) {
        return false;
    }
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    if (!infiltratr_ascii_equal_ci(text, "AM") &&
        !infiltratr_ascii_equal_ci(text, "PM")) {
        return false;
    }
    hour %= 12;
    if (infiltratr_ascii_equal_ci(text, "PM")) {
        hour += 12;
    }
    *microseconds_of_day = ((int64_t)hour * 3600 + minute * 60 + second) * US_PER_SECOND;
    return true;
}

static bool parse_decimal(const char *text, int64_t *microseconds_of_day)
{
    int hour, minute, second;
    if (!parse_fields(&text, &hour, &minute, &second) || *text != '\0' || hour > 9) {
        return false;
    }
    /* One decimal second is exactly 864000 microseconds; no rounding needed. */
    *microseconds_of_day = ((int64_t)hour * 10000 + minute * 100 + second) * INT64_C(864000);
    return *microseconds_of_day < US_PER_DAY;
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
