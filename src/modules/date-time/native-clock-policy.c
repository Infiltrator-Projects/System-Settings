// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/native-clock-policy.h"

#include <string.h>

const char *ss_native_clock_mode_id(bool use_24h)
{
    return use_24h ? "standard-24" : "standard-12";
}

bool ss_native_clock_mode_profile(const char *clock_mode,
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
    return false;
}

bool ss_native_clock_mode_is_legacy_default(const char *clock_mode)
{
    return clock_mode != NULL &&
           strcmp(clock_mode, "standard") == 0;
}

bool ss_native_clock_mode_is_conventional(const char *clock_mode)
{
    if (clock_mode == NULL) {
        return false;
    }

    return strcmp(clock_mode, "standard-12") == 0 ||
           strcmp(clock_mode, "standard-24") == 0;
}

bool ss_native_clock_mode_tracks_desktop(const char *clock_mode)
{
    return ss_native_clock_mode_is_legacy_default(clock_mode) ||
           ss_native_clock_mode_is_conventional(clock_mode);
}

unsigned int ss_clock_presentation_refresh_interval_ms(
    const char *clock_mode,
    bool show_seconds)
{
    if (!show_seconds || ss_native_clock_mode_tracks_desktop(clock_mode)) {
        return 1000U;
    }
    return 500U;
}
