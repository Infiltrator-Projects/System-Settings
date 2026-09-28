// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-location.c
 * @brief Locality search, coordinate transactions and location presentation.
 *
 * The Date & Time controller delegates the complete locality workflow here:
 * asynchronous geocoding, durable metadata pairing, rollback, advisory
 * time-zone inference and debounced coordinate publication.
 */

#include "linux-date-time-panel-private.h"
#include "linux-ui-helpers.h"

#include "system-settings/location-search.h"
#include "system-settings/regional-context.h"

#include <string.h>

typedef struct LocationSearchUiRequest {
    GtkWindow *window;
    guint generation;
} LocationSearchUiRequest;

bool ss_linux_date_time_location_coordinate_close(
    double left,
    double right)
{
    double difference = left - right;

    if (difference < 0.0) {
        difference = -difference;
    }
    return difference < 0.000001;
}

bool ss_linux_date_time_location_metadata_matches_policy(
    const SsLinuxDateTimePanel *state,
    const InfiltratrTemporalPolicyV3 *policy)
{
    return state != NULL &&
           policy != NULL &&
           policy->location_configured &&
           state->location_metadata_present &&
           ss_linux_date_time_location_coordinate_close(
               policy->latitude,
               state->location_metadata.latitude) &&
           ss_linux_date_time_location_coordinate_close(
               policy->longitude,
               state->location_metadata.longitude);
}

static gchar *format_coordinate_pair(
    double latitude,
    double longitude)
{
    const double latitude_magnitude =
        latitude < 0.0 ? -latitude : latitude;
    const double longitude_magnitude =
        longitude < 0.0 ? -longitude : longitude;

    return g_strdup_printf(
        "%.4f° %c, %.4f° %c",
        latitude_magnitude, latitude < 0.0 ? 'S' : 'N',
        longitude_magnitude, longitude < 0.0 ? 'W' : 'E');
}

void ss_linux_date_time_location_update_summary(
    SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy;
    g_autofree gchar *coordinates = NULL;
    g_autofree gchar *summary = NULL;

    if (state == NULL || state->location_summary == NULL) {
        return;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL) {
        return;
    }

    if (policy->location_configured) {
        coordinates = format_coordinate_pair(
            policy->latitude, policy->longitude);
        if (ss_linux_date_time_location_metadata_matches_policy(
                state, policy) &&
            state->location_metadata.display_name[0] != '\0') {
            summary = g_strdup_printf(
                "%s • %s",
                state->location_metadata.display_name,
                coordinates);
        } else {
            summary = g_strdup_printf(
                "Selected geographic coordinates • %s",
                coordinates);
        }
    } else if (state->regional_context.has_reference_coordinates) {
        const char *city =
            state->regional_context.timezone_city[0] != '\0'
                ? state->regional_context.timezone_city
                : state->regional_context.timezone_id;
        coordinates = format_coordinate_pair(
            state->regional_context.reference_latitude,
            state->regional_context.reference_longitude);
        summary = g_strdup_printf(
            "%s is the current approximation from system time zone %s • %s. "
            "Search for your actual locality to refine it.",
            city,
            state->regional_context.timezone_id,
            coordinates);
    } else {
        summary = g_strdup(
            "Search for a locality to establish geographic coordinates.");
    }

    gtk_label_set_text(GTK_LABEL(state->location_summary), summary);
}

bool ss_linux_date_time_location_policy_matches_reference(
    const SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL ||
        !state->regional_context.has_reference_coordinates) {
        return false;
    }

    policy = ss_date_time_model_policy(&state->model);
    return policy != NULL &&
           policy->location_configured &&
           ss_linux_date_time_location_coordinate_close(
               policy->latitude,
               state->regional_context.reference_latitude) &&
           ss_linux_date_time_location_coordinate_close(
               policy->longitude,
               state->regional_context.reference_longitude);
}

void ss_linux_date_time_location_cancel_coordinate_commit(
    SsLinuxDateTimePanel *state)
{
    if (state != NULL && state->coordinate_commit_id != 0U) {
        g_source_remove(state->coordinate_commit_id);
        state->coordinate_commit_id = 0U;
    }
}

static void clear_location_results(SsLinuxDateTimePanel *state)
{
    GtkWidget *child;

    if (state == NULL || state->location_results == NULL) {
        return;
    }

    child = gtk_widget_get_first_child(
        GTK_WIDGET(state->location_results));
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(state->location_results, child);
        child = next;
    }
    gtk_widget_set_visible(
        GTK_WIDGET(state->location_results), FALSE);
}

void on_location_result_activated(
    GtkListBox *box G_GNUC_UNUSED,
    GtkListBoxRow *row,
    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    const SsLocationSearchResult *result;
    char timezone_id[SS_TIMEZONE_ID_CAPACITY] = {0};
    const InfiltratrTemporalPolicyV3 *policy;
    InfiltratrTemporalPolicyV3 previous_policy;
    SsLocationMetadata previous_metadata;
    bool previous_metadata_present;
    SsSystemTimeState current_system;

    if (state == NULL || row == NULL) {
        return;
    }

    result = g_object_get_data(
        G_OBJECT(row), "ss-location-result");
    if (result == NULL) {
        return;
    }

    if (!ss_location_metadata_transaction_begin()) {
        ss_linux_date_time_panel_set_status(
            state,
            "Could not acquire the locality transaction lock.",
            true);
        return;
    }
    if (!ss_date_time_model_reload(&state->model)) {
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not reload the authoritative temporal policy before saving the locality.",
            true);
        return;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL) {
        ss_location_metadata_transaction_end();
        return;
    }
    previous_policy = *policy;
    previous_metadata = state->location_metadata;
    previous_metadata_present = state->location_metadata_present;

    memset(
        &state->location_metadata,
        0,
        sizeof(state->location_metadata));
    (void)g_strlcpy(
        state->location_metadata.display_name,
        result->display_name,
        sizeof(state->location_metadata.display_name));
    (void)g_strlcpy(
        state->location_metadata.country_code,
        result->country_code,
        sizeof(state->location_metadata.country_code));
    state->location_metadata.latitude = result->latitude;
    state->location_metadata.longitude = result->longitude;

    if (state->system_time_service != NULL &&
        ss_system_time_service_read(
            state->system_time_service, &current_system) &&
        current_system.timezone[0] != '\0') {
        (void)g_strlcpy(
            state->location_metadata.timezone_id,
            current_system.timezone,
            sizeof(state->location_metadata.timezone_id));
    }

    if (result->country_code[0] != '\0') {
        (void)ss_regional_context_nearest_timezone(
            result->country_code,
            result->latitude,
            result->longitude,
            timezone_id,
            sizeof(timezone_id));
    }

    /*
     * Journal metadata before publishing coordinates. If the process dies
     * after the policy commit but before metadata publication, startup can
     * finish the exact staged record rather than exposing a mismatched pair.
     */
    if (!ss_location_metadata_stage(&state->location_metadata)) {
        state->location_metadata = previous_metadata;
        state->location_metadata_present = previous_metadata_present;
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not stage the selected locality metadata.",
            true);
        return;
    }

    if (!ss_date_time_model_set_location(
            &state->model,
            true,
            result->latitude,
            result->longitude)) {
        ss_location_metadata_discard_staged();
        state->location_metadata = previous_metadata;
        state->location_metadata_present = previous_metadata_present;
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not save the selected geographic location.",
            true);
        return;
    }

    /*
     * zone.tab coordinates are representative points, not zone polygons.
     * Persist only the authoritative timedated value above; this inference is
     * user guidance and never an implicit system-time mutation.
     */
    state->location_metadata_present =
        ss_location_metadata_finish_staged();
    if (!state->location_metadata_present) {
        const bool restored = ss_date_time_model_set_location(
            &state->model,
            previous_policy.location_configured,
            previous_policy.latitude,
            previous_policy.longitude);
        const bool reloaded = restored
            ? true
            : ss_date_time_model_reload(&state->model);

        if (restored) {
            ss_location_metadata_discard_staged();
        }
        state->location_metadata = previous_metadata;
        state->location_metadata_present = previous_metadata_present;
        if (!previous_metadata_present) {
            memset(
                &state->location_metadata,
                0,
                sizeof(state->location_metadata));
        }

        ss_linux_date_time_panel_set_status(
            state,
            restored
                ? "The selected location could not be saved; the previous location and metadata were restored."
                : (reloaded
                    ? "The selected location could not be saved and rollback persistence failed; authoritative policy was reloaded and previous metadata restored."
                    : "The selected location could not be saved, and neither rollback nor authoritative reload succeeded; previous metadata was restored in memory."),
            true);
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_sync_controls(state);
        return;
    }

    ss_location_metadata_transaction_end();
    state->location_follows_timezone_reference = false;
    state->updating_controls = true;
    gtk_editable_set_text(
        GTK_EDITABLE(state->location_search),
        result->display_name);
    gtk_spin_button_set_value(
        state->latitude, result->latitude);
    gtk_spin_button_set_value(
        state->longitude, result->longitude);
    state->updating_controls = false;
    clear_location_results(state);
    ss_linux_date_time_location_update_summary(state);
    ss_linux_date_time_panel_policy_saved(state);

    if (timezone_id[0] != '\0') {
        g_autofree gchar *message = g_strdup_printf(
            "Location saved. Suggested time zone: %s. Review and choose it explicitly from Time zone if appropriate; the operating-system time zone was not changed.",
            timezone_id);
        ss_linux_date_time_panel_set_status(
            state, message, false);
    }
}

static void location_search_request_free(
    LocationSearchUiRequest *request)
{
    if (request == NULL) {
        return;
    }
    g_clear_object(&request->window);
    g_free(request);
}

static void location_search_complete(
    GPtrArray *results,
    const char *error_message,
    gpointer user_data)
{
    LocationSearchUiRequest *request = user_data;
    SsLinuxDateTimePanel *state;
    size_t index;

    if (request == NULL || request->window == NULL) {
        if (results != NULL) {
            g_ptr_array_unref(results);
        }
        location_search_request_free(request);
        return;
    }

    state = g_object_get_data(
        G_OBJECT(request->window),
        "system-settings-date-time-panel");
    if (state == NULL ||
        request->generation != state->location_search_generation) {
        if (results != NULL) {
            g_ptr_array_unref(results);
        }
        location_search_request_free(request);
        return;
    }

    clear_location_results(state);
    if (results == NULL || results->len == 0U) {
        ss_linux_date_time_panel_set_status(
            state,
            error_message != NULL
                ? error_message
                : "No matching locations were found.",
            true);
        if (results != NULL) {
            g_ptr_array_unref(results);
        }
        location_search_request_free(request);
        return;
    }

    for (index = 0U; index < results->len; ++index) {
        const SsLocationSearchResult *item =
            g_ptr_array_index(results, (guint)index);
        SsLocationSearchResult *copy;
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *title;
        GtkWidget *coordinates;
        g_autofree gchar *coordinate_text = NULL;

        if (item == NULL) {
            continue;
        }
        copy = g_memdup2(item, sizeof(*item));
        g_object_set_data_full(
            G_OBJECT(row),
            "ss-location-result",
            copy,
            g_free);

        title = ss_linux_ui_make_label(
            item->display_name, "setting-label");
        coordinate_text = format_coordinate_pair(
            item->latitude, item->longitude);
        coordinates = ss_linux_ui_make_label(
            coordinate_text, "setting-description");
        gtk_box_append(GTK_BOX(box), title);
        gtk_box_append(GTK_BOX(box), coordinates);
        gtk_widget_add_css_class(row, "location-result-row");
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        gtk_list_box_append(state->location_results, row);
    }

    gtk_widget_set_visible(
        GTK_WIDGET(state->location_results), TRUE);
    ss_linux_date_time_panel_set_status(
        state,
        "Select the matching locality. Its coordinates will become the system geographic location; any nearest IANA time zone is advisory and must be chosen explicitly.",
        false);

    g_ptr_array_unref(results);
    location_search_request_free(request);
}

static void begin_location_search(SsLinuxDateTimePanel *state)
{
    const char *query;

    if (state == NULL || state->location_search == NULL) {
        return;
    }

    query = gtk_editable_get_text(
        GTK_EDITABLE(state->location_search));
    if (query == NULL || query[0] == '\0') {
        ss_linux_date_time_panel_set_status(
            state, "Enter a locality or place name.", true);
        return;
    }

    if (state->location_search_cancellable != NULL) {
        g_cancellable_cancel(state->location_search_cancellable);
        g_clear_object(&state->location_search_cancellable);
    }
    state->location_search_cancellable = g_cancellable_new();
    state->location_search_generation++;

    clear_location_results(state);
    ss_linux_date_time_panel_set_status(
        state, "Searching for matching localities…", false);

    {
        LocationSearchUiRequest *request =
            g_new0(LocationSearchUiRequest, 1);
        request->window = g_object_ref(state->window);
        request->generation = state->location_search_generation;
        ss_location_search_async(
            query,
            state->location_search_cancellable,
            location_search_complete,
            request);
    }
}

void on_location_search_clicked(
    GtkButton *button G_GNUC_UNUSED,
    gpointer user_data)
{
    begin_location_search(user_data);
}

void on_location_search_activate(
    GtkEntry *entry G_GNUC_UNUSED,
    gpointer user_data)
{
    begin_location_search(user_data);
}

static gboolean commit_location_coordinates(gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    double latitude;
    double longitude;
    const InfiltratrTemporalPolicyV3 *policy;
    InfiltratrTemporalPolicyV3 previous_policy;
    SsLocationMetadata previous_metadata;
    bool previous_metadata_present;

    if (state == NULL) {
        return G_SOURCE_REMOVE;
    }
    state->coordinate_commit_id = 0U;
    if (state->updating_controls) {
        return G_SOURCE_REMOVE;
    }

    latitude = gtk_spin_button_get_value(state->latitude);
    longitude = gtk_spin_button_get_value(state->longitude);
    if (!ss_location_metadata_transaction_begin()) {
        ss_linux_date_time_panel_set_status(
            state,
            "Could not acquire the locality transaction lock.",
            true);
        ss_linux_date_time_panel_sync_controls(state);
        return G_SOURCE_REMOVE;
    }
    if (!ss_date_time_model_reload(&state->model)) {
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not reload the authoritative temporal policy before saving custom coordinates.",
            true);
        ss_linux_date_time_panel_sync_controls(state);
        return G_SOURCE_REMOVE;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL) {
        ss_location_metadata_transaction_end();
        return G_SOURCE_REMOVE;
    }
    previous_policy = *policy;
    previous_metadata = state->location_metadata;
    previous_metadata_present = state->location_metadata_present;

    memset(
        &state->location_metadata,
        0,
        sizeof(state->location_metadata));
    (void)g_strlcpy(
        state->location_metadata.display_name,
        "Custom coordinates",
        sizeof(state->location_metadata.display_name));
    state->location_metadata.latitude = latitude;
    state->location_metadata.longitude = longitude;

    if (state->system_time_service != NULL) {
        SsSystemTimeState current_system;
        if (ss_system_time_service_read(
                state->system_time_service, &current_system) &&
            current_system.timezone[0] != '\0') {
            (void)g_strlcpy(
                state->location_metadata.timezone_id,
                current_system.timezone,
                sizeof(state->location_metadata.timezone_id));
        }
    }

    if (!ss_location_metadata_stage(&state->location_metadata)) {
        state->location_metadata = previous_metadata;
        state->location_metadata_present = previous_metadata_present;
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not stage custom geographic coordinates.",
            true);
        ss_linux_date_time_panel_sync_controls(state);
        return G_SOURCE_REMOVE;
    }

    if (!ss_date_time_model_set_location(
            &state->model, true, latitude, longitude)) {
        ss_location_metadata_discard_staged();
        state->location_metadata = previous_metadata;
        state->location_metadata_present = previous_metadata_present;
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not save custom geographic coordinates.",
            true);
        ss_linux_date_time_panel_sync_controls(state);
        return G_SOURCE_REMOVE;
    }

    state->location_metadata_present =
        ss_location_metadata_finish_staged();
    if (!state->location_metadata_present) {
        const bool restored = ss_date_time_model_set_location(
            &state->model,
            previous_policy.location_configured,
            previous_policy.latitude,
            previous_policy.longitude);
        const bool reloaded = restored
            ? true
            : ss_date_time_model_reload(&state->model);

        if (restored) {
            ss_location_metadata_discard_staged();
        }
        state->location_metadata = previous_metadata;
        state->location_metadata_present = previous_metadata_present;
        if (!previous_metadata_present) {
            memset(
                &state->location_metadata,
                0,
                sizeof(state->location_metadata));
        }

        ss_linux_date_time_panel_set_status(
            state,
            restored
                ? "Custom coordinates could not be saved; the previous location and metadata were restored."
                : (reloaded
                    ? "Custom coordinates could not be saved and rollback persistence failed; authoritative policy was reloaded and previous metadata restored."
                    : "Custom coordinates could not be saved, and neither rollback nor authoritative reload succeeded; previous metadata was restored in memory."),
            true);
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_sync_controls(state);
        return G_SOURCE_REMOVE;
    }

    ss_location_metadata_transaction_end();
    state->location_follows_timezone_reference = false;
    gtk_editable_set_text(
        GTK_EDITABLE(state->location_search),
        "Custom coordinates");
    ss_linux_date_time_panel_policy_saved(state);
    return G_SOURCE_REMOVE;
}

void on_location_coordinate_changed(
    GObject *object G_GNUC_UNUSED,
    GParamSpec *pspec G_GNUC_UNUSED,
    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;

    if (state == NULL || state->updating_controls) {
        return;
    }

    ss_linux_date_time_location_cancel_coordinate_commit(state);
    state->coordinate_commit_id = g_timeout_add(
        300U,
        commit_location_coordinates,
        state);
    g_source_set_name_by_id(
        state->coordinate_commit_id,
        "[system-settings] coordinate transaction debounce");
}
