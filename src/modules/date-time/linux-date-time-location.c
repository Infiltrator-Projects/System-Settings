// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-location.c
 * @brief Locality search, coordinate transactions and location presentation.
 *
 * Locality compiles against a deliberately narrow facade. It owns its mutable
 * state and may coordinate with the Date & Time composition root only through
 * explicit model, regional-context, lifetime and status/synchronisation APIs.
 */

#include "linux-date-time-location-private.h"
#include "linux-ui-helpers.h"

#include "system-settings/location-search.h"
#include "system-settings/regional-context.h"

#include <string.h>

#define LOCATION(state) (*ss_linux_date_time_location_state((state)))
#define LOCATION_CONST(state) (*ss_linux_date_time_location_state_const((state)))
#define MODEL(state) (*ss_linux_date_time_location_model((state)))
#define MODEL_CONST(state) (*ss_linux_date_time_location_model_const((state)))
#define REGIONAL(state) (*ss_linux_date_time_location_regional_context((state)))
#define REGIONAL_CONST(state) \
    (*ss_linux_date_time_location_regional_context_const((state)))

typedef struct LocationSearchUiRequest {
    SsLinuxDateTimeLifetime *lifetime;
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

/*
 * Snapshot locality metadata only after the caller owns the locality lock.
 * Missing metadata is a valid empty state; an existing but unreadable file is
 * an error because replacing it from a stale in-memory copy would lose a
 * concurrent writer's committed locality.
 */
static bool load_authoritative_location_metadata(
    SsLocationMetadata *metadata,
    bool *present)
{
    SsLocationMetadataLoadStatus status;

    if (metadata == NULL || present == NULL) return false;
    memset(metadata, 0, sizeof(*metadata));
    status = ss_location_metadata_load_status(metadata);
    if (status == SS_LOCATION_METADATA_LOAD_OK) {
        *present = true;
        return true;
    }
    if (status == SS_LOCATION_METADATA_LOAD_MISSING) {
        *present = false;
        return true;
    }
    return false;
}

bool ss_linux_date_time_location_metadata_matches_policy(
    const SsLinuxDateTimePanel *state,
    const InfiltratrTemporalPolicyV3 *policy)
{
    return state != NULL &&
           policy != NULL &&
           policy->location_configured &&
           LOCATION_CONST(state).metadata_present &&
           ss_linux_date_time_location_coordinate_close(
               policy->latitude,
               LOCATION_CONST(state).metadata.latitude) &&
           ss_linux_date_time_location_coordinate_close(
               policy->longitude,
               LOCATION_CONST(state).metadata.longitude);
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

    if (state == NULL || LOCATION(state).summary == NULL) {
        return;
    }

    policy = ss_date_time_model_policy(&MODEL(state));
    if (policy == NULL) {
        return;
    }

    if (policy->location_configured) {
        coordinates = format_coordinate_pair(
            policy->latitude, policy->longitude);
        if (ss_linux_date_time_location_metadata_matches_policy(
                state, policy) &&
            LOCATION(state).metadata.display_name[0] != '\0') {
            summary = g_strdup_printf(
                "%s • %s",
                LOCATION(state).metadata.display_name,
                coordinates);
        } else {
            summary = g_strdup_printf(
                "Selected geographic coordinates • %s",
                coordinates);
        }
    } else if (REGIONAL(state).has_reference_coordinates) {
        const char *city =
            REGIONAL(state).timezone_city[0] != '\0'
                ? REGIONAL(state).timezone_city
                : REGIONAL(state).timezone_id;
        coordinates = format_coordinate_pair(
            REGIONAL(state).reference_latitude,
            REGIONAL(state).reference_longitude);
        summary = g_strdup_printf(
            "%s is the current approximation from system time zone %s • %s. "
            "Search for your actual locality to refine it.",
            city,
            REGIONAL(state).timezone_id,
            coordinates);
    } else {
        summary = g_strdup(
            "Search for a locality to establish geographic coordinates.");
    }

    gtk_label_set_text(GTK_LABEL(LOCATION(state).summary), summary);
}

bool ss_linux_date_time_location_policy_matches_reference(
    const SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL ||
        !REGIONAL_CONST(state).has_reference_coordinates) {
        return false;
    }

    policy = ss_date_time_model_policy(&MODEL_CONST(state));
    return policy != NULL &&
           policy->location_configured &&
           ss_linux_date_time_location_coordinate_close(
               policy->latitude,
               REGIONAL_CONST(state).reference_latitude) &&
           ss_linux_date_time_location_coordinate_close(
               policy->longitude,
               REGIONAL_CONST(state).reference_longitude);
}

void ss_linux_date_time_location_cancel_coordinate_commit(
    SsLinuxDateTimePanel *state)
{
    if (state != NULL && LOCATION(state).coordinate_commit_id != 0U) {
        g_source_remove(LOCATION(state).coordinate_commit_id);
        LOCATION(state).coordinate_commit_id = 0U;
    }
}

static void clear_location_results(SsLinuxDateTimePanel *state)
{
    GtkWidget *child;

    if (state == NULL || LOCATION(state).results == NULL) {
        return;
    }

    child = gtk_widget_get_first_child(
        GTK_WIDGET(LOCATION(state).results));
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(LOCATION(state).results, child);
        child = next;
    }
    gtk_widget_set_visible(
        GTK_WIDGET(LOCATION(state).results), FALSE);
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
    if (!ss_date_time_model_reload(&MODEL(state))) {
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not reload the authoritative temporal policy before saving the locality.",
            true);
        return;
    }

    if (!load_authoritative_location_metadata(
            &previous_metadata,
            &previous_metadata_present)) {
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not reload the authoritative locality metadata before saving the selected location.",
            true);
        return;
    }
    LOCATION(state).metadata = previous_metadata;
    LOCATION(state).metadata_present = previous_metadata_present;

    policy = ss_date_time_model_policy(&MODEL(state));
    if (policy == NULL) {
        ss_location_metadata_transaction_end();
        return;
    }
    previous_policy = *policy;

    memset(
        &LOCATION(state).metadata,
        0,
        sizeof(LOCATION(state).metadata));
    (void)g_strlcpy(
        LOCATION(state).metadata.display_name,
        result->display_name,
        sizeof(LOCATION(state).metadata.display_name));
    (void)g_strlcpy(
        LOCATION(state).metadata.country_code,
        result->country_code,
        sizeof(LOCATION(state).metadata.country_code));
    LOCATION(state).metadata.latitude = result->latitude;
    LOCATION(state).metadata.longitude = result->longitude;

    if (ss_linux_date_time_panel_read_system_time(
            state, &current_system) &&
        current_system.timezone[0] != '\0') {
        (void)g_strlcpy(
            LOCATION(state).metadata.timezone_id,
            current_system.timezone,
            sizeof(LOCATION(state).metadata.timezone_id));
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
    if (!ss_location_metadata_stage(&LOCATION(state).metadata)) {
        LOCATION(state).metadata = previous_metadata;
        LOCATION(state).metadata_present = previous_metadata_present;
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not stage the selected locality metadata.",
            true);
        return;
    }

    if (!ss_date_time_model_set_location(
            &MODEL(state),
            true,
            result->latitude,
            result->longitude)) {
        ss_location_metadata_discard_staged();
        LOCATION(state).metadata = previous_metadata;
        LOCATION(state).metadata_present = previous_metadata_present;
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
    LOCATION(state).metadata_present =
        ss_location_metadata_finish_staged();
    if (!LOCATION(state).metadata_present) {
        const bool restored = ss_date_time_model_set_location(
            &MODEL(state),
            previous_policy.location_configured,
            previous_policy.latitude,
            previous_policy.longitude);
        const bool reloaded = restored
            ? true
            : ss_date_time_model_reload(&MODEL(state));

        if (restored) {
            ss_location_metadata_discard_staged();
        }
        LOCATION(state).metadata = previous_metadata;
        LOCATION(state).metadata_present = previous_metadata_present;
        if (!previous_metadata_present) {
            memset(
                &LOCATION(state).metadata,
                0,
                sizeof(LOCATION(state).metadata));
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
    LOCATION(state).follows_timezone_reference = false;
    LOCATION(state).updating_controls = true;
    gtk_editable_set_text(
        GTK_EDITABLE(LOCATION(state).search_entry),
        result->display_name);
    gtk_spin_button_set_value(
        LOCATION(state).latitude, result->latitude);
    gtk_spin_button_set_value(
        LOCATION(state).longitude, result->longitude);
    LOCATION(state).updating_controls = false;
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
    ss_linux_date_time_lifetime_unref(request->lifetime);
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

    if (request == NULL || request->lifetime == NULL) {
        if (results != NULL) {
            g_ptr_array_unref(results);
        }
        location_search_request_free(request);
        return;
    }

    state = ss_linux_date_time_lifetime_get_panel(request->lifetime);
    if (state == NULL ||
        request->generation != LOCATION(state).search_generation) {
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
        gtk_list_box_append(LOCATION(state).results, row);
    }

    gtk_widget_set_visible(
        GTK_WIDGET(LOCATION(state).results), TRUE);
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

    if (state == NULL || LOCATION(state).search_entry == NULL) {
        return;
    }

    query = gtk_editable_get_text(
        GTK_EDITABLE(LOCATION(state).search_entry));
    if (query == NULL || query[0] == '\0') {
        ss_linux_date_time_panel_set_status(
            state, "Enter a locality or place name.", true);
        return;
    }

    if (LOCATION(state).search_cancellable != NULL) {
        g_cancellable_cancel(LOCATION(state).search_cancellable);
        g_clear_object(&LOCATION(state).search_cancellable);
    }
    LOCATION(state).search_cancellable = g_cancellable_new();
    LOCATION(state).search_generation++;

    clear_location_results(state);
    ss_linux_date_time_panel_set_status(
        state, "Searching for matching localities…", false);

    {
        LocationSearchUiRequest *request =
            g_new0(LocationSearchUiRequest, 1);
        request->lifetime = ss_linux_date_time_lifetime_ref(
            ss_linux_date_time_location_lifetime(state));
        request->generation = LOCATION(state).search_generation;
        ss_location_search_async(
            query,
            LOCATION(state).search_cancellable,
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
    LOCATION(state).coordinate_commit_id = 0U;
    if (LOCATION(state).updating_controls) {
        return G_SOURCE_REMOVE;
    }

    latitude = gtk_spin_button_get_value(LOCATION(state).latitude);
    longitude = gtk_spin_button_get_value(LOCATION(state).longitude);
    if (!ss_location_metadata_transaction_begin()) {
        ss_linux_date_time_panel_set_status(
            state,
            "Could not acquire the locality transaction lock.",
            true);
        ss_linux_date_time_panel_sync_controls(state);
        return G_SOURCE_REMOVE;
    }
    if (!ss_date_time_model_reload(&MODEL(state))) {
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not reload the authoritative temporal policy before saving custom coordinates.",
            true);
        ss_linux_date_time_panel_sync_controls(state);
        return G_SOURCE_REMOVE;
    }

    if (!load_authoritative_location_metadata(
            &previous_metadata,
            &previous_metadata_present)) {
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not reload the authoritative locality metadata before saving custom coordinates.",
            true);
        ss_linux_date_time_panel_sync_controls(state);
        return G_SOURCE_REMOVE;
    }
    LOCATION(state).metadata = previous_metadata;
    LOCATION(state).metadata_present = previous_metadata_present;

    policy = ss_date_time_model_policy(&MODEL(state));
    if (policy == NULL) {
        ss_location_metadata_transaction_end();
        return G_SOURCE_REMOVE;
    }
    previous_policy = *policy;

    memset(
        &LOCATION(state).metadata,
        0,
        sizeof(LOCATION(state).metadata));
    (void)g_strlcpy(
        LOCATION(state).metadata.display_name,
        "Custom coordinates",
        sizeof(LOCATION(state).metadata.display_name));
    LOCATION(state).metadata.latitude = latitude;
    LOCATION(state).metadata.longitude = longitude;

    {
        SsSystemTimeState current_system;
        if (ss_linux_date_time_panel_read_system_time(
                state, &current_system) &&
            current_system.timezone[0] != '\0') {
            (void)g_strlcpy(
                LOCATION(state).metadata.timezone_id,
                current_system.timezone,
                sizeof(LOCATION(state).metadata.timezone_id));
        }
    }

    if (!ss_location_metadata_stage(&LOCATION(state).metadata)) {
        LOCATION(state).metadata = previous_metadata;
        LOCATION(state).metadata_present = previous_metadata_present;
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not stage custom geographic coordinates.",
            true);
        ss_linux_date_time_panel_sync_controls(state);
        return G_SOURCE_REMOVE;
    }

    if (!ss_date_time_model_set_location(
            &MODEL(state), true, latitude, longitude)) {
        ss_location_metadata_discard_staged();
        LOCATION(state).metadata = previous_metadata;
        LOCATION(state).metadata_present = previous_metadata_present;
        ss_location_metadata_transaction_end();
        ss_linux_date_time_panel_set_status(
            state,
            "Could not save custom geographic coordinates.",
            true);
        ss_linux_date_time_panel_sync_controls(state);
        return G_SOURCE_REMOVE;
    }

    LOCATION(state).metadata_present =
        ss_location_metadata_finish_staged();
    if (!LOCATION(state).metadata_present) {
        const bool restored = ss_date_time_model_set_location(
            &MODEL(state),
            previous_policy.location_configured,
            previous_policy.latitude,
            previous_policy.longitude);
        const bool reloaded = restored
            ? true
            : ss_date_time_model_reload(&MODEL(state));

        if (restored) {
            ss_location_metadata_discard_staged();
        }
        LOCATION(state).metadata = previous_metadata;
        LOCATION(state).metadata_present = previous_metadata_present;
        if (!previous_metadata_present) {
            memset(
                &LOCATION(state).metadata,
                0,
                sizeof(LOCATION(state).metadata));
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
    LOCATION(state).follows_timezone_reference = false;
    gtk_editable_set_text(
        GTK_EDITABLE(LOCATION(state).search_entry),
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

    if (state == NULL || LOCATION(state).updating_controls) {
        return;
    }

    ss_linux_date_time_location_cancel_coordinate_commit(state);
    LOCATION(state).coordinate_commit_id = g_timeout_add(
        300U,
        commit_location_coordinates,
        state);
    g_source_set_name_by_id(
        LOCATION(state).coordinate_commit_id,
        "[system-settings] coordinate transaction debounce");
}
