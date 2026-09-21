// SPDX-License-Identifier: GPL-3.0-or-later
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

bool ss_location_metadata_load(SsLocationMetadata *metadata);
bool ss_location_metadata_save(const SsLocationMetadata *metadata);

#ifdef __cplusplus
}
#endif
#endif
