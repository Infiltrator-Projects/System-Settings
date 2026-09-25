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

/**
 * Format a supplied instant under a supplied temporal policy.
 *
 * desktop_use_24h is consulted only for the legacy "standard" clock mode.
 * Strings are owned by @out and released with
 * ss_home_temporal_presentation_clear().
 */
bool ss_home_temporal_presentation_format(
    const InfiltratrTemporalPolicyV3 *policy,
    GDateTime *now,
    bool desktop_use_24h,
    SsHomeTemporalPresentation *out);

/**
 * Read the system-wide policy and format the current local instant.
 *
 * Failure leaves @out cleared.
 */
bool ss_home_temporal_presentation_now(
    SsHomeTemporalPresentation *out);

/** Release all strings held by one presentation value. */
void ss_home_temporal_presentation_clear(
    SsHomeTemporalPresentation *presentation);

#endif
