// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_REGIONAL_CONTEXT_H
#define SYSTEM_SETTINGS_REGIONAL_CONTEXT_H

#include <glib.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SS_TIMEZONE_ID_CAPACITY 128U
#define SS_TIMEZONE_CITY_CAPACITY 96U

typedef struct SsRegionalContext {
    char timezone_id[SS_TIMEZONE_ID_CAPACITY];
    char timezone_city[SS_TIMEZONE_CITY_CAPACITY];
    double reference_latitude;
    double reference_longitude;
    bool has_reference_coordinates;
} SsRegionalContext;

/**
 * Read the current operating-system time-zone identity and, where tzdata
 * provides one, its representative reference coordinate.
 *
 * The reference coordinate belongs to the time-zone database entry. It is a
 * regional hint only and must never be represented as the user's physical
 * location unless the user explicitly chooses to use it.
 */
bool ss_regional_context_detect(SsRegionalContext *context);

/**
 * Resolve one IANA time-zone identifier through a zone.tab/zone1970.tab file.
 * Exposed for deterministic fixture testing and alternate platform adapters.
 */
bool ss_regional_context_lookup_timezone_reference(
    const char *zone_tab_path,
    const char *timezone_id,
    double *latitude,
    double *longitude);

/** Produce a readable city/zone tail such as "Port of Spain". */
bool ss_regional_context_city_name(const char *timezone_id,
                                   char *buffer,
                                   size_t capacity);

/**
 * Return canonical IANA time-zone identifiers from installed tzdata.
 *
 * Returns: (transfer full) (element-type utf8): strings owned by the array.
 */
GPtrArray *ss_regional_context_list_timezones(
    const char *current_timezone_id);

/**
 * Find the nearest representative IANA zone within a country.
 *
 * This is intentionally a best-effort locality-to-zone hint based on tzdata
 * reference points, not a polygon boundary oracle. The resulting zone remains
 * user-visible and editable before/after it is applied to the system.
 */
bool ss_regional_context_nearest_timezone(
    const char *country_code,
    double latitude,
    double longitude,
    char *timezone_id,
    size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
