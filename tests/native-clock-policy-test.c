// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/native-clock-policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "Native-clock-policy test failed: %s (%s:%d)\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

int main(void)
{
    CHECK(strcmp(ss_native_clock_mode_id(false), "standard-12") == 0);
    CHECK(strcmp(ss_native_clock_mode_id(true), "standard-24") == 0);

    CHECK(ss_native_clock_mode_is_legacy_default("standard"));
    CHECK(!ss_native_clock_mode_is_legacy_default("standard-12"));

    CHECK(ss_native_clock_mode_is_conventional("standard-12"));
    CHECK(ss_native_clock_mode_is_conventional("standard-24"));
    CHECK(!ss_native_clock_mode_is_conventional("decimal"));

    CHECK(ss_native_clock_mode_tracks_desktop("standard"));
    CHECK(ss_native_clock_mode_tracks_desktop("standard-12"));
    CHECK(ss_native_clock_mode_tracks_desktop("standard-24"));
    CHECK(!ss_native_clock_mode_tracks_desktop("decimal"));
    CHECK(!ss_native_clock_mode_tracks_desktop("sidereal"));
    return 0;
}
