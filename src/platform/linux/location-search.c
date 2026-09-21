// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/location-search.h"

#include <geocode-glib/geocode-glib.h>

#include <string.h>

typedef struct SsLocationSearchRequest {
    SsLocationSearchCompletion callback;
    gpointer user_data;
} SsLocationSearchRequest;

static void result_free(gpointer data)
{
    g_free(data);
}

static char *best_description(GeocodePlace *place,
                              GeocodeLocation *location)
{
    const char *description;
    const char *name;
    const char *state;
    const char *country;

    if (location != NULL) {
        description = geocode_location_get_description(location);
        if (description != NULL && description[0] != '\0') {
            return g_strdup(description);
        }
    }

    name = place != NULL ? geocode_place_get_name(place) : NULL;
    state = place != NULL ? geocode_place_get_state(place) : NULL;
    country = place != NULL ? geocode_place_get_country(place) : NULL;

    if (name != NULL && state != NULL && country != NULL) {
        return g_strdup_printf("%s, %s, %s", name, state, country);
    }
    if (name != NULL && country != NULL) {
        return g_strdup_printf("%s, %s", name, country);
    }
    return g_strdup(name != NULL ? name : "Unnamed location");
}

static void search_finished(GObject *source,
                            GAsyncResult *async_result,
                            gpointer user_data)
{
    SsLocationSearchRequest *request = user_data;
    g_autoptr(GError) error = NULL;
    GList *places;
    GList *cursor;
    GPtrArray *results;

    if (request == NULL) {
        return;
    }

    places = geocode_forward_search_finish(
        GEOCODE_FORWARD(source), async_result, &error);
    results = g_ptr_array_new_with_free_func(result_free);

    if (places != NULL) {
        for (cursor = places;
             cursor != NULL && results->len < 8U;
             cursor = cursor->next) {
            GeocodePlace *place = GEOCODE_PLACE(cursor->data);
            GeocodeLocation *location;
            SsLocationSearchResult *item;
            g_autofree char *description = NULL;
            const char *country_code;

            if (place == NULL) {
                continue;
            }
            location = geocode_place_get_location(place);
            if (location == NULL) {
                continue;
            }

            item = g_new0(SsLocationSearchResult, 1);
            description = best_description(place, location);
            if (description == NULL ||
                g_strlcpy(item->display_name,
                          description,
                          sizeof(item->display_name)) >=
                    sizeof(item->display_name)) {
                g_free(item);
                continue;
            }

            country_code = geocode_place_get_country_code(place);
            if (country_code != NULL) {
                (void)g_strlcpy(item->country_code,
                                country_code,
                                sizeof(item->country_code));
            }

            item->latitude = geocode_location_get_latitude(location);
            item->longitude = geocode_location_get_longitude(location);
            g_ptr_array_add(results, item);
        }
        g_list_free_full(places, g_object_unref);
    }

    if (request->callback != NULL) {
        request->callback(
            results,
            error != NULL ? error->message : NULL,
            request->user_data);
    } else {
        g_ptr_array_unref(results);
    }

    g_free(request);
    g_object_unref(source);
}

void ss_location_search_async(
    const char *query,
    GCancellable *cancellable,
    SsLocationSearchCompletion callback,
    gpointer user_data)
{
    SsLocationSearchRequest *request;
    GeocodeForward *forward;

    if (query == NULL || query[0] == '\0') {
        if (callback != NULL) {
            GPtrArray *empty =
                g_ptr_array_new_with_free_func(result_free);
            callback(empty, "Enter a locality or place name.", user_data);
        }
        return;
    }

    forward = geocode_forward_new_for_string(query);
    if (forward == NULL) {
        if (callback != NULL) {
            GPtrArray *empty =
                g_ptr_array_new_with_free_func(result_free);
            callback(empty, "Unable to initialise location search.", user_data);
        }
        return;
    }

    geocode_forward_set_answer_count(forward, 8U);

    request = g_new0(SsLocationSearchRequest, 1);
    request->callback = callback;
    request->user_data = user_data;

    /*
     * Keep one explicit reference until the completion callback. This avoids
     * depending on backend-specific lifetime details while the request is in
     * flight.
     */
    g_object_ref(forward);
    geocode_forward_search_async(
        forward, cancellable, search_finished, request);
    g_object_unref(forward);
}
