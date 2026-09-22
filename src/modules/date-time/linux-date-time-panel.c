// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-panel.c
 * @brief Built-in Linux Date & Time settings module.
 *
 * Domain state, native backend reconciliation, async operations and GTK panel
 * construction live here rather than in the generic application shell.
 * The GTK-facing header is private to the current built-in integration and is
 * not the future public module ABI.
 */

#include "linux-date-time-panel.h"
#include "linux-ui-helpers.h"
#include "calendar-preview-provider.h"

#include "system-settings/cinnamon-interface.h"
#include "system-settings/date-time-model.h"
#include "system-settings/location-metadata.h"
#include "system-settings/location-search.h"
#include "system-settings/manual-time.h"
#include "system-settings/native-clock-policy.h"
#include "system-settings/regional-context.h"
#include "system-settings/system-time-service.h"
#include "system-settings/temporal-policy-store.h"

#include <infiltratr/core.h>
#include <infiltratr/temporal.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct SsLinuxDateTimePanel {
    GtkWindow *window;
    GtkWidget *root;
    GtkWidget *clock_preview;
    GtkWidget *date_preview;
    GtkDropDown *clock_mode;
    GtkDropDown *calendar;
    GtkSwitch *show_seconds;
    GtkEntry *location_search;
    GtkButton *location_search_button;
    GtkListBox *location_results;
    GtkWidget *location_summary;
    GtkSpinButton *latitude;
    GtkSpinButton *longitude;
    GtkDropDown *timezone;
    GtkSwitch *network_time;
    GtkEntry *manual_date;
    GtkEntry *manual_time;
    GtkButton *manual_set_time;
    GtkWidget *manual_row;
    GtkWidget *manual_unavailable;
    GtkSwitch *show_date;
    GtkDropDown *first_day;
    GtkWidget *status_label;
    GSettings *cinnamon_interface_settings;
    SsDateTimeModel model;
    SsRegionalContext regional_context;
    SsLocationMetadata location_metadata;
    SsSystemTimeService *system_time_service;
    SsCalendarPreviewProvider *calendar_preview_provider;
    GPtrArray *clock_mode_ids;
    GPtrArray *timezone_ids;
    GCancellable *location_search_cancellable;
    GCancellable *system_time_cancellable;
    guint timer_id;
    guint location_search_generation;
    bool location_metadata_present;
    bool location_follows_timezone_reference;
    bool updating_controls;
    bool updating_system_controls;
};

typedef struct LocationSearchUiRequest {
    GtkWindow *window;
    guint generation;
} LocationSearchUiRequest;

static void set_status(SsLinuxDateTimePanel *state,
                       const char *message,
                       bool error)
{
    if (state == NULL || state->status_label == NULL) {
        return;
    }
    gtk_label_set_text(GTK_LABEL(state->status_label), message);
    gtk_widget_remove_css_class(state->status_label, "status-ok");
    gtk_widget_remove_css_class(state->status_label, "error");
    gtk_widget_add_css_class(state->status_label,
                             error ? "error" : "status-ok");
}

static GtkStringList *clock_mode_strings(
    SsLinuxDateTimePanel *state)
{
    GtkStringList *list = gtk_string_list_new(NULL);
    size_t index;

    if (state == NULL) {
        return list;
    }

    g_clear_pointer(&state->clock_mode_ids, g_ptr_array_unref);
    state->clock_mode_ids = g_ptr_array_new_with_free_func(g_free);

    for (index = 0U; index < infiltratr_temporal_clock_mode_count(); ++index) {
        const InfiltratrTemporalClockModeInfo *info =
            infiltratr_temporal_clock_mode_at(index);

        /*
         * "standard" remains an internal Common/bootstrap identifier so
         * consumers can run without System Settings. Once System Settings is
         * present it is not a user-facing third conventional clock choice:
         * the explicit 12-hour and 24-hour systems are the settings authority.
         */
        if (info != NULL &&
            !ss_native_clock_mode_is_legacy_default(info->id)) {
            g_ptr_array_add(state->clock_mode_ids, g_strdup(info->id));
            gtk_string_list_append(list, info->name);
        }
    }
    return list;
}

static GtkStringList *calendar_strings(void)
{
    GtkStringList *list = gtk_string_list_new(NULL);
    size_t index;

    for (index = 0U; index < infiltratr_temporal_calendar_count(); ++index) {
        const InfiltratrTemporalCalendarInfo *info =
            infiltratr_temporal_calendar_at(index);
        if (info != NULL) {
            gtk_string_list_append(list, info->name);
        }
    }
    return list;
}

/*
 * The preview bridge belongs to the Date & Time panel, not to timedated
 * notifications or control-list construction. Keep one provider for the
 * lifetime of the panel so every specialised clock and calendar follows the
 * same runtime-discovery/retry path.
 */
static SsCalendarPreviewProvider *ensure_calendar_preview_provider(
    SsLinuxDateTimePanel *state)
{
    if (state == NULL) {
        return NULL;
    }

    if (state->calendar_preview_provider == NULL) {
        state->calendar_preview_provider =
            ss_calendar_preview_provider_new();
    }
    return state->calendar_preview_provider;
}

static bool coordinate_close(double left, double right)
{
    double difference = left - right;

    if (difference < 0.0) {
        difference = -difference;
    }
    return difference < 0.000001;
}

static bool location_metadata_matches_policy(
    const SsLinuxDateTimePanel *state,
    const InfiltratrTemporalPolicyV3 *policy)
{
    return state != NULL &&
           policy != NULL &&
           policy->location_configured &&
           state->location_metadata_present &&
           coordinate_close(
               policy->latitude, state->location_metadata.latitude) &&
           coordinate_close(
               policy->longitude, state->location_metadata.longitude);
}

static gchar *format_coordinate_pair(double latitude, double longitude)
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

static GtkStringList *timezone_strings(SsLinuxDateTimePanel *state)
{
    GtkStringList *strings = gtk_string_list_new(NULL);
    size_t index;

    if (state == NULL) {
        return strings;
    }

    g_clear_pointer(&state->timezone_ids, g_ptr_array_unref);
    state->timezone_ids = ss_regional_context_list_timezones();

    if (state->timezone_ids == NULL) {
        state->timezone_ids =
            g_ptr_array_new_with_free_func(g_free);
    }

    if (state->timezone_ids->len == 0U &&
        state->regional_context.timezone_id[0] != '\0') {
        g_ptr_array_add(
            state->timezone_ids,
            g_strdup(state->regional_context.timezone_id));
    }

    for (index = 0U; index < state->timezone_ids->len; ++index) {
        const char *zone = g_ptr_array_index(
            state->timezone_ids, (guint)index);
        gtk_string_list_append(strings, zone);
    }
    return strings;
}

static guint timezone_index_for_id(
    const SsLinuxDateTimePanel *state,
    const char *timezone_id)
{
    size_t index;

    if (state == NULL || state->timezone_ids == NULL ||
        timezone_id == NULL) {
        return GTK_INVALID_LIST_POSITION;
    }

    for (index = 0U; index < state->timezone_ids->len; ++index) {
        const char *candidate = g_ptr_array_index(
            state->timezone_ids, (guint)index);
        if (g_strcmp0(candidate, timezone_id) == 0) {
            return (guint)index;
        }
    }
    return GTK_INVALID_LIST_POSITION;
}

static GtkStringList *first_day_strings(void)
{
    const char *const values[] = {
        "Use locale default",
        "Sunday",
        "Monday",
        NULL
    };
    return gtk_string_list_new(values);
}

static guint first_day_index(int value)
{
    if (value == 0) {
        return 1U;
    }
    if (value == 1) {
        return 2U;
    }
    return 0U;
}

static int first_day_value(guint index)
{
    if (index == 1U) {
        return 0;
    }
    if (index == 2U) {
        return 1;
    }
    return 7;
}

static void update_location_summary(SsLinuxDateTimePanel *state)
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
        if (location_metadata_matches_policy(state, policy) &&
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

static void sync_native_format_controls(SsLinuxDateTimePanel *state)
{
    bool value;
    int first_day = 7;

    if (state == NULL || state->cinnamon_interface_settings == NULL) {
        return;
    }

    state->updating_system_controls = true;
    if (state->show_date != NULL &&
        ss_cinnamon_interface_get_boolean(
            state->cinnamon_interface_settings,
            "clock-show-date",
            &value)) {
        gtk_switch_set_active(state->show_date, value);
    }
    if (state->first_day != NULL &&
        ss_cinnamon_interface_get_first_day(
            state->cinnamon_interface_settings,
            &first_day)) {
        gtk_drop_down_set_selected(
            state->first_day, first_day_index(first_day));
    }
    state->updating_system_controls = false;
}

static bool desktop_uses_24h(const SsLinuxDateTimePanel *state)
{
    bool value = true;

    if (state != NULL &&
        state->cinnamon_interface_settings != NULL) {
        (void)ss_cinnamon_interface_get_boolean(
            state->cinnamon_interface_settings,
            "clock-use-24h",
            &value);
    }
    return value;
}

static bool manual_representation_supported(
    const SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL) {
        return false;
    }

    policy = ss_date_time_model_policy(&state->model);
    return policy != NULL &&
           strcmp(policy->calendar, "gregorian") == 0 &&
           ss_manual_time_mode_supported(policy->clock_mode);
}

static bool location_policy_matches_reference(
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
           coordinate_close(
               policy->latitude,
               state->regional_context.reference_latitude) &&
           coordinate_close(
               policy->longitude,
               state->regional_context.reference_longitude);
}

static void fill_manual_time_entries(SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy;
    g_autoptr(GDateTime) now = NULL;
    g_autofree gchar *date = NULL;
    char time[64];
    int64_t unix_microseconds;
    gint64 offset_microseconds;

    if (state == NULL || state->manual_date == NULL ||
        state->manual_time == NULL) {
        return;
    }

    policy = ss_date_time_model_policy(&state->model);
    now = g_date_time_new_now_local();
    if (policy == NULL || now == NULL ||
        !manual_representation_supported(state)) {
        return;
    }

    date = g_date_time_format(now, "%Y-%m-%d");
    unix_microseconds = g_get_real_time();
    offset_microseconds = g_date_time_get_utc_offset(now);

    if (date != NULL) {
        gtk_editable_set_text(
            GTK_EDITABLE(state->manual_date), date);
    }
    if (ss_manual_time_format(
            policy->clock_mode,
            desktop_uses_24h(state),
            unix_microseconds,
            (int32_t)(offset_microseconds / G_USEC_PER_SEC),
            policy->show_seconds,
            time,
            sizeof(time))) {
        gtk_editable_set_text(
            GTK_EDITABLE(state->manual_time), time);
    }
}

static void sync_system_time_controls(SsLinuxDateTimePanel *state)
{
    SsSystemTimeState system_state;
    guint zone_index;
    bool manual_supported;
    bool manual_visible;

    if (state == NULL) {
        return;
    }

    if (state->system_time_service == NULL ||
        !ss_system_time_service_read(
            state->system_time_service, &system_state)) {
        if (state->timezone != NULL) {
            gtk_widget_set_sensitive(
                GTK_WIDGET(state->timezone), FALSE);
        }
        if (state->network_time != NULL) {
            gtk_widget_set_sensitive(
                GTK_WIDGET(state->network_time), FALSE);
        }
        if (state->manual_row != NULL) {
            gtk_widget_set_visible(state->manual_row, FALSE);
        }
        if (state->manual_unavailable != NULL) {
            gtk_widget_set_visible(
                state->manual_unavailable, FALSE);
        }
        return;
    }

    state->updating_system_controls = true;

    if (state->timezone != NULL) {
        zone_index = timezone_index_for_id(
            state, system_state.timezone);
        if (zone_index != GTK_INVALID_LIST_POSITION) {
            gtk_drop_down_set_selected(
                state->timezone, zone_index);
        }
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->timezone), TRUE);
    }

    if (state->network_time != NULL) {
        gtk_switch_set_active(
            state->network_time, system_state.ntp_enabled);
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->network_time),
            system_state.can_ntp);
    }

    manual_supported = manual_representation_supported(state);
    manual_visible = !system_state.ntp_enabled && manual_supported;

    if (state->manual_row != NULL) {
        gtk_widget_set_visible(state->manual_row, manual_visible);
    }
    if (state->manual_unavailable != NULL) {
        gtk_widget_set_visible(
            state->manual_unavailable,
            !system_state.ntp_enabled && !manual_supported);
    }

    if (manual_visible) {
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->manual_date), TRUE);
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->manual_time), TRUE);
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->manual_set_time), TRUE);
        fill_manual_time_entries(state);
    }

    state->updating_system_controls = false;
}

static guint clock_index_for_id(
    const SsLinuxDateTimePanel *state,
    const char *id)
{
    const char *effective_id = id;
    bool use_24h = true;
    size_t index;

    if (state == NULL || state->clock_mode_ids == NULL || id == NULL) {
        return 0U;
    }

    if (ss_native_clock_mode_is_legacy_default(id)) {
        if (state->cinnamon_interface_settings != NULL) {
            (void)ss_cinnamon_interface_get_boolean(
                state->cinnamon_interface_settings,
                "clock-use-24h",
                &use_24h);
        }
        effective_id = ss_native_clock_mode_id(use_24h);
    }

    for (index = 0U; index < state->clock_mode_ids->len; ++index) {
        const char *candidate =
            g_ptr_array_index(state->clock_mode_ids, (guint)index);
        if (g_strcmp0(candidate, effective_id) == 0) {
            return (guint)index;
        }
    }
    return 0U;
}

static guint calendar_index_for_id(const char *id)
{
    size_t index;

    for (index = 0U; index < infiltratr_temporal_calendar_count(); ++index) {
        const InfiltratrTemporalCalendarInfo *info =
            infiltratr_temporal_calendar_at(index);
        if (info != NULL && strcmp(info->id, id) == 0) {
            return (guint)index;
        }
    }
    return 0U;
}

static const InfiltratrTemporalClockModeInfo *
selected_clock_mode(const SsLinuxDateTimePanel *state)
{
    guint selected;
    const char *id;

    if (state == NULL || state->clock_mode == NULL ||
        state->clock_mode_ids == NULL) {
        return NULL;
    }

    selected = gtk_drop_down_get_selected(state->clock_mode);
    if (selected >= state->clock_mode_ids->len) {
        return NULL;
    }

    id = g_ptr_array_index(state->clock_mode_ids, selected);
    return infiltratr_temporal_clock_mode_find(id);
}

static const InfiltratrTemporalCalendarInfo *
selected_calendar(GtkDropDown *dropdown)
{
    size_t index;
    if (dropdown == NULL) {
        return NULL;
    }
    index = (size_t)gtk_drop_down_get_selected(dropdown);
    return infiltratr_temporal_calendar_at(index);
}

static void update_control_capabilities(SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalClockModeInfo *mode;
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL) {
        return;
    }

    mode = selected_clock_mode(state);
    policy = ss_date_time_model_policy(&state->model);
    gtk_widget_set_sensitive(GTK_WIDGET(state->show_seconds),
                             mode == NULL || mode->supports_seconds);
    gtk_widget_set_sensitive(GTK_WIDGET(state->latitude), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(state->longitude), TRUE);

    update_location_summary(state);
    sync_native_format_controls(state);
    sync_system_time_controls(state);

    if (mode != NULL &&
        (mode->requires_latitude || mode->requires_longitude) &&
        (policy == NULL || !policy->location_configured) &&
        !state->regional_context.has_reference_coordinates) {
        set_status(state,
                   "This clock system needs a geographic location for a meaningful result.",
                   false);
    }
}

static bool preview_coordinates(
    const SsLinuxDateTimePanel *state,
    double *latitude,
    double *longitude)
{
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL || latitude == NULL || longitude == NULL) {
        return false;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy != NULL && policy->location_configured) {
        *latitude = policy->latitude;
        *longitude = policy->longitude;
        return true;
    }

    if (state->regional_context.has_reference_coordinates) {
        *latitude = state->regional_context.reference_latitude;
        *longitude = state->regional_context.reference_longitude;
        return true;
    }

    return false;
}

static bool format_preview(SsLinuxDateTimePanel *state,
                           char *buffer,
                           size_t capacity)
{
    const InfiltratrTemporalPolicyV3 *policy =
        ss_date_time_model_policy(&state->model);
    const InfiltratrTemporalClockModeInfo *mode;
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree gchar *native_text = NULL;
    int64_t unix_us;
    gint64 offset_us;
    InfiltratrClockProfile conventional;
    double latitude = 0.0;
    double longitude = 0.0;
    bool has_location;

    if (policy == NULL || now == NULL || buffer == NULL || capacity == 0U) {
        return false;
    }

    if (strcmp(policy->clock_mode, "standard") == 0) {
        const gboolean use_24h =
            state->cinnamon_interface_settings != NULL
                ? g_settings_get_boolean(
                    state->cinnamon_interface_settings, "clock-use-24h")
                : TRUE;
        conventional = use_24h
            ? INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24
            : INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12;
    } else if (strcmp(policy->clock_mode, "standard-12") == 0) {
        conventional = INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12;
    } else if (strcmp(policy->clock_mode, "standard-24") == 0) {
        conventional = INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24;
    } else if (strcmp(policy->clock_mode, "decimal") == 0) {
        conventional = INFILTRATR_CLOCK_PROFILE_DECIMAL_10;
    } else {
        mode = infiltratr_temporal_clock_mode_find(policy->clock_mode);
        if (mode == NULL) {
            return false;
        }

        has_location = preview_coordinates(
            state, &latitude, &longitude);
        if ((mode->requires_latitude || mode->requires_longitude) &&
            !has_location) {
            return g_snprintf(
                       buffer,
                       capacity,
                       "%s — location required",
                       mode->name) > 0;
        }

        /*
         * Calendar owns the specialised clock algorithms. Do not fake a
         * preview by displaying the mode's label: ask Calendar's stable
         * runtime ABI for the same formatter the panel clock uses.
         */
        unix_us = g_get_real_time();
        offset_us = g_date_time_get_utc_offset(now);
        native_text = ss_calendar_preview_provider_format_clock(
            ensure_calendar_preview_provider(state),
            policy->clock_mode,
            unix_us,
            (int)(offset_us / G_USEC_PER_SEC),
            policy->show_seconds,
            latitude,
            longitude);
        if (native_text != NULL) {
            return g_strlcpy(
                       buffer, native_text, capacity) < capacity;
        }

        return g_snprintf(
                   buffer,
                   capacity,
                   "%s — preview unavailable",
                   mode->name) > 0;
    }

    unix_us = g_get_real_time();
    offset_us = g_date_time_get_utc_offset(now);
    return infiltratr_temporal_format_clock(
        conventional, unix_us,
        (int32_t)(offset_us / G_USEC_PER_SEC),
        policy->show_seconds, buffer, capacity, NULL);
}

static gboolean refresh_preview(gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    const InfiltratrTemporalPolicyV3 *policy;
    const InfiltratrTemporalCalendarInfo *calendar;
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    char clock_text[160];

    if (state == NULL || state->clock_preview == NULL || now == NULL) {
        return G_SOURCE_CONTINUE;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL) {
        return G_SOURCE_CONTINUE;
    }

    if (format_preview(state, clock_text, sizeof(clock_text))) {
        gtk_label_set_text(GTK_LABEL(state->clock_preview), clock_text);
    }

    calendar = infiltratr_temporal_calendar_find(policy->calendar);
    {
        g_autofree gchar *gregorian =
            g_date_time_format(now, "%A, %e %B %Y");
        g_autofree gchar *selected_date = NULL;
        g_autofree gchar *summary = NULL;

        if (calendar != NULL) {
            if (strcmp(policy->calendar, "gregorian") == 0) {
                summary = g_strdup_printf(
                    "%s • %s",
                    calendar->name,
                    gregorian != NULL ? gregorian : "");
            } else {
                selected_date =
                    ss_calendar_preview_provider_format_date(
                        ensure_calendar_preview_provider(state),
                        policy->calendar,
                        g_date_time_get_year(now),
                        g_date_time_get_month(now),
                        g_date_time_get_day_of_month(now));
                if (selected_date != NULL) {
                    summary = g_strdup_printf(
                        "%s • %s",
                        calendar->name,
                        selected_date);
                } else {
                    summary = g_strdup_printf(
                        "%s • preview unavailable",
                        calendar->name);
                }
            }
        }
        if (summary != NULL) {
            gtk_label_set_text(
                GTK_LABEL(state->date_preview), summary);
        }
    }

    return G_SOURCE_CONTINUE;
}

static void sync_controls(SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy =
        ss_date_time_model_policy(&state->model);

    if (policy == NULL) {
        return;
    }

    state->updating_controls = true;
    gtk_drop_down_set_selected(
        state->clock_mode, clock_index_for_id(state, policy->clock_mode));
    gtk_drop_down_set_selected(
        state->calendar,
        calendar_index_for_id(policy->calendar));
    gtk_switch_set_active(state->show_seconds, policy->show_seconds);
    if (policy->location_configured) {
        gtk_spin_button_set_value(
            state->latitude, policy->latitude);
        gtk_spin_button_set_value(
            state->longitude, policy->longitude);
        if (state->location_search != NULL &&
            location_metadata_matches_policy(state, policy)) {
            gtk_editable_set_text(
                GTK_EDITABLE(state->location_search),
                state->location_metadata.display_name);
        }
    } else if (state->regional_context.has_reference_coordinates) {
        gtk_spin_button_set_value(
            state->latitude,
            state->regional_context.reference_latitude);
        gtk_spin_button_set_value(
            state->longitude,
            state->regional_context.reference_longitude);
        if (state->location_search != NULL &&
            state->regional_context.timezone_city[0] != '\0') {
            gtk_editable_set_text(
                GTK_EDITABLE(state->location_search),
                state->regional_context.timezone_city);
        }
    }
    state->updating_controls = false;
    update_control_capabilities(state);
    (void)refresh_preview(state);
}

static void on_cinnamon_interface_changed(
    GSettings *settings G_GNUC_UNUSED,
    gchar *key,
    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    const InfiltratrTemporalPolicyV3 *policy;
    bool native_value = false;
    bool changed = false;

    if (state == NULL || key == NULL) {
        return;
    }

    sync_native_format_controls(state);
    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL) {
        return;
    }

    /*
     * Native Cinnamon settings are a backend, not a competing visible
     * authority. If another program changes a conventional setting that has
     * an exact System Settings representation, reconcile it into our explicit
     * policy. Extended clocks deliberately ignore the native 12/24-hour
     * compatibility fallback.
     */
    if (g_strcmp0(key, "clock-use-24h") == 0 &&
        ss_native_clock_mode_tracks_desktop(policy->clock_mode) &&
        ss_cinnamon_interface_get_boolean(
            state->cinnamon_interface_settings,
            "clock-use-24h",
            &native_value)) {
        const char *desired = ss_native_clock_mode_id(native_value);

        if (g_strcmp0(policy->clock_mode, desired) != 0) {
            changed = ss_date_time_model_set_clock_mode(
                &state->model, desired);
        }
    } else if (g_strcmp0(key, "clock-show-seconds") == 0 &&
               ss_cinnamon_interface_get_boolean(
                   state->cinnamon_interface_settings,
                   "clock-show-seconds",
                   &native_value) &&
               policy->show_seconds != native_value) {
        changed = ss_date_time_model_set_show_seconds(
            &state->model, native_value);
    }

    if (changed) {
        sync_controls(state);
        set_status(
            state,
            "An external Mint/Cinnamon time preference changed and System Settings reconciled the matching setting.",
            false);
        return;
    }

    (void)refresh_preview(state);
}

static void policy_saved(SsLinuxDateTimePanel *state)
{
    set_status(state,
               "System temporal policy saved. Calendar and other Common-aware applications use this setting.",
               false);
    update_control_capabilities(state);
    (void)refresh_preview(state);
}

static void on_clock_changed(GObject *object,
                             GParamSpec *pspec,
                             gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    const InfiltratrTemporalClockModeInfo *mode;

    (void)object;
    (void)pspec;
    if (state == NULL || state->updating_controls) {
        return;
    }

    mode = selected_clock_mode(state);
    if (mode == NULL ||
        !ss_date_time_model_set_clock_mode(&state->model, mode->id)) {
        set_status(state, "Could not save the clock system.", true);
        sync_controls(state);
        return;
    }
    policy_saved(state);
}

static void on_calendar_changed(GObject *object,
                                GParamSpec *pspec,
                                gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    const InfiltratrTemporalCalendarInfo *calendar;

    (void)object;
    (void)pspec;
    if (state == NULL || state->updating_controls) {
        return;
    }

    calendar = selected_calendar(state->calendar);
    if (calendar == NULL ||
        !ss_date_time_model_set_calendar(&state->model, calendar->id)) {
        set_status(state, "Could not save the calendar.", true);
        sync_controls(state);
        return;
    }
    policy_saved(state);
}

static void on_seconds_changed(GObject *object,
                               GParamSpec *pspec,
                               gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    gboolean active;

    (void)pspec;
    if (state == NULL || state->updating_controls) {
        return;
    }

    active = gtk_switch_get_active(GTK_SWITCH(object));
    if (!ss_date_time_model_set_show_seconds(&state->model,
                                             active != FALSE)) {
        set_status(state, "Could not save the seconds setting.", true);
        sync_controls(state);
        return;
    }
    policy_saved(state);
}

static void system_time_changed(
    SsSystemTimeService *service G_GNUC_UNUSED,
    const SsSystemTimeState *system_state,
    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;

    if (state == NULL || system_state == NULL) {
        return;
    }

    (void)ss_regional_context_detect(&state->regional_context);

    if (state->location_metadata_present &&
        system_state->timezone[0] != '\0' &&
        g_strcmp0(
            state->location_metadata.timezone_id,
            system_state->timezone) != 0) {
        (void)g_strlcpy(
            state->location_metadata.timezone_id,
            system_state->timezone,
            sizeof(state->location_metadata.timezone_id));
        (void)ss_location_metadata_save(&state->location_metadata);
    }

    if (state->location_follows_timezone_reference &&
        state->regional_context.has_reference_coordinates) {
        if (ss_date_time_model_set_location(
                &state->model,
                true,
                state->regional_context.reference_latitude,
                state->regional_context.reference_longitude)) {
            state->updating_controls = true;
            gtk_spin_button_set_value(
                state->latitude,
                state->regional_context.reference_latitude);
            gtk_spin_button_set_value(
                state->longitude,
                state->regional_context.reference_longitude);
            if (state->location_search != NULL &&
                state->regional_context.timezone_city[0] != '\0') {
                gtk_editable_set_text(
                    GTK_EDITABLE(state->location_search),
                    state->regional_context.timezone_city);
            }
            state->updating_controls = false;
        }
    }

    sync_system_time_controls(state);
    update_location_summary(state);
    (void)refresh_preview(state);
}

static void system_time_operation_complete(
    SsSystemTimeService *service G_GNUC_UNUSED,
    bool success,
    const char *error_message,
    gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);
    SsLinuxDateTimePanel *state = g_object_get_data(
        G_OBJECT(window), "system-settings-date-time-panel");

    if (state != NULL) {
        if (success) {
            (void)ss_regional_context_detect(&state->regional_context);
            sync_system_time_controls(state);
            update_location_summary(state);
            set_status(
                state,
                "Operating-system date/time settings updated.",
                false);
        } else {
            sync_system_time_controls(state);
            set_status(
                state,
                error_message != NULL
                    ? error_message
                    : "The operating-system date/time setting could not be changed.",
                true);
        }
    }
    g_object_unref(window);
}

static void ensure_system_cancellable(SsLinuxDateTimePanel *state)
{
    if (state != NULL && state->system_time_cancellable == NULL) {
        state->system_time_cancellable = g_cancellable_new();
    }
}

static void request_system_timezone(
    SsLinuxDateTimePanel *state,
    const char *timezone_id)
{
    if (state == NULL || state->system_time_service == NULL ||
        timezone_id == NULL || timezone_id[0] == '\0') {
        return;
    }

    ensure_system_cancellable(state);
    set_status(state, "Updating the operating-system time zone…", false);
    ss_system_time_service_set_timezone_async(
        state->system_time_service,
        timezone_id,
        state->system_time_cancellable,
        system_time_operation_complete,
        g_object_ref(state->window));
}

static void on_timezone_changed(GObject *object G_GNUC_UNUSED,
                                GParamSpec *pspec G_GNUC_UNUSED,
                                gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    guint selected;
    const char *timezone_id;

    if (state == NULL || state->updating_system_controls ||
        state->timezone_ids == NULL) {
        return;
    }

    selected = gtk_drop_down_get_selected(state->timezone);
    if (selected >= state->timezone_ids->len) {
        return;
    }
    timezone_id = g_ptr_array_index(state->timezone_ids, selected);
    request_system_timezone(state, timezone_id);
}

static void on_network_time_changed(GObject *object,
                                    GParamSpec *pspec G_GNUC_UNUSED,
                                    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    bool enabled;

    if (state == NULL || state->updating_system_controls ||
        state->system_time_service == NULL) {
        return;
    }

    enabled = gtk_switch_get_active(GTK_SWITCH(object)) != FALSE;
    ensure_system_cancellable(state);
    set_status(
        state,
        enabled ? "Enabling network time…" : "Disabling network time…",
        false);
    ss_system_time_service_set_ntp_async(
        state->system_time_service,
        enabled,
        state->system_time_cancellable,
        system_time_operation_complete,
        g_object_ref(state->window));
}

static bool parse_manual_datetime(
    const SsLinuxDateTimePanel *state,
    const char *date_text,
    const char *time_text,
    int64_t *unix_time_usec)
{
    const InfiltratrTemporalPolicyV3 *policy;
    int year;
    int month;
    int day;
    char trailing;
    int64_t microseconds_of_day;
    int64_t whole_seconds;
    int hour;
    int minute;
    double seconds;
    g_autoptr(GDateTime) value = NULL;

    if (state == NULL || date_text == NULL || time_text == NULL ||
        unix_time_usec == NULL ||
        !manual_representation_supported(state) ||
        sscanf(date_text,
               "%d-%d-%d%c",
               &year, &month, &day, &trailing) != 3) {
        return false;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL ||
        !ss_manual_time_parse(
            policy->clock_mode,
            desktop_uses_24h(state),
            time_text,
            &microseconds_of_day)) {
        return false;
    }

    if (year < 1970 || year > 9999 ||
        month < 1 || month > 12 ||
        day < 1 || day > 31) {
        return false;
    }

    whole_seconds = microseconds_of_day / G_USEC_PER_SEC;
    hour = (int)(whole_seconds / INT64_C(3600));
    minute = (int)((whole_seconds % INT64_C(3600)) / INT64_C(60));
    seconds =
        (double)(whole_seconds % INT64_C(60)) +
        (double)(microseconds_of_day % G_USEC_PER_SEC) /
            (double)G_USEC_PER_SEC;

    value = g_date_time_new_local(
        year, month, day, hour, minute, seconds);
    if (value == NULL) {
        return false;
    }
    *unix_time_usec =
        (int64_t)g_date_time_to_unix(value) * G_USEC_PER_SEC +
        (microseconds_of_day % G_USEC_PER_SEC);
    return true;
}

static void on_manual_set_time_clicked(
    GtkButton *button G_GNUC_UNUSED,
    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    int64_t unix_time_usec;
    const char *date_text;
    const char *time_text;

    if (state == NULL || state->system_time_service == NULL) {
        return;
    }

    date_text = gtk_editable_get_text(GTK_EDITABLE(state->manual_date));
    time_text = gtk_editable_get_text(GTK_EDITABLE(state->manual_time));
    if (!parse_manual_datetime(
            state, date_text, time_text, &unix_time_usec)) {
        set_status(
            state,
            "Enter a valid Gregorian date and a time in the selected clock system.",
            true);
        return;
    }

    ensure_system_cancellable(state);
    set_status(state, "Setting the operating-system date and time…", false);
    ss_system_time_service_set_time_async(
        state->system_time_service,
        unix_time_usec,
        state->system_time_cancellable,
        system_time_operation_complete,
        g_object_ref(state->window));
}

static void on_show_date_changed(GObject *object,
                                 GParamSpec *pspec G_GNUC_UNUSED,
                                 gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    bool value;

    if (state == NULL || state->updating_system_controls ||
        state->cinnamon_interface_settings == NULL) {
        return;
    }
    value = gtk_switch_get_active(GTK_SWITCH(object)) != FALSE;
    if (!ss_cinnamon_interface_set_boolean(
            state->cinnamon_interface_settings,
            "clock-show-date",
            value)) {
        set_status(state, "Could not change the Cinnamon panel date setting.", true);
        sync_native_format_controls(state);
        return;
    }
    set_status(state, "Cinnamon panel date setting updated.", false);
}

static void on_first_day_changed(GObject *object G_GNUC_UNUSED,
                                 GParamSpec *pspec G_GNUC_UNUSED,
                                 gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    int value;

    if (state == NULL || state->updating_system_controls ||
        state->cinnamon_interface_settings == NULL) {
        return;
    }

    value = first_day_value(
        gtk_drop_down_get_selected(state->first_day));
    if (!ss_cinnamon_interface_set_first_day(
            state->cinnamon_interface_settings, value)) {
        set_status(state, "Could not change the first day of week.", true);
        sync_native_format_controls(state);
        return;
    }
    set_status(state, "First day of week updated.", false);
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

static void on_location_result_activated(
    GtkListBox *box G_GNUC_UNUSED,
    GtkListBoxRow *row,
    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    const SsLocationSearchResult *result;
    char timezone_id[SS_TIMEZONE_ID_CAPACITY] = {0};
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL || row == NULL) {
        return;
    }

    result = g_object_get_data(
        G_OBJECT(row), "ss-location-result");
    if (result == NULL) {
        return;
    }

    if (!ss_date_time_model_set_location(
            &state->model,
            true,
            result->latitude,
            result->longitude)) {
        set_status(state, "Could not save the selected geographic location.", true);
        return;
    }

    memset(&state->location_metadata, 0,
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

    if (result->country_code[0] != '\0' &&
        ss_regional_context_nearest_timezone(
            result->country_code,
            result->latitude,
            result->longitude,
            timezone_id,
            sizeof(timezone_id))) {
        (void)g_strlcpy(
            state->location_metadata.timezone_id,
            timezone_id,
            sizeof(state->location_metadata.timezone_id));
    }

    state->location_metadata_present =
        ss_location_metadata_save(&state->location_metadata);
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

    policy = ss_date_time_model_policy(&state->model);
    if (policy != NULL) {
        update_location_summary(state);
    }
    policy_saved(state);

    if (timezone_id[0] != '\0') {
        request_system_timezone(state, timezone_id);
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
        G_OBJECT(request->window), "system-settings-date-time-panel");
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
        set_status(
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

        title = ss_linux_ui_make_label(item->display_name, "setting-label");
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
    set_status(
        state,
        "Select the matching locality. Its coordinates will become the system geographic location and the nearest IANA time zone will be applied to the operating system.",
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
        set_status(state, "Enter a locality or place name.", true);
        return;
    }

    if (state->location_search_cancellable != NULL) {
        g_cancellable_cancel(state->location_search_cancellable);
        g_clear_object(&state->location_search_cancellable);
    }
    state->location_search_cancellable = g_cancellable_new();
    state->location_search_generation++;

    clear_location_results(state);
    set_status(state, "Searching for matching localities…", false);
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

static void on_location_search_clicked(
    GtkButton *button G_GNUC_UNUSED,
    gpointer user_data)
{
    begin_location_search(user_data);
}

static void on_location_search_activate(
    GtkEntry *entry G_GNUC_UNUSED,
    gpointer user_data)
{
    begin_location_search(user_data);
}

static void on_location_coordinate_changed(
    GObject *object G_GNUC_UNUSED,
    GParamSpec *pspec G_GNUC_UNUSED,
    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    double latitude;
    double longitude;

    if (state == NULL || state->updating_controls) {
        return;
    }

    latitude = gtk_spin_button_get_value(state->latitude);
    longitude = gtk_spin_button_get_value(state->longitude);

    if (!ss_date_time_model_set_location(
            &state->model, true, latitude, longitude)) {
        set_status(state, "Could not save custom geographic coordinates.", true);
        sync_controls(state);
        return;
    }

    memset(&state->location_metadata, 0,
           sizeof(state->location_metadata));
    (void)g_strlcpy(
        state->location_metadata.display_name,
        "Custom coordinates",
        sizeof(state->location_metadata.display_name));
    state->location_metadata.latitude = latitude;
    state->location_metadata.longitude = longitude;
    state->location_metadata_present =
        ss_location_metadata_save(&state->location_metadata);
    state->location_follows_timezone_reference = false;
    gtk_editable_set_text(
        GTK_EDITABLE(state->location_search),
        "Custom coordinates");
    policy_saved(state);
}

static GtkWidget *build_panel(SsLinuxDateTimePanel *state)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkWidget *summary = ss_linux_ui_make_label(
        "System Settings is the authoritative Date & Time frontend. It writes ordinary Mint/Linux settings through their native interfaces and adds richer Common-aware clock, calendar and geographic policy without maintaining a second copy of native system state.",
        "page-summary");
    GtkWidget *preview_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *location_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *clock_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *calendar_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *system_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *format_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *manual_box;
    GtkWidget *search_box;
    GtkStringList *strings;
    GtkExpression *expression;

    gtk_widget_add_css_class(page, "settings-content");
    gtk_box_append(GTK_BOX(page), ss_linux_ui_make_label("Date & Time", "page-title"));
    gtk_label_set_wrap(GTK_LABEL(summary), TRUE);
    gtk_box_append(GTK_BOX(page), summary);

    gtk_widget_add_css_class(preview_card, "preview-card");
    state->clock_preview = ss_linux_ui_make_label("--:--", "preview-time");
    state->date_preview = ss_linux_ui_make_label("", "preview-date");
    gtk_label_set_wrap(GTK_LABEL(state->clock_preview), TRUE);
    gtk_label_set_wrap(GTK_LABEL(state->date_preview), TRUE);
    gtk_box_append(GTK_BOX(preview_card), state->clock_preview);
    gtk_box_append(GTK_BOX(preview_card), state->date_preview);
    gtk_box_append(GTK_BOX(page), preview_card);

    /*
     * Locality, coordinates and time zone are one coherent settings family.
     * A named locality may propose the matching IANA zone, while the explicit
     * zone selector remains visible for correction. If no precise locality has
     * been selected, the system zone's tzdata reference follows zone changes.
     */
    gtk_widget_add_css_class(location_card, "settings-card");
    gtk_widget_add_css_class(location_card, "location-card");
    gtk_box_append(
        GTK_BOX(location_card),
        ss_linux_ui_make_label("Location & time zone", "section-title"));

    state->location_summary = ss_linux_ui_make_label("", "accent-note");
    gtk_label_set_wrap(GTK_LABEL(state->location_summary), TRUE);
    gtk_box_append(GTK_BOX(location_card), state->location_summary);

    search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->location_search = GTK_ENTRY(gtk_entry_new());
    state->location_search_button =
        GTK_BUTTON(gtk_button_new_with_label("Search"));
    gtk_entry_set_placeholder_text(
        state->location_search,
        "Type a locality, for example Mooroopna");
    gtk_widget_set_hexpand(
        GTK_WIDGET(state->location_search), TRUE);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_search), "setting-entry");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_search_button), "setting-button");
    gtk_box_append(
        GTK_BOX(search_box), GTK_WIDGET(state->location_search));
    gtk_box_append(
        GTK_BOX(search_box),
        GTK_WIDGET(state->location_search_button));
    gtk_box_append(
        GTK_BOX(location_card),
        ss_linux_ui_make_setting_row(
            "Locality",
            "Search by town, suburb, city or place name. Selecting a result stores its coordinates and applies the nearest matching IANA time zone to Linux.",
            search_box));

    state->location_results = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_results), "location-results");
    gtk_widget_set_visible(
        GTK_WIDGET(state->location_results), FALSE);
    gtk_box_append(
        GTK_BOX(location_card),
        GTK_WIDGET(state->location_results));

    strings = timezone_strings(state);
    state->timezone = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    expression = gtk_property_expression_new(
        GTK_TYPE_STRING_OBJECT, NULL, "string");
    gtk_drop_down_set_expression(state->timezone, expression);
    gtk_expression_unref(expression);
    gtk_drop_down_set_enable_search(state->timezone, TRUE);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->timezone), "setting-dropdown");
    gtk_widget_set_size_request(
        GTK_WIDGET(state->timezone), 360, -1);
    gtk_box_append(
        GTK_BOX(location_card),
        ss_linux_ui_make_setting_row(
            "Time zone",
            "The real operating-system IANA zone. Locality selection normally chooses it automatically; it remains editable for correction or deliberate overrides.",
            GTK_WIDGET(state->timezone)));

    state->latitude = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(-90.0, 90.0, 0.0001));
    gtk_widget_add_css_class(
        GTK_WIDGET(state->latitude), "setting-spin");
    gtk_spin_button_set_digits(state->latitude, 4U);
    gtk_box_append(
        GTK_BOX(location_card),
        ss_linux_ui_make_setting_row(
            "Latitude",
            "Advanced coordinate override. Degrees north are positive; degrees south are negative.",
            GTK_WIDGET(state->latitude)));

    state->longitude = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(-180.0, 180.0, 0.0001));
    gtk_widget_add_css_class(
        GTK_WIDGET(state->longitude), "setting-spin");
    gtk_spin_button_set_digits(state->longitude, 4U);
    gtk_box_append(
        GTK_BOX(location_card),
        ss_linux_ui_make_setting_row(
            "Longitude",
            "Advanced coordinate override. Degrees east of Greenwich are positive; degrees west are negative.",
            GTK_WIDGET(state->longitude)));
    gtk_box_append(GTK_BOX(page), location_card);

    gtk_widget_add_css_class(clock_card, "settings-card");
    gtk_widget_add_css_class(clock_card, "clock-card");
    gtk_box_append(
        GTK_BOX(clock_card),
        ss_linux_ui_make_label("Clock system", "section-title"));

    strings = clock_mode_strings(state);
    state->clock_mode = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->clock_mode), "setting-dropdown");
    gtk_widget_set_size_request(
        GTK_WIDGET(state->clock_mode), 360, -1);
    gtk_box_append(
        GTK_BOX(clock_card),
        ss_linux_ui_make_setting_row(
            "System clock",
            "Choose the human clock representation used by Common-aware applications. Standard time follows the native desktop's 12/24-hour preference below.",
            GTK_WIDGET(state->clock_mode)));

    state->show_seconds = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->show_seconds), "setting-switch");
    gtk_box_append(
        GTK_BOX(clock_card),
        ss_linux_ui_make_setting_row(
            "Show seconds",
            "Show seconds or the closest finer unit supported by the selected clock system. The equivalent Cinnamon panel preference is kept aligned.",
            GTK_WIDGET(state->show_seconds)));
    gtk_box_append(GTK_BOX(page), clock_card);

    gtk_widget_add_css_class(calendar_card, "settings-card");
    gtk_widget_add_css_class(calendar_card, "calendar-card");
    gtk_box_append(
        GTK_BOX(calendar_card),
        ss_linux_ui_make_label("Calendar system", "section-title"));

    strings = calendar_strings();
    state->calendar = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->calendar), "setting-dropdown");
    gtk_widget_set_size_request(
        GTK_WIDGET(state->calendar), 360, -1);
    gtk_box_append(
        GTK_BOX(calendar_card),
        ss_linux_ui_make_setting_row(
            "Calendar",
            "Choose the calendar system used by Common-aware applications. Gregorian remains the ordinary Mint/Linux calendar.",
            GTK_WIDGET(state->calendar)));
    gtk_box_append(GTK_BOX(page), calendar_card);

    /*
     * Network/manual clock source belongs after the user's presentation
     * choices. When NTP is enabled the manual controls do not merely become
     * insensitive: they disappear because they are not an active source.
     */
    gtk_widget_add_css_class(system_card, "settings-card");
    gtk_widget_add_css_class(system_card, "system-card");
    gtk_box_append(
        GTK_BOX(system_card),
        ss_linux_ui_make_label("System time", "section-title"));

    state->network_time = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->network_time), "setting-switch");
    gtk_box_append(
        GTK_BOX(system_card),
        ss_linux_ui_make_setting_row(
            "Network time",
            "Synchronise the system clock through the operating system's configured network-time service.",
            GTK_WIDGET(state->network_time)));

    manual_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->manual_date = GTK_ENTRY(gtk_entry_new());
    state->manual_time = GTK_ENTRY(gtk_entry_new());
    state->manual_set_time =
        GTK_BUTTON(gtk_button_new_with_label("Set"));
    gtk_entry_set_placeholder_text(
        state->manual_date, "YYYY-MM-DD");
    gtk_entry_set_placeholder_text(
        state->manual_time, "Selected clock time");
    gtk_entry_set_max_length(state->manual_date, 10);
    gtk_entry_set_max_length(state->manual_time, 24);
    gtk_widget_set_size_request(
        GTK_WIDGET(state->manual_date), 130, -1);
    gtk_widget_set_size_request(
        GTK_WIDGET(state->manual_time), 150, -1);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_date), "setting-entry");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_time), "setting-entry");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_set_time), "setting-button");
    gtk_box_append(
        GTK_BOX(manual_box), GTK_WIDGET(state->manual_date));
    gtk_box_append(
        GTK_BOX(manual_box), GTK_WIDGET(state->manual_time));
    gtk_box_append(
        GTK_BOX(manual_box), GTK_WIDGET(state->manual_set_time));

    state->manual_row = ss_linux_ui_make_setting_row(
        "Manual date and time",
        "Shown only when Network time is off. The time entry follows the selected clock system above; the current reversible editor supports Gregorian dates with Standard, 12-hour, 24-hour and French Republican decimal time.",
        manual_box);
    gtk_box_append(GTK_BOX(system_card), state->manual_row);

    state->manual_unavailable = ss_linux_ui_make_label(
        "Manual setting is hidden for this clock/calendar combination because System Settings will not reinterpret a presentation it cannot safely convert back to one canonical system instant.",
        "accent-note");
    gtk_label_set_wrap(
        GTK_LABEL(state->manual_unavailable), TRUE);
    gtk_widget_set_visible(state->manual_unavailable, FALSE);
    gtk_box_append(
        GTK_BOX(system_card), state->manual_unavailable);

    state->status_label = ss_linux_ui_make_label("", "status-ok");
    gtk_label_set_wrap(GTK_LABEL(state->status_label), TRUE);
    gtk_box_append(GTK_BOX(system_card), state->status_label);
    gtk_box_append(GTK_BOX(page), system_card);

    /*
     * These are ordinary Cinnamon authorities, not copies in the Infiltrator
     * policy. Writing them here has exactly the same system effect as Mint's
     * Date & Time format controls.
     */
    gtk_widget_add_css_class(format_card, "settings-card");
    gtk_box_append(
        GTK_BOX(format_card),
        ss_linux_ui_make_label("Desktop format", "section-title"));

    state->show_date = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->show_date), "setting-switch");
    gtk_box_append(
        GTK_BOX(format_card),
        ss_linux_ui_make_setting_row(
            "Display the date",
            "Show the date in Cinnamon's panel clock.",
            GTK_WIDGET(state->show_date)));

    strings = first_day_strings();
    state->first_day = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->first_day), "setting-dropdown");
    gtk_widget_set_size_request(
        GTK_WIDGET(state->first_day), 220, -1);
    gtk_box_append(
        GTK_BOX(format_card),
        ss_linux_ui_make_setting_row(
            "First day of week",
            "Use the locale default, Sunday or Monday for Cinnamon's calendar.",
            GTK_WIDGET(state->first_day)));
    gtk_box_append(GTK_BOX(page), format_card);

    g_signal_connect(
        state->clock_mode, "notify::selected",
        G_CALLBACK(on_clock_changed), state);
    g_signal_connect(
        state->calendar, "notify::selected",
        G_CALLBACK(on_calendar_changed), state);
    g_signal_connect(
        state->show_seconds, "notify::active",
        G_CALLBACK(on_seconds_changed), state);

    g_signal_connect(
        state->timezone, "notify::selected",
        G_CALLBACK(on_timezone_changed), state);
    g_signal_connect(
        state->network_time, "notify::active",
        G_CALLBACK(on_network_time_changed), state);
    g_signal_connect(
        state->manual_set_time, "clicked",
        G_CALLBACK(on_manual_set_time_clicked), state);

    g_signal_connect(
        state->location_search_button, "clicked",
        G_CALLBACK(on_location_search_clicked), state);
    g_signal_connect(
        state->location_search, "activate",
        G_CALLBACK(on_location_search_activate), state);
    g_signal_connect(
        state->location_results, "row-activated",
        G_CALLBACK(on_location_result_activated), state);
    g_signal_connect(
        state->latitude, "notify::value",
        G_CALLBACK(on_location_coordinate_changed), state);
    g_signal_connect(
        state->longitude, "notify::value",
        G_CALLBACK(on_location_coordinate_changed), state);

    g_signal_connect(
        state->show_date, "notify::active",
        G_CALLBACK(on_show_date_changed), state);
    g_signal_connect(
        state->first_day, "notify::selected",
        G_CALLBACK(on_first_day_changed), state);

    return page;
}



static bool migrate_legacy_native_clock(
    SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL) {
        return false;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL ||
        !ss_native_clock_mode_is_legacy_default(policy->clock_mode)) {
        return true;
    }

    return ss_date_time_model_set_clock_mode(
        &state->model,
        ss_native_clock_mode_id(desktop_uses_24h(state)));
}

SsLinuxDateTimePanel *ss_linux_date_time_panel_new(
    GtkWindow *host_window,
    GError **error)
{
    SsLinuxDateTimePanel *state;
    g_autoptr(GError) system_time_error = NULL;
    bool legacy_migration_ok;

    if (host_window == NULL) {
        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_INVALID_ARGUMENT,
            "Date & Time requires a host window.");
        return NULL;
    }

    state = g_new0(SsLinuxDateTimePanel, 1);
    state->window = host_window;

    if (!ss_date_time_model_init(
            &state->model,
            ss_platform_temporal_policy_store())) {
        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_FAILED,
            "Unable to initialise Date & Time settings.");
        g_free(state);
        return NULL;
    }

    state->calendar_preview_provider =
        ss_calendar_preview_provider_new();

    (void)ss_regional_context_detect(&state->regional_context);
    state->location_metadata_present =
        ss_location_metadata_load(&state->location_metadata);
    state->location_follows_timezone_reference =
        !state->location_metadata_present &&
        (!ss_date_time_model_policy(&state->model)->location_configured ||
         location_policy_matches_reference(state));

    state->system_time_service =
        ss_system_time_service_new(&system_time_error);
    state->system_time_cancellable = g_cancellable_new();

    state->cinnamon_interface_settings =
        ss_cinnamon_interface_settings_new();
    if (state->cinnamon_interface_settings != NULL) {
        g_signal_connect(
            state->cinnamon_interface_settings,
            "changed",
            G_CALLBACK(on_cinnamon_interface_changed),
            state);
    }

    legacy_migration_ok = migrate_legacy_native_clock(state);
    state->root = build_panel(state);

    if (state->system_time_service != NULL) {
        ss_system_time_service_set_changed_callback(
            state->system_time_service,
            system_time_changed,
            state);
    }

    sync_controls(state);
    if (!legacy_migration_ok) {
        set_status(
            state,
            "The legacy native-default clock policy could not be migrated to an explicit 12-hour or 24-hour clock selection.",
            true);
    } else if (state->system_time_service == NULL) {
        set_status(
            state,
            system_time_error != NULL
                ? system_time_error->message
                : "The operating-system date/time service is unavailable.",
            true);
    } else {
        set_status(
            state,
            state->model.persisted_policy_present
                ? "Using the saved system-wide temporal policy. Native Mint/Linux date and time controls are live."
                : "Using Mint/Cinnamon temporal preferences as the initial policy. Native Mint/Linux date and time controls are live.",
            false);
    }

    state->timer_id =
        g_timeout_add_seconds(1U, refresh_preview, state);
    return state;
}

GtkWidget *ss_linux_date_time_panel_widget(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? panel->root : NULL;
}

void ss_linux_date_time_panel_free(gpointer data)
{
    SsLinuxDateTimePanel *state = data;

    if (state == NULL) {
        return;
    }

    if (state->timer_id != 0U) {
        g_source_remove(state->timer_id);
        state->timer_id = 0U;
    }
    if (state->location_search_cancellable != NULL) {
        g_cancellable_cancel(state->location_search_cancellable);
        g_clear_object(&state->location_search_cancellable);
    }
    if (state->system_time_cancellable != NULL) {
        g_cancellable_cancel(state->system_time_cancellable);
        g_clear_object(&state->system_time_cancellable);
    }

    g_clear_pointer(&state->clock_mode_ids, g_ptr_array_unref);
    g_clear_pointer(&state->timezone_ids, g_ptr_array_unref);
    ss_calendar_preview_provider_free(
        state->calendar_preview_provider);
    state->calendar_preview_provider = NULL;
    ss_system_time_service_free(state->system_time_service);
    state->system_time_service = NULL;
    g_clear_object(&state->cinnamon_interface_settings);
    g_free(state);
}
