// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-temporal-presenter.h
 * @brief Narrow Date & Time presentation service consumed by the shell adapter.
 *
 * This interface intentionally exposes only already-formatted Home strings and
 * refresh cadence. It does not expose temporal policy/domain types to the Linux
 * shell composition boundary.
 */
#ifndef SYSTEM_SETTINGS_HOME_TEMPORAL_PRESENTER_H
#define SYSTEM_SETTINGS_HOME_TEMPORAL_PRESENTER_H

#include <glib.h>
#include <stdbool.h>

typedef struct SsHomeTemporalPresentation {
    gchar *clock_text;
    gchar *date_text;
    gchar *system_time_text;
} SsHomeTemporalPresentation;

typedef struct SsHomeTemporalPresenter SsHomeTemporalPresenter;

void ss_home_temporal_presentation_init(
    SsHomeTemporalPresentation *presentation);

SsHomeTemporalPresenter *ss_home_temporal_presenter_new(void);
void ss_home_temporal_presenter_free(
    SsHomeTemporalPresenter *presenter);

/**
 * Format the current instant using authoritative Date & Time state. @out is a
 * pure output parameter; callers must clear a populated value before reuse.
 */
bool ss_home_temporal_presenter_format_now(
    SsHomeTemporalPresenter *presenter,
    SsHomeTemporalPresentation *out);

/** Suggested visible refresh cadence for the presenter's current clock mode. */
guint ss_home_temporal_presenter_refresh_interval_ms(
    const SsHomeTemporalPresenter *presenter);

void ss_home_temporal_presentation_clear(
    SsHomeTemporalPresentation *presentation);

#endif
