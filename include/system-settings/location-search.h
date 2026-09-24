// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file location-search.h
 * @brief Asynchronous locality lookup contract for the Linux Date & Time UI.
 */
#ifndef SYSTEM_SETTINGS_LOCATION_SEARCH_H
#define SYSTEM_SETTINGS_LOCATION_SEARCH_H

#include <gio/gio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SS_LOCATION_NAME_CAPACITY 256U
#define SS_LOCATION_COUNTRY_CAPACITY 8U

typedef struct SsLocationSearchResult {
    char display_name[SS_LOCATION_NAME_CAPACITY];
    char country_code[SS_LOCATION_COUNTRY_CAPACITY];
    double latitude;
    double longitude;
} SsLocationSearchResult;

/**
 * Search completion.
 *
 * results is always a caller-owned GPtrArray of SsLocationSearchResult entries,
 * including on error/cancellation; unref it after use. error_message is
 * borrowed for the callback duration. user_data remains caller-owned.
 */
typedef void (*SsLocationSearchCompletion)(
    GPtrArray *results,
    const char *error_message,
    gpointer user_data);

/**
 * Resolve a human place query such as "Mooroopna" asynchronously.
 *
 * At most eight results are requested. The cancellable is borrowed for the
 * asynchronous operation and may be NULL. Completion still follows the normal
 * callback path when the backend reports cancellation.
 */
void ss_location_search_async(
    const char *query,
    GCancellable *cancellable,
    SsLocationSearchCompletion callback,
    gpointer user_data);

#ifdef __cplusplus
}
#endif
#endif
