// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-temporal-presentation.h
 * @brief Date & Time temporal formatting contract.
 *
 * Pure supplied-instant formatting remains a Date & Time domain operation.
 * Shell-facing current-state presentation lives in home-temporal-presenter.h
 * so the composition boundary does not need Common temporal policy types.
 */
#ifndef SYSTEM_SETTINGS_HOME_TEMPORAL_PRESENTATION_H
#define SYSTEM_SETTINGS_HOME_TEMPORAL_PRESENTATION_H

#include "home-temporal-presenter.h"

#include <infiltratr/temporal.h>

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

#endif
