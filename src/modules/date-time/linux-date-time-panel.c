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

typedef enum SsSystemTimeOperation {
    SS_SYSTEM_TIME_OPERATION_TIMEZONE,
    SS_SYSTEM_TIME_OPERATION_NTP,
    SS_SYSTEM_TIME_OPERATION_MANUAL
} SsSystemTimeOperation;

typedef struct SystemTimeUiRequest {
    GtkWindow *window;
    guint generation;
    SsSystemTimeOperation operation;
} SystemTimeUiRequest;

static void sync_controls(SsLinuxDateTimePanel *state);
static gboolean refresh_preview(gpointer user_data);
static void restart_preview_timer(SsLinuxDateTimePanel *state);
static void schedule_locality_recovery_retry(
    SsLinuxDateTimePanel *state);

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

void ss_linux_date_time_panel_set_status(
    SsLinuxDateTimePanel *state,
    const char *message,
    bool error)
{
    set_status(state, message, error);
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

static gchar *temporal_policy_path(void)
{
    return g_build_filename(
        g_get_user_config_dir(),
        "infiltrator",
        "presentation.conf",
        NULL);
}

static bool temporal_policy_equal(
    const InfiltratrTemporalPolicyV3 *left,
    const InfiltratrTemporalPolicyV3 *right)
{
    return left != NULL && right != NULL &&
        g_strcmp0(left->clock_mode, right->clock_mode) == 0 &&
        g_strcmp0(left->calendar, right->calendar) == 0 &&
        left->show_seconds == right->show_seconds &&
        left->location_configured == right->location_configured &&
        ss_linux_date_time_location_coordinate_close(
            left->latitude, right->latitude) &&
        ss_linux_date_time_location_coordinate_close(
            left->longitude, right->longitude);
}

static void refresh_location_authority_state(
    SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy;
    const bool metadata_matches =
        state != NULL &&
        ss_linux_date_time_location_metadata_matches_policy(
            state, ss_date_time_model_policy(&state->model));

    if (state == NULL) {
        return;
    }
    policy = ss_date_time_model_policy(&state->model);
    if (state->location_metadata_uncertain) {
        /*
         * An unreadable locality file is not evidence that the user has no
         * explicit locality. Never replace uncertain authority with a tzdata
         * reference approximation.
         */
        state->location_follows_timezone_reference = false;
        return;
    }
    state->location_follows_timezone_reference =
        !metadata_matches &&
        (policy == NULL ||
         !policy->location_configured ||
         ss_linux_date_time_location_policy_matches_reference(state));
}

static void location_metadata_file_changed(gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    SsLocationMetadata loaded = {0};
    SsLocationMetadataLoadStatus status;

    if (state == NULL) {
        return;
    }

    status = ss_location_metadata_load_status(&loaded);
    if (status == SS_LOCATION_METADATA_LOAD_OK) {
        state->location_metadata = loaded;
        state->location_metadata_present = true;
        state->location_metadata_uncertain = false;
    } else if (status == SS_LOCATION_METADATA_LOAD_MISSING) {
        memset(&state->location_metadata, 0, sizeof(state->location_metadata));
        state->location_metadata_present = false;
        state->location_metadata_uncertain = false;
    } else {
        state->location_metadata_uncertain = true;
        set_status(
            state,
            "Locality metadata changed externally but could not be read; the last known-good locality remains active.",
            true);
    }

    refresh_location_authority_state(state);
    sync_controls(state);
    ss_linux_date_time_location_update_summary(state);
}

static gboolean retry_locality_recovery(gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL) {
        return G_SOURCE_REMOVE;
    }

    state->locality_recovery_retry_id = 0U;
    policy = ss_date_time_model_policy(&state->model);
    if (policy != NULL &&
        ss_location_metadata_recover(
            policy->location_configured,
            policy->latitude,
            policy->longitude)) {
        state->locality_recovery_failed = false;
        state->locality_recovery_retry_seconds = 2U;
        location_metadata_file_changed(state);
        set_status(
            state,
            "Interrupted locality metadata was recovered.",
            false);
        return G_SOURCE_REMOVE;
    }

    state->locality_recovery_failed = true;
    schedule_locality_recovery_retry(state);
    return G_SOURCE_REMOVE;
}

static void schedule_locality_recovery_retry(
    SsLinuxDateTimePanel *state)
{
    if (state == NULL ||
        state->locality_recovery_retry_id != 0U ||
        !state->locality_recovery_failed) {
        return;
    }

    if (state->locality_recovery_retry_seconds == 0U)
        state->locality_recovery_retry_seconds = 2U;
    state->locality_recovery_retry_id = g_timeout_add_seconds(
        state->locality_recovery_retry_seconds,
        retry_locality_recovery,
        state);
    state->locality_recovery_retry_seconds =
        MIN(state->locality_recovery_retry_seconds * 2U, 60U);
    g_source_set_name_by_id(
        state->locality_recovery_retry_id,
        "[system-settings] locality recovery retry");
}

static void policy_file_changed(gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    InfiltratrTemporalPolicyV3 previous;

    if (state == NULL) {
        return;
    }

    previous = state->model.policy;
    ss_linux_date_time_location_cancel_coordinate_commit(state);
    if (ss_date_time_model_reload(&state->model)) {
        const InfiltratrTemporalPolicyV3 *policy =
            ss_date_time_model_policy(&state->model);
        const bool locality_recovery_ok =
            policy != NULL &&
            ss_location_metadata_recover(
                policy->location_configured,
                policy->latitude,
                policy->longitude);
        SsLocationMetadata recovered_metadata = {0};
        const SsLocationMetadataLoadStatus recovered_metadata_status =
            locality_recovery_ok
                ? ss_location_metadata_load_status(&recovered_metadata)
                : SS_LOCATION_METADATA_LOAD_IO_ERROR;
        const bool recovered_metadata_present =
            recovered_metadata_status == SS_LOCATION_METADATA_LOAD_OK;
        state->locality_recovery_failed = !locality_recovery_ok;
        if (locality_recovery_ok) {
            if (state->locality_recovery_retry_id != 0U) {
                g_source_remove(state->locality_recovery_retry_id);
                state->locality_recovery_retry_id = 0U;
            }
        } else {
            schedule_locality_recovery_retry(state);
        }
        const bool changed =
            !temporal_policy_equal(&previous, &state->model.policy);
        const bool manual_context_changed =
            g_strcmp0(
                previous.clock_mode,
                state->model.policy.clock_mode) != 0 ||
            g_strcmp0(
                previous.calendar,
                state->model.policy.calendar) != 0;

        /*
         * Preserve a user's typed wall-time draft across unrelated external
         * changes. Only a changed clock/calendar representation invalidates
         * how that draft should be interpreted.
         */
        if (manual_context_changed) {
            state->manual_dirty = false;
        }
        if (recovered_metadata_present) {
            state->location_metadata = recovered_metadata;
            state->location_metadata_present = true;
            state->location_metadata_uncertain = false;
        } else if (locality_recovery_ok &&
                   recovered_metadata_status ==
                       SS_LOCATION_METADATA_LOAD_MISSING) {
            memset(&state->location_metadata, 0, sizeof(state->location_metadata));
            state->location_metadata_present = false;
            state->location_metadata_uncertain = false;
        } else if (locality_recovery_ok) {
            state->location_metadata_uncertain = true;
        }
        refresh_location_authority_state(state);
        sync_controls(state);
        restart_preview_timer(state);
        (void)refresh_preview(state);
        if (!locality_recovery_ok) {
            set_status(
                state,
                "Temporal policy changed externally, but interrupted locality recovery could not acquire or complete its transaction lock.",
                true);
        } else if (changed) {
            set_status(
                state,
                "Temporal policy changed externally and was reloaded.",
                false);
        }
    } else {
        set_status(
            state,
            "The temporal policy changed externally but could not be reloaded; the last committed in-memory policy is still active.",
            true);
    }
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

static void update_overview_policy(SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy;
    const InfiltratrTemporalClockModeInfo *clock;
    const InfiltratrTemporalCalendarInfo *calendar;

    if (state == NULL) {
        return;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL) {
        return;
    }

    clock = infiltratr_temporal_clock_mode_find(policy->clock_mode);
    if (state->overview_clock != NULL) {
        gtk_label_set_text(
            GTK_LABEL(state->overview_clock),
            clock != NULL ? clock->name : policy->clock_mode);
    }

    calendar = infiltratr_temporal_calendar_find(policy->calendar);
    if (state->overview_calendar != NULL) {
        gtk_label_set_text(
            GTK_LABEL(state->overview_calendar),
            calendar != NULL ? calendar->name : policy->calendar);
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
        if (state->overview_timezone != NULL) {
            gtk_label_set_text(GTK_LABEL(state->overview_timezone), "Unavailable");
        }
        if (state->overview_sync != NULL) {
            gtk_label_set_text(GTK_LABEL(state->overview_sync), "Unavailable");
        }
        return;
    }

    state->updating_system_controls = true;

    if (state->overview_timezone != NULL) {
        gtk_label_set_text(
            GTK_LABEL(state->overview_timezone),
            system_state.timezone[0] != '\0'
                ? system_state.timezone
                : "Unknown");
    }
    if (state->overview_sync != NULL) {
        gtk_label_set_text(
            GTK_LABEL(state->overview_sync),
            system_state.ntp_enabled ? "Automatic" : "Manual");
    }

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

    const bool protected_operation_active =
        state->timezone_cancellable != NULL ||
        state->ntp_cancellable != NULL ||
        state->manual_time_cancellable != NULL;

    if (!manual_visible) {
        /*
         * A hidden editor must never retain text authored under a different
         * clock/calendar/NTP context. Re-seed it when manual mode becomes
         * meaningful again.
         */
        state->manual_dirty = false;
    } else {
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->manual_date), !protected_operation_active);
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->manual_time), !protected_operation_active);
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->manual_set_time), !protected_operation_active);
        if (!state->manual_dirty) {
            fill_manual_time_entries(state);
        }
    }

    if (state->timezone != NULL) {
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->timezone), !protected_operation_active);
    }
    if (state->network_time != NULL) {
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->network_time),
            system_state.can_ntp && !protected_operation_active);
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

    ss_linux_date_time_location_update_summary(state);
    update_overview_policy(state);
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
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    char clock_text[160];

    if (state == NULL || state->clock_preview == NULL || now == NULL) {
        return G_SOURCE_CONTINUE;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL) {
        return G_SOURCE_CONTINUE;
    }

    if (format_preview(state, clock_text, sizeof(clock_text)) &&
        g_strcmp0(
            gtk_label_get_text(GTK_LABEL(state->clock_preview)),
            clock_text) != 0) {
        gtk_label_set_text(GTK_LABEL(state->clock_preview), clock_text);
    }

    /*
     * Extended clocks can legitimately require a 250 ms timer, but the
     * calendar and overview policy do not. Older iterations rebuilt the same
     * date, called into Calendar and rewrote static labels on every clock tick.
     * Cache successful date formatting by civil day/calendar and leave overview
     * policy refreshes on their existing policy/settings change paths.
     */
    const gint year = g_date_time_get_year(now);
    const gint month = g_date_time_get_month(now);
    const gint day = g_date_time_get_day_of_month(now);
    g_autofree gchar *date_key = g_strdup_printf(
        "%04d-%02d-%02d|%s",
        year, month, day, policy->calendar);
    const char *cached_key = g_object_get_data(
        G_OBJECT(state->date_preview),
        "system-settings-preview-date-key");

    if (g_strcmp0(cached_key, date_key) != 0) {
        const InfiltratrTemporalCalendarInfo *calendar =
            infiltratr_temporal_calendar_find(policy->calendar);
        g_autofree gchar *gregorian =
            g_date_time_format(now, "%A, %e %B %Y");
        g_autofree gchar *selected_date = NULL;
        g_autofree gchar *summary = NULL;
        bool cacheable = false;

        if (calendar != NULL) {
            if (strcmp(policy->calendar, "gregorian") == 0) {
                summary = g_strdup_printf(
                    "%s • %s",
                    calendar->name,
                    gregorian != NULL ? gregorian : "");
                cacheable = true;
            } else {
                selected_date =
                    ss_calendar_preview_provider_format_date(
                        ensure_calendar_preview_provider(state),
                        policy->calendar,
                        year,
                        month,
                        day);
                if (selected_date != NULL) {
                    summary = g_strdup_printf(
                        "%s • %s",
                        calendar->name,
                        selected_date);
                    cacheable = true;
                } else {
                    summary = g_strdup_printf(
                        "%s • preview unavailable",
                        calendar->name);
                }
            }
        }

        if (summary != NULL &&
            g_strcmp0(
                gtk_label_get_text(GTK_LABEL(state->date_preview)),
                summary) != 0) {
            gtk_label_set_text(GTK_LABEL(state->date_preview), summary);
        }
        if (cacheable) {
            g_object_set_data_full(
                G_OBJECT(state->date_preview),
                "system-settings-preview-date-key",
                g_strdup(date_key),
                g_free);
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

static void stop_preview_timer(SsLinuxDateTimePanel *state)
{
    if (state != NULL && state->timer_id != 0U) {
        g_source_remove(state->timer_id);
        state->timer_id = 0U;
    }
}

guint ss_linux_date_time_panel_preview_interval_ms(
    const SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy =
        state != NULL
            ? ss_date_time_model_policy(&state->model)
            : NULL;

    if (policy == NULL || !policy->show_seconds) {
        return 1000U;
    }
    if (g_strcmp0(policy->clock_mode, "standard") == 0 ||
        g_strcmp0(policy->clock_mode, "standard-12") == 0 ||
        g_strcmp0(policy->clock_mode, "standard-24") == 0) {
        return 1000U;
    }
    /*
     * Extended clocks may advance displayed units faster than one SI second.
     * Until Common exposes a per-mode cadence capability, 250 ms remains the
     * conservative sampling interval for those modes only.
     */
    return 250U;
}

static void restart_preview_timer(SsLinuxDateTimePanel *state)
{
    if (state == NULL || state->root == NULL ||
        !gtk_widget_get_mapped(state->root)) {
        return;
    }
    stop_preview_timer(state);
    state->timer_id = g_timeout_add(
        ss_linux_date_time_panel_preview_interval_ms(state),
        refresh_preview,
        state);
    g_source_set_name_by_id(
        state->timer_id,
        "[system-settings] visible Date & Time preview");
}

static void on_panel_mapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;

    if (state == NULL) {
        return;
    }
    (void)refresh_preview(state);
    if (state->timer_id == 0U) {
        state->timer_id = g_timeout_add(
            ss_linux_date_time_panel_preview_interval_ms(state),
            refresh_preview,
            state);
        g_source_set_name_by_id(
            state->timer_id,
            "[system-settings] visible Date & Time preview");
    }
}

static void on_panel_unmapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    stop_preview_timer(user_data);
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
            ss_linux_date_time_location_metadata_matches_policy(state, policy)) {
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

void ss_linux_date_time_panel_sync_controls(
    SsLinuxDateTimePanel *state)
{
    sync_controls(state);
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
    bool reconciliation_attempted = false;

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
            reconciliation_attempted = true;
            changed = ss_date_time_model_set_clock_mode(
                &state->model, desired);
        }
    } else if (g_strcmp0(key, "clock-show-seconds") == 0 &&
               ss_cinnamon_interface_get_boolean(
                   state->cinnamon_interface_settings,
                   "clock-show-seconds",
                   &native_value) &&
               policy->show_seconds != native_value) {
        reconciliation_attempted = true;
        changed = ss_date_time_model_set_show_seconds(
            &state->model, native_value);
    }

    if (reconciliation_attempted && !changed) {
        set_status(
            state,
            "A Mint/Cinnamon time preference changed, but System Settings could not persist the matching temporal policy.",
            true);
        sync_controls(state);
        return;
    }

    if (changed) {
        sync_controls(state);
        restart_preview_timer(state);
        set_status(
            state,
            "An external Mint/Cinnamon time preference changed and System Settings reconciled the matching setting.",
            false);
        return;
    }

    (void)refresh_preview(state);
}

static bool native_compatibility_matches(
    const SsLinuxDateTimePanel *state)
{
    const SsTemporalPolicyStore *store =
        ss_platform_temporal_policy_store();
    const InfiltratrTemporalPolicyV3 *policy =
        state != NULL
            ? ss_date_time_model_policy(&state->model)
            : NULL;

    if (policy == NULL) {
        return false;
    }
    return store == NULL ||
           store->compatibility_matches == NULL ||
           store->compatibility_matches(policy);
}

static void policy_saved(SsLinuxDateTimePanel *state)
{
    const bool compatibility_ok =
        native_compatibility_matches(state);

    set_status(
        state,
        compatibility_ok
            ? "System temporal policy saved. Calendar and other Common-aware applications use this setting."
            : "System temporal policy saved, but Mint/Cinnamon compatibility settings did not fully synchronize.",
        !compatibility_ok);
    update_control_capabilities(state);
    restart_preview_timer(state);
    (void)refresh_preview(state);
}

void ss_linux_date_time_panel_policy_saved(
    SsLinuxDateTimePanel *state)
{
    policy_saved(state);
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
    state->manual_dirty = false;
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
    state->manual_dirty = false;
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

    const bool timezone_changed =
        system_state->timezone[0] != '\0' &&
        g_strcmp0(
            state->regional_context.timezone_id,
            system_state->timezone) != 0;
    (void)ss_regional_context_detect(&state->regional_context);

    if (timezone_changed || system_state->ntp_enabled) {
        /*
         * Typed wall time is meaningful only in the context in which it was
         * entered. A time-zone or NTP authority change invalidates that draft.
         */
        state->manual_dirty = false;
    }

    /*
     * Time-zone reconciliation and automatic reference-coordinate publication
     * are locality writers too. Serialize them with explicit locality changes
     * and reload both policy and metadata after taking the locality lock, so a
     * second System Settings process cannot overwrite a freshly selected place
     * with stale metadata or stale time-zone reference coordinates.
     */
    if ((timezone_changed || state->location_follows_timezone_reference) &&
        ss_location_metadata_transaction_begin()) {
        SsLocationMetadata authoritative_metadata = {0};
        SsSystemTimeState locked_system = {0};
        const bool locked_system_ok =
            state->system_time_service != NULL &&
            ss_system_time_service_read(
                state->system_time_service, &locked_system);
        const bool regional_reloaded =
            ss_regional_context_detect(&state->regional_context);
        const bool regional_matches_system =
            locked_system_ok &&
            regional_reloaded &&
            locked_system.timezone[0] != '\0' &&
            g_strcmp0(
                state->regional_context.timezone_id,
                locked_system.timezone) == 0;
        const bool policy_reloaded =
            ss_date_time_model_reload(&state->model);
        const SsLocationMetadataLoadStatus metadata_status =
            ss_location_metadata_load_status(&authoritative_metadata);

        if (metadata_status == SS_LOCATION_METADATA_LOAD_OK) {
            state->location_metadata = authoritative_metadata;
            state->location_metadata_present = true;
        } else if (metadata_status == SS_LOCATION_METADATA_LOAD_MISSING) {
            memset(&state->location_metadata, 0, sizeof(state->location_metadata));
            state->location_metadata_present = false;
        }
        if (policy_reloaded) {
            refresh_location_authority_state(state);
        }

        if (policy_reloaded &&
            ss_linux_date_time_location_metadata_matches_policy(
                state, ss_date_time_model_policy(&state->model)) &&
            locked_system_ok &&
            locked_system.timezone[0] != '\0' &&
            g_strcmp0(
                state->location_metadata.timezone_id,
                locked_system.timezone) != 0) {
            SsLocationMetadata candidate = state->location_metadata;
            (void)g_strlcpy(
                candidate.timezone_id,
                locked_system.timezone,
                sizeof(candidate.timezone_id));
            if (ss_location_metadata_save(&candidate)) {
                state->location_metadata = candidate;
                set_status(
                    state,
                    "Operating-system time zone and locality metadata reconciled.",
                    false);
            } else {
                set_status(
                    state,
                    "The operating-system time zone changed, but the locality metadata could not be updated on disk.",
                    true);
            }
        }

        if (policy_reloaded &&
            state->location_follows_timezone_reference &&
            regional_matches_system &&
            state->regional_context.has_reference_coordinates) {
            const InfiltratrTemporalPolicyV3 *policy =
                ss_date_time_model_policy(&state->model);
            const bool already_matches =
                policy != NULL &&
                policy->location_configured &&
                ss_linux_date_time_location_coordinate_close(
                    policy->latitude,
                    state->regional_context.reference_latitude) &&
                ss_linux_date_time_location_coordinate_close(
                    policy->longitude,
                    state->regional_context.reference_longitude);

            if (!already_matches &&
                !ss_date_time_model_set_location(
                    &state->model,
                    true,
                    state->regional_context.reference_latitude,
                    state->regional_context.reference_longitude)) {
                set_status(
                    state,
                    "The system time zone changed, but its reference coordinates could not be persisted to the temporal policy.",
                    true);
            } else {
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

        ss_location_metadata_transaction_end();
    } else if (timezone_changed ||
               state->location_follows_timezone_reference) {
        set_status(
            state,
            "The system time changed, but the locality transaction lock could not be acquired.",
            true);
    }

    sync_system_time_controls(state);
    ss_linux_date_time_location_update_summary(state);
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

static guint *operation_generation(
    SsLinuxDateTimePanel *state,
    SsSystemTimeOperation operation)
{
    if (state == NULL) {
        return NULL;
    }
    switch (operation) {
    case SS_SYSTEM_TIME_OPERATION_TIMEZONE:
        return &state->timezone_generation;
    case SS_SYSTEM_TIME_OPERATION_NTP:
        return &state->ntp_generation;
    case SS_SYSTEM_TIME_OPERATION_MANUAL:
        return &state->manual_time_generation;
    default:
        return NULL;
    }
}

static GCancellable **operation_cancellable(
    SsLinuxDateTimePanel *state,
    SsSystemTimeOperation operation)
{
    if (state == NULL) {
        return NULL;
    }
    switch (operation) {
    case SS_SYSTEM_TIME_OPERATION_TIMEZONE:
        return &state->timezone_cancellable;
    case SS_SYSTEM_TIME_OPERATION_NTP:
        return &state->ntp_cancellable;
    case SS_SYSTEM_TIME_OPERATION_MANUAL:
        return &state->manual_time_cancellable;
    default:
        return NULL;
    }
}

static SystemTimeUiRequest *begin_system_time_operation(
    SsLinuxDateTimePanel *state,
    SsSystemTimeOperation operation)
{
    SystemTimeUiRequest *request;
    GCancellable **cancellable = operation_cancellable(state, operation);
    guint *generation = operation_generation(state, operation);

    if (state == NULL || cancellable == NULL || generation == NULL) {
        return NULL;
    }

    /*
     * Time-zone, NTP and manual-clock changes are separate D-Bus operations,
     * but their user-visible semantics overlap. Do not compute/apply a wall
     * time while a zone or NTP transition is unresolved, and vice versa.
     * A repeated request of the same kind may still supersede its predecessor.
     */
    if ((operation != SS_SYSTEM_TIME_OPERATION_TIMEZONE &&
         state->timezone_cancellable != NULL) ||
        (operation != SS_SYSTEM_TIME_OPERATION_NTP &&
         state->ntp_cancellable != NULL) ||
        (operation != SS_SYSTEM_TIME_OPERATION_MANUAL &&
         state->manual_time_cancellable != NULL)) {
        return NULL;
    }

    if (*cancellable != NULL) {
        g_cancellable_cancel(*cancellable);
        g_clear_object(cancellable);
    }
    *cancellable = g_cancellable_new();
    ++(*generation);

    request = g_new0(SystemTimeUiRequest, 1);
    request->window = g_object_ref(state->window);
    request->generation = *generation;
    request->operation = operation;
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
    guint *generation = state != NULL
        ? operation_generation(state, request->operation)
        : NULL;

    if (state != NULL && generation != NULL &&
        request->generation == *generation) {
        GCancellable **cancellable =
            operation_cancellable(state, request->operation);
        if (cancellable != NULL) {
            g_clear_object(cancellable);
        }
        if (success) {
            bool metadata_out_of_sync = false;
            state->manual_dirty = false;
            (void)ss_regional_context_detect(&state->regional_context);
            sync_system_time_controls(state);
            ss_linux_date_time_location_update_summary(state);

            if (request->operation == SS_SYSTEM_TIME_OPERATION_TIMEZONE &&
                state->location_metadata_present) {
                SsSystemTimeState observed;
                if (ss_system_time_service_read(
                        state->system_time_service, &observed) &&
                    observed.timezone[0] != '\0' &&
                    g_strcmp0(
                        state->location_metadata.timezone_id,
                        observed.timezone) != 0) {
                    metadata_out_of_sync = true;
                }
            }

            set_status(
                state,
                metadata_out_of_sync
                    ? "The operating-system time zone changed, but locality metadata is not yet synchronized."
                    : "Operating-system date/time settings updated.",
                metadata_out_of_sync);
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
        begin_system_time_operation(
            state, SS_SYSTEM_TIME_OPERATION_TIMEZONE);

    if (request == NULL) {
        sync_system_time_controls(state);
        set_status(
            state,
            "Another protected date/time change is still being applied.",
            true);
        return;
    }
    sync_system_time_controls(state);
    set_status(state, "Updating the operating-system time zone…", false);
    ss_system_time_service_set_timezone_async(
        state->system_time_service,
        timezone_id,
        state->timezone_cancellable,
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
    request = begin_system_time_operation(
        state, SS_SYSTEM_TIME_OPERATION_NTP);
    if (request == NULL) {
        sync_system_time_controls(state);
        set_status(
            state,
            "Another protected date/time change is still being applied.",
            true);
        return;
    }
    sync_system_time_controls(state);
    set_status(
        state,
        enabled ? "Enabling network time…" : "Disabling network time…",
        false);
    ss_system_time_service_set_ntp_async(
        state->system_time_service,
        enabled,
        state->ntp_cancellable,
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

void on_manual_entry_changed(
    GtkEditable *editable G_GNUC_UNUSED,
    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;

    if (state != NULL && !state->updating_system_controls) {
        state->manual_dirty = true;
    }
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
            begin_system_time_operation(
                state, SS_SYSTEM_TIME_OPERATION_MANUAL);

        if (request == NULL) {
            set_status(
                state,
                "Another protected date/time change is still being applied.",
                true);
            return;
        }
        sync_system_time_controls(state);
        set_status(
            state,
            "Setting the operating-system date and time…",
            false);
        ss_system_time_service_set_time_async(
            state->system_time_service,
            unix_time_usec,
            state->manual_time_cancellable,
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
    if (state->locality_recovery_failed) {
        set_status(
            state,
            "Date & Time is available, but interrupted locality metadata could not yet be recovered. The journal has been preserved for retry.",
            true);
    } else {
        set_status(
            state,
            state->model.persisted_policy_present
                ? "Using the saved system-wide temporal policy. Native Mint/Linux date and time controls are live."
                : "Using Mint/Cinnamon temporal preferences as the initial policy. Native Mint/Linux date and time controls are live.",
            false);
    }
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
    {
        const InfiltratrTemporalPolicyV3 *policy =
            ss_date_time_model_policy(&state->model);
        if (policy == NULL) {
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_FAILED,
                "Unable to read the temporal policy during locality recovery.");
            ss_calendar_preview_provider_free(
                state->calendar_preview_provider);
            g_free(state);
            return NULL;
        }
        state->locality_recovery_failed =
            !ss_location_metadata_recover(
                policy->location_configured,
                policy->latitude,
                policy->longitude);
    }
    {
        const SsLocationMetadataLoadStatus metadata_status =
            ss_location_metadata_load_status(&state->location_metadata);
        state->location_metadata_present =
            metadata_status == SS_LOCATION_METADATA_LOAD_OK;
        state->location_metadata_uncertain =
            metadata_status == SS_LOCATION_METADATA_LOAD_INVALID ||
            metadata_status == SS_LOCATION_METADATA_LOAD_IO_ERROR;
    }
    refresh_location_authority_state(state);

    state->service_cancellable = g_cancellable_new();

    {
        g_autofree gchar *policy_path = temporal_policy_path();
        state->policy_observer = ss_policy_file_observer_new(
            policy_path,
            policy_file_changed,
            state);
        if (state->policy_observer == NULL) {
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_FAILED,
                "Unable to observe the temporal policy.");
            g_clear_object(&state->service_cancellable);
            ss_calendar_preview_provider_free(
                state->calendar_preview_provider);
            g_free(state);
            return NULL;
        }
    }
    {
        g_autofree gchar *metadata_path =
            ss_location_metadata_path_alloc();
        state->location_metadata_observer =
            ss_policy_file_observer_new(
                metadata_path,
                location_metadata_file_changed,
                state);
        if (state->location_metadata_observer == NULL) {
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_FAILED,
                "Unable to observe locality metadata.");
            ss_policy_file_observer_free(state->policy_observer);
            state->policy_observer = NULL;
            g_clear_object(&state->service_cancellable);
            ss_calendar_preview_provider_free(
                state->calendar_preview_provider);
            g_free(state);
            return NULL;
        }
    }

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
    g_signal_connect(
        state->root, "map",
        G_CALLBACK(on_panel_mapped), state);
    g_signal_connect(
        state->root, "unmap",
        G_CALLBACK(on_panel_unmapped), state);

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
    } else if (state->locality_recovery_failed) {
        set_status(
            state,
            "Date & Time is available, but interrupted locality metadata could not yet be recovered. The journal has been preserved for retry.",
            true);
        schedule_locality_recovery_retry(state);
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
        state->service_cancellable,
        system_time_service_ready,
        g_object_ref(state->window));
    state->preview_idle_id =
        g_idle_add(refresh_preview_once, state);
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

    stop_preview_timer(state);
    ss_linux_date_time_location_cancel_coordinate_commit(state);
    ss_policy_file_observer_free(state->policy_observer);
    state->policy_observer = NULL;
    ss_policy_file_observer_free(state->location_metadata_observer);
    state->location_metadata_observer = NULL;
    if (state->preview_idle_id != 0U) {
        g_source_remove(state->preview_idle_id);
        state->preview_idle_id = 0U;
    }
    if (state->locality_recovery_retry_id != 0U) {
        g_source_remove(state->locality_recovery_retry_id);
        state->locality_recovery_retry_id = 0U;
    }
    if (state->location_search_cancellable != NULL) {
        g_cancellable_cancel(state->location_search_cancellable);
        g_clear_object(&state->location_search_cancellable);
    }
    if (state->service_cancellable != NULL) {
        g_cancellable_cancel(state->service_cancellable);
        g_clear_object(&state->service_cancellable);
    }
    if (state->timezone_cancellable != NULL) {
        g_cancellable_cancel(state->timezone_cancellable);
        g_clear_object(&state->timezone_cancellable);
    }
    if (state->ntp_cancellable != NULL) {
        g_cancellable_cancel(state->ntp_cancellable);
        g_clear_object(&state->ntp_cancellable);
    }
    if (state->manual_time_cancellable != NULL) {
        g_cancellable_cancel(state->manual_time_cancellable);
        g_clear_object(&state->manual_time_cancellable);
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
