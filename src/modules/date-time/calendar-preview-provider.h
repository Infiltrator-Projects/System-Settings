// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file calendar-preview-provider.h
 * @brief Optional runtime bridge to Calendar-owned date algorithms.
 */
#ifndef SYSTEM_SETTINGS_CALENDAR_PREVIEW_PROVIDER_H
#define SYSTEM_SETTINGS_CALENDAR_PREVIEW_PROVIDER_H

#include <stdbool.h>

typedef struct SsCalendarPreviewProvider SsCalendarPreviewProvider;

/**
 * Common owns every system clock formatter. Calendar owns specialised calendar
 * date algorithms that are not duplicated in System Settings. This bridge
 * discovers Calendar's stable date ABI dynamically so System Settings remains
 * usable when Calendar is not installed.
 */
/** Create a lazy provider; no library search is performed by the constructor. */
SsCalendarPreviewProvider *ss_calendar_preview_provider_new(void);
/**
 * Open one explicit library immediately. Returns NULL unless the calendar-date
 * preview capability can be bound completely.
 */
SsCalendarPreviewProvider *ss_calendar_preview_provider_new_from(
    const char *library_name);
/** Private deterministic discovery-root override used only by regression tests. */
SsCalendarPreviewProvider *ss_calendar_preview_provider_new_with_root_for_test(
    const char *root);
/** Clear retry throttling for deterministic missing-then-appearing tests. */
void ss_calendar_preview_provider_force_retry_for_test(
    SsCalendarPreviewProvider *provider);
void ss_calendar_preview_provider_free(
    SsCalendarPreviewProvider *provider);

/** Report whether Calendar's date-preview capability is already bound. */
bool ss_calendar_preview_provider_available(
    const SsCalendarPreviewProvider *provider);

/**
 * Format one calendar date preview.
 * Returns newly allocated UTF-8 owned by the caller, or NULL when unavailable.
 */
char *ss_calendar_preview_provider_format_date(
    SsCalendarPreviewProvider *provider,
    const char *calendar_id,
    int gregorian_year,
    int gregorian_month,
    int gregorian_day);

#endif
