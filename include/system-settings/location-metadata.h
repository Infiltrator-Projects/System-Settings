// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file location-metadata.h
 * @brief Per-user metadata for an explicitly selected geographic locality.
 *
 * This record is not the operating-system time-zone authority. It preserves
 * the user's chosen display name/country/coordinates so location-dependent
 * presentation can distinguish a precise locality from a tzdata reference.
 */
#ifndef SYSTEM_SETTINGS_LOCATION_METADATA_H
#define SYSTEM_SETTINGS_LOCATION_METADATA_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SsLocationMetadata {
    char display_name[256];
    char country_code[8];
    char timezone_id[128];
    double latitude;
    double longitude;
} SsLocationMetadata;

/**
 * Load ~/.config/infiltrator/system-settings/location.ini.
 * The output is cleared before parsing; false covers missing or malformed data.
 */
bool ss_location_metadata_load(SsLocationMetadata *metadata);

/**
 * Durably replace the per-user metadata file.
 * The parent directory is private (0700) and the file is written as 0600.
 */
bool ss_location_metadata_save(const SsLocationMetadata *metadata);

#ifdef __cplusplus
}
#endif
#endif
