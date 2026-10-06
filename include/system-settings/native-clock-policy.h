// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_NATIVE_CLOCK_POLICY_H
#define SYSTEM_SETTINGS_NATIVE_CLOCK_POLICY_H

#include <stdbool.h>
#include <infiltratr/temporal.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Return the explicit conventional clock ID matching the native desktop. */
const char *ss_native_clock_mode_id(bool use_24h);

/**
 * Resolve legacy/native conventional policy IDs to their canonical Common
 * clock profile. Extended clock systems deliberately return false here.
 */
bool ss_native_clock_mode_profile(const char *clock_mode,
                                  bool desktop_use_24h,
                                  InfiltratrClockProfile *profile);

/** Internal bootstrap/fallback mode inherited from pre-authority designs. */
bool ss_native_clock_mode_is_legacy_default(const char *clock_mode);

/** Whether this mode has an exact native Cinnamon/GNOME 12/24-hour mapping. */
bool ss_native_clock_mode_is_conventional(const char *clock_mode);

/**
 * Decide whether an external native 12/24-hour change should update the
 * System Settings clock policy. Extended clock systems deliberately return
 * false because their native 12/24-hour value is only a compatibility fallback.
 */
bool ss_native_clock_mode_tracks_desktop(const char *clock_mode);

/**
 * Return the canonical UI refresh cadence for a clock presentation.
 *
 * Conventional clocks and clocks without visible seconds need only a 1 Hz
 * refresh. Extended clocks with visible seconds use 500 ms: this is faster
 * than the shortest displayed second in the supported catalogue while
 * avoiding the old duplicated 250/500 ms policy split between UI surfaces.
 */
unsigned int ss_clock_presentation_refresh_interval_ms(
    const char *clock_mode,
    bool show_seconds);

#ifdef __cplusplus
}
#endif

#endif
