// SPDX-License-Identifier: GPL-3.0-or-later
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

typedef void (*SsLocationSearchCompletion)(
    GPtrArray *results,
    const char *error_message,
    gpointer user_data);

/**
 * Resolve a human place query such as "Mooroopna" asynchronously.
 *
 * Results are returned as SsLocationSearchResult objects in a GPtrArray owned
 * by the callback. The callback must unref the array when finished.
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
