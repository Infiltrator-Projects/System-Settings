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
 * Honors XDG_CONFIG_HOME. The output stays cleared on failure; false covers
 * missing, unreadable or malformed data. Coordinates must be finite/in range
 * and all text must fit, be terminated and contain valid UTF-8.
 */
bool ss_location_metadata_load(SsLocationMetadata *metadata);

/**
 * Durably replace the per-user metadata file.
 * The parent directory is private (0700) and the file is written as 0600.
 */
bool ss_location_metadata_save(const SsLocationMetadata *metadata);

/**
 * Durably remove the persisted locality metadata.
 * Missing metadata is already a successful cleared state.
 */
bool ss_location_metadata_clear(void);

/** Return the XDG metadata path; caller releases with g_free(). */
char *ss_location_metadata_path_alloc(void);

/**
 * Serialize the complete locality transaction across System Settings processes.
 *
 * Callers that perform the staged metadata -> temporal-policy -> metadata
 * publication sequence must hold this lock for the complete sequence, including
 * any rollback. This prevents one process from overwriting another process's
 * journal or rolling a newer locality back to stale coordinates.
 */
bool ss_location_metadata_transaction_begin(void);
void ss_location_metadata_transaction_end(void);

/**
 * Stage locality metadata durably before the paired temporal-policy write.
 * finish_staged() publishes the staged record to location.ini and removes the
 * journal. discard_staged() removes an abandoned journal.
 */
bool ss_location_metadata_stage(const SsLocationMetadata *metadata);
bool ss_location_metadata_finish_staged(void);
void ss_location_metadata_discard_staged(void);

/**
 * Recover an interrupted two-file locality transaction. If the staged
 * coordinates match the authoritative temporal policy, finalize metadata;
 * otherwise discard the abandoned stage. No staged file is also success.
 */
bool ss_location_metadata_recover(
    bool location_configured,
    double latitude,
    double longitude);

#ifdef __cplusplus
}
#endif
#endif
