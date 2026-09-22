// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_CALENDAR_PREVIEW_PROVIDER_H
#define SYSTEM_SETTINGS_CALENDAR_PREVIEW_PROVIDER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct SsCalendarPreviewProvider SsCalendarPreviewProvider;

/*
 * Calendar owns specialised clock/calendar algorithms. System Settings owns
 * the user's policy and uses Calendar's stable runtime C ABI only to render a
 * faithful preview. The dependency is discovered dynamically so System
 * Settings remains usable when Calendar is not installed.
 */
SsCalendarPreviewProvider *ss_calendar_preview_provider_new(void);
SsCalendarPreviewProvider *ss_calendar_preview_provider_new_from(
    const char *library_name);
SsCalendarPreviewProvider *ss_calendar_preview_provider_new_with_root_for_test(
    const char *root);
void ss_calendar_preview_provider_force_retry_for_test(
    SsCalendarPreviewProvider *provider);
void ss_calendar_preview_provider_free(
    SsCalendarPreviewProvider *provider);

bool ss_calendar_preview_provider_available(
    const SsCalendarPreviewProvider *provider);

char *ss_calendar_preview_provider_format_clock(
    SsCalendarPreviewProvider *provider,
    const char *clock_mode,
    int64_t unix_microseconds,
    int utc_offset_seconds,
    bool show_seconds,
    double latitude,
    double longitude);

char *ss_calendar_preview_provider_format_date(
    SsCalendarPreviewProvider *provider,
    const char *calendar_id,
    int gregorian_year,
    int gregorian_month,
    int gregorian_day);

#endif
