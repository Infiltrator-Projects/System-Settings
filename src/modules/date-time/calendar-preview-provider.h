// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file calendar-preview-provider.h
 * @brief Optional runtime bridge to Calendar-owned presentation algorithms.
 */
#ifndef SYSTEM_SETTINGS_CALENDAR_PREVIEW_PROVIDER_H
#define SYSTEM_SETTINGS_CALENDAR_PREVIEW_PROVIDER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct SsCalendarPreviewProvider SsCalendarPreviewProvider;

/**
 * Calendar owns specialised clock/calendar algorithms. System Settings owns
 * the user's policy and uses Calendar's stable runtime C ABI only to render a
 * faithful preview. The dependency is discovered dynamically so System
 * Settings remains usable when Calendar is not installed.
 *
 * Clock and calendar capabilities are independent: a runtime exposing only one
 * family can still serve that family. Discovery failures are therefore not
 * fatal to System Settings itself.
 */
/** Create a lazy provider; no library search is performed by the constructor. */
SsCalendarPreviewProvider *ss_calendar_preview_provider_new(void);
/**
 * Open one explicit library immediately. Returns NULL unless at least one
 * supported preview capability can be bound.
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

/** Report whether a clock or calendar capability is already bound. */
bool ss_calendar_preview_provider_available(
    const SsCalendarPreviewProvider *provider);

/**
 * Format one specialised clock preview.
 * Returns newly allocated UTF-8 owned by the caller, or NULL when unavailable.
 */
char *ss_calendar_preview_provider_format_clock(
    SsCalendarPreviewProvider *provider,
    const char *clock_mode,
    int64_t unix_microseconds,
    int utc_offset_seconds,
    bool show_seconds,
    double latitude,
    double longitude);

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
