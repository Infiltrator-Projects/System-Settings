// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-temporal-presentation.h
 * @brief Date & Time module bridge for the shell's Home dashboard.
 *
 * The shell remains presentation-only. This bridge reads the authoritative
 * temporal policy and returns already formatted strings for Home consumers.
 */
#ifndef SYSTEM_SETTINGS_HOME_TEMPORAL_PRESENTATION_H
#define SYSTEM_SETTINGS_HOME_TEMPORAL_PRESENTATION_H

#include <glib.h>
#include <infiltratr/temporal.h>
#include <stdbool.h>

typedef struct SsHomeTemporalPresentation {
    gchar *clock_text;
    gchar *date_text;
    gchar *system_time_text;
} SsHomeTemporalPresentation;

typedef struct SsHomeTemporalPresenter SsHomeTemporalPresenter;

void ss_home_temporal_presentation_init(
    SsHomeTemporalPresentation *presentation);

/**
 * Format one supplied instant. @out is a pure output parameter: its previous
 * contents are never inspected or released. A caller reusing a populated
 * presentation must clear it first.
 */
bool ss_home_temporal_presentation_format(
    const InfiltratrTemporalPolicyV3 *policy,
    GDateTime *now,
    bool desktop_use_24h,
    SsHomeTemporalPresentation *out);

/** Same pure-output ownership contract as ss_home_temporal_presentation_format(). */
bool ss_home_temporal_presentation_now(
    SsHomeTemporalPresentation *out);

SsHomeTemporalPresenter *ss_home_temporal_presenter_new(void);
void ss_home_temporal_presenter_free(
    SsHomeTemporalPresenter *presenter);
/** Same pure-output ownership contract as ss_home_temporal_presentation_format(). */
bool ss_home_temporal_presenter_format_now(
    SsHomeTemporalPresenter *presenter,
    SsHomeTemporalPresentation *out);

void ss_home_temporal_presentation_clear(
    SsHomeTemporalPresentation *presentation);

#endif
