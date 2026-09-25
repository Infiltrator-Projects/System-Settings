// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-panel.c
 * @brief Built-in Linux Date & Time settings module.
 *
 * Domain state, native backend reconciliation and asynchronous operations
 * live here. GTK construction is isolated in linux-date-time-panel-ui.c.
 * The GTK-facing header is private to the current built-in integration and is
 * not the future public module ABI.
 */

#include "linux-date-time-panel-private.h"
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

typedef struct LocationSearchUiRequest {
    GtkWindow *window;
    guint generation;
} LocationSearchUiRequest;

typedef struct SystemTimeUiRequest {
    GtkWindow *window;
    guint generation;
} SystemTimeUiRequest;

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
    unix_microseconds = g_date_time_to_unix(now) * G_USEC_PER_SEC +
        g_date_time_get_microsecond(now);
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
        if (zone_index == GTK_INVALID_LIST_POSITION) {
            GtkStringList *strings = GTK_STRING_LIST(gtk_drop_down_get_model(state->timezone));
            g_ptr_array_add(state->timezone_ids, g_strdup(system_state.timezone));
            gtk_string_list_append(strings, system_state.timezone);
            zone_index = state->timezone_ids->len - 1U;
        }
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
    const char *effective_mode;
    int64_t unix_us;
    gint64 offset_us;
    double latitude = 0.0;
    double longitude = 0.0;
    bool has_location;

    if (policy == NULL || now == NULL || buffer == NULL || capacity == 0U) {
        return false;
    }

    /*
     * Common is the system-wide clock renderer. The legacy "standard" value
     * deliberately remains platform-owned, so resolve it to Cinnamon's current
     * conventional choice before entering the portable formatter. Every
     * explicit mode then uses the same implementation that other Infiltrator
     * applications consume.
     */
    effective_mode = policy->clock_mode;
    if (strcmp(effective_mode, "standard") == 0) {
        effective_mode = ss_native_clock_mode_id(
            desktop_uses_24h(state));
    }

    mode = infiltratr_temporal_clock_mode_find(effective_mode);
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

    unix_us = g_date_time_to_unix(now) * G_USEC_PER_SEC +
        g_date_time_get_microsecond(now);
    offset_us = g_date_time_get_utc_offset(now);
    return infiltratr_temporal_format_clock_mode(
        effective_mode,
        unix_us,
        (int32_t)(offset_us / G_USEC_PER_SEC),
        policy->show_seconds,
        false,
        has_location,
        latitude,
        longitude,
        buffer,
        capacity,
        NULL);
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

static gboolean refresh_preview_once(gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;

    if (state != NULL) {
        state->preview_idle_id = 0U;
        (void)refresh_preview(state);
    }
    return G_SOURCE_REMOVE;
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

    if (state == NULL || key == NULL || state->updating_controls || state->model.saving) {
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

void on_clock_changed(GObject *object,
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

void on_calendar_changed(GObject *object,
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

void on_seconds_changed(GObject *object,
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

    if (!system_state->available) {
        sync_system_time_controls(state);
        set_status(state, "The operating-system date/time service is unavailable.", true);
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
        const InfiltratrTemporalPolicyV3 *policy = ss_date_time_model_policy(&state->model);
        if (!policy->location_configured || ss_date_time_model_set_location(
                &state->model, true,
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

static void system_time_ui_request_free(SystemTimeUiRequest *request)
{
    if (request == NULL) {
        return;
    }
    g_clear_object(&request->window);
    g_free(request);
}

static SystemTimeUiRequest *begin_system_time_operation(
    SsLinuxDateTimePanel *state)
{
    SystemTimeUiRequest *request;

    if (state == NULL) {
        return NULL;
    }

    if (state->system_time_cancellable != NULL) {
        g_cancellable_cancel(state->system_time_cancellable);
        g_clear_object(&state->system_time_cancellable);
    }
    state->system_time_cancellable = g_cancellable_new();
    state->system_time_generation++;

    request = g_new0(SystemTimeUiRequest, 1);
    request->window = g_object_ref(state->window);
    request->generation = state->system_time_generation;
    return request;
}

static void system_time_operation_complete(
    SsSystemTimeService *service G_GNUC_UNUSED,
    bool success,
    const char *error_message,
    gpointer user_data)
{
    SystemTimeUiRequest *request = user_data;
    SsLinuxDateTimePanel *state = NULL;

    if (request != NULL && request->window != NULL) {
        state = g_object_get_data(
            G_OBJECT(request->window),
            "system-settings-date-time-panel");
    }

    /*
     * A later timezone/NTP/manual-time request invalidates every older
     * completion. Cancellation alone is insufficient because D-Bus replies
     * can race with the cancellation notification.
     */
    if (state != NULL &&
        request->generation == state->system_time_generation) {
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

    system_time_ui_request_free(request);
}

static void request_system_timezone(
    SsLinuxDateTimePanel *state,
    const char *timezone_id)
{
    if (state == NULL || state->system_time_service == NULL ||
        timezone_id == NULL || timezone_id[0] == '\0') {
        return;
    }

    SystemTimeUiRequest *request =
        begin_system_time_operation(state);

    if (request == NULL) {
        return;
    }
    set_status(state, "Updating the operating-system time zone…", false);
    ss_system_time_service_set_timezone_async(
        state->system_time_service,
        timezone_id,
        state->system_time_cancellable,
        system_time_operation_complete,
        request);
}

void on_timezone_changed(GObject *object G_GNUC_UNUSED,
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

void on_network_time_changed(GObject *object,
                                    GParamSpec *pspec G_GNUC_UNUSED,
                                    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    bool enabled;

    if (state == NULL || state->updating_system_controls ||
        state->system_time_service == NULL) {
        return;
    }

    SystemTimeUiRequest *request;

    enabled = gtk_switch_get_active(GTK_SWITCH(object)) != FALSE;
    request = begin_system_time_operation(state);
    if (request == NULL) {
        return;
    }
    set_status(
        state,
        enabled ? "Enabling network time…" : "Disabling network time…",
        false);
    ss_system_time_service_set_ntp_async(
        state->system_time_service,
        enabled,
        state->system_time_cancellable,
        system_time_operation_complete,
        request);
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
        strlen(date_text) != 10U || date_text[4] != '-' || date_text[7] != '-' ||
        strspn(date_text, "0123456789-") != 10U ||
        sscanf(date_text,
               "%4d-%2d-%2d%c",
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
    /* GLib normalises nonexistent DST wall times. A protected clock write
     * must reject that adjustment instead of setting a different time. Folds
     * follow GLib's documented standard-time choice. */
    if (value == NULL || g_date_time_get_year(value) != year ||
        g_date_time_get_month(value) != month || g_date_time_get_day_of_month(value) != day ||
        g_date_time_get_hour(value) != hour || g_date_time_get_minute(value) != minute ||
        g_date_time_get_second(value) != (int)(whole_seconds % 60)) {
        return false;
    }
    *unix_time_usec =
        (int64_t)g_date_time_to_unix(value) * G_USEC_PER_SEC +
        (microseconds_of_day % G_USEC_PER_SEC);
    return true;
}

void on_manual_set_time_clicked(
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

    {
        SystemTimeUiRequest *request =
            begin_system_time_operation(state);

        if (request == NULL) {
            return;
        }
        set_status(
            state,
            "Setting the operating-system date and time…",
            false);
        ss_system_time_service_set_time_async(
            state->system_time_service,
            unix_time_usec,
            state->system_time_cancellable,
            system_time_operation_complete,
            request);
    }
}

void on_show_date_changed(GObject *object,
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

void on_first_day_changed(GObject *object G_GNUC_UNUSED,
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

void on_location_result_activated(
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

void on_location_coordinate_changed(
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


static void system_time_service_ready(
    SsSystemTimeService *service,
    const char *error_message,
    gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);
    SsLinuxDateTimePanel *state = g_object_get_data(
        G_OBJECT(window), "system-settings-date-time-panel");

    if (state == NULL) {
        ss_system_time_service_free(service);
        g_object_unref(window);
        return;
    }

    if (service == NULL) {
        set_status(
            state,
            error_message != NULL
                ? error_message
                : "The operating-system date/time service is unavailable.",
            true);
        g_object_unref(window);
        return;
    }

    state->system_time_service = service;
    ss_system_time_service_set_changed_callback(
        state->system_time_service,
        system_time_changed,
        state);
    sync_system_time_controls(state);
    SsSystemTimeState initial;
    if (!ss_system_time_service_read(service, &initial)) {
        set_status(state, "The operating-system date/time service is unavailable.", true);
        g_object_unref(window);
        return;
    }
    set_status(
        state,
        state->model.persisted_policy_present
            ? "Using the saved system-wide temporal policy. Native Mint/Linux date and time controls are live."
            : "Using Mint/Cinnamon temporal preferences as the initial policy. Native Mint/Linux date and time controls are live.",
        false);
    g_object_unref(window);
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

    if (!state->model.persisted_policy_present) {
        infiltratr_copy_string(state->model.policy.clock_mode,
            sizeof(state->model.policy.clock_mode),
            ss_native_clock_mode_id(desktop_uses_24h(state)));
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
    state->root = g_object_ref_sink(ss_linux_date_time_panel_build_ui(state));

    gtk_widget_set_sensitive(GTK_WIDGET(state->show_date),
        state->cinnamon_interface_settings != NULL &&
        ss_cinnamon_interface_has_key("clock-show-date"));
    gtk_widget_set_sensitive(GTK_WIDGET(state->first_day),
        state->cinnamon_interface_settings != NULL &&
        ss_cinnamon_interface_has_key("first-day-of-week"));
    sync_controls(state);
    if (!legacy_migration_ok) {
        set_status(
            state,
            "The legacy native-default clock policy could not be migrated to an explicit 12-hour or 24-hour clock selection.",
            true);
    } else {
        set_status(
            state,
            "Connecting to the operating-system date/time service…",
            false);
    }

    /*
     * Proxy construction and Calendar runtime discovery are deliberately
     * deferred until after construction, allowing the host to present the
     * window before either external-service work path runs.
     */
    ss_system_time_service_new_async(
        state->system_time_cancellable,
        system_time_service_ready,
        g_object_ref(state->window));
    state->preview_idle_id =
        g_idle_add(refresh_preview_once, state);
    state->timer_id =
        g_timeout_add_seconds(1U, refresh_preview, state);
    return state;
}

GtkWidget *ss_linux_date_time_panel_widget(
    SsLinuxDateTimePanel *panel)
{
    return panel != NULL ? panel->root : NULL;
}

/* Detach every module signal before releasing widgets retained by the host. */
static void disconnect_panel_widgets(GtkWidget *widget, gpointer state)
{
    for (GtkWidget *child = gtk_widget_get_first_child(widget); child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        disconnect_panel_widgets(child, state);
    }
    g_signal_handlers_disconnect_by_data(widget, state);
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
    if (state->preview_idle_id != 0U) {
        g_source_remove(state->preview_idle_id);
        state->preview_idle_id = 0U;
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
    if (state->root != NULL) {
        disconnect_panel_widgets(state->root, state);
        g_clear_object(&state->root);
    }
    g_free(state);
}
