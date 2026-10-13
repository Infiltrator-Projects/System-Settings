// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-panel.c
 * @brief Built-in Linux Date & Time settings module.
 *
 * Cross-domain policy reconciliation and panel composition live here.
 * Protected system-time operations are isolated in
 * linux-date-time-system-time.c; GTK construction remains isolated in
 * linux-date-time-panel-ui.c.
 * The GTK-facing header is private to the current built-in integration and is
 * not the future public module ABI.
 */

#include "linux-date-time-panel-private.h"
#include "linux-ui-helpers.h"

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

struct SsLinuxDateTimeLifetime {
    gint references;
    gpointer panel;
};

SsLinuxDateTimeLifetime *ss_linux_date_time_lifetime_new(
    SsLinuxDateTimePanel *panel)
{
    SsLinuxDateTimeLifetime *lifetime =
        g_new0(SsLinuxDateTimeLifetime, 1);

    lifetime->references = 1;
    g_atomic_pointer_set(&lifetime->panel, panel);
    return lifetime;
}

SsLinuxDateTimeLifetime *ss_linux_date_time_lifetime_ref(
    SsLinuxDateTimeLifetime *lifetime)
{
    if (lifetime != NULL) {
        g_atomic_int_inc(&lifetime->references);
    }
    return lifetime;
}

void ss_linux_date_time_lifetime_invalidate(
    SsLinuxDateTimeLifetime *lifetime)
{
    if (lifetime != NULL) {
        g_atomic_pointer_set(&lifetime->panel, NULL);
    }
}

void ss_linux_date_time_lifetime_unref(
    SsLinuxDateTimeLifetime *lifetime)
{
    if (lifetime != NULL &&
        g_atomic_int_dec_and_test(&lifetime->references)) {
        g_free(lifetime);
    }
}

SsLinuxDateTimePanel *ss_linux_date_time_lifetime_get_panel(
    SsLinuxDateTimeLifetime *lifetime)
{
    return lifetime != NULL
        ? g_atomic_pointer_get(&lifetime->panel)
        : NULL;
}

static void sync_controls(SsLinuxDateTimePanel *state);
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
    if (state->location.metadata_uncertain) {
        /*
         * An unreadable locality file is not evidence that the user has no
         * explicit locality. Never replace uncertain authority with a tzdata
         * reference approximation.
         */
        state->location.follows_timezone_reference = false;
        return;
    }
    state->location.follows_timezone_reference =
        !metadata_matches &&
        (policy == NULL ||
         !policy->location_configured ||
         ss_linux_date_time_location_policy_matches_reference(state));
}


void ss_linux_date_time_panel_refresh_location_authority_state(
    SsLinuxDateTimePanel *state)
{
    refresh_location_authority_state(state);
}

void ss_linux_date_time_panel_reconcile_system_time_change(
    SsLinuxDateTimePanel *state,
    const SsSystemTimeState *system_state)
{

    if (state == NULL || system_state == NULL) {
        return;
    }

    if (!system_state->available) {
        ss_linux_date_time_system_time_sync(state);
        ss_linux_date_time_panel_set_status(state, "The operating-system date/time service is unavailable.", true);
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
        ss_linux_date_time_system_time_invalidate_manual_draft(state);
    }

    /*
     * Time-zone reconciliation and automatic reference-coordinate publication
     * are locality writers too. Serialize them with explicit locality changes
     * and reload both policy and metadata after taking the locality lock, so a
     * second System Settings process cannot overwrite a freshly selected place
     * with stale metadata or stale time-zone reference coordinates.
     */
    if ((timezone_changed || state->location.follows_timezone_reference) &&
        ss_location_metadata_transaction_begin()) {
        SsLocationMetadata authoritative_metadata = {0};
        SsSystemTimeState locked_system = {0};
        const bool locked_system_ok =
            ss_linux_date_time_panel_read_system_time(
                state, &locked_system);
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
            state->location.metadata = authoritative_metadata;
            state->location.metadata_present = true;
        } else if (metadata_status == SS_LOCATION_METADATA_LOAD_MISSING) {
            memset(&state->location.metadata, 0, sizeof(state->location.metadata));
            state->location.metadata_present = false;
        }
        if (policy_reloaded) {
            ss_linux_date_time_panel_refresh_location_authority_state(state);
        }

        if (policy_reloaded &&
            ss_linux_date_time_location_metadata_matches_policy(
                state, ss_date_time_model_policy(&state->model)) &&
            locked_system_ok &&
            locked_system.timezone[0] != '\0' &&
            g_strcmp0(
                state->location.metadata.timezone_id,
                locked_system.timezone) != 0) {
            SsLocationMetadata candidate = state->location.metadata;
            (void)g_strlcpy(
                candidate.timezone_id,
                locked_system.timezone,
                sizeof(candidate.timezone_id));
            if (ss_location_metadata_save(&candidate)) {
                state->location.metadata = candidate;
                ss_linux_date_time_panel_set_status(
                    state,
                    "Operating-system time zone and locality metadata reconciled.",
                    false);
            } else {
                ss_linux_date_time_panel_set_status(
                    state,
                    "The operating-system time zone changed, but the locality metadata could not be updated on disk.",
                    true);
            }
        }

        if (policy_reloaded &&
            state->location.follows_timezone_reference &&
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
                ss_linux_date_time_panel_set_status(
                    state,
                    "The system time zone changed, but its reference coordinates could not be persisted to the temporal policy.",
                    true);
            } else {
                state->location.updating_controls = true;
                gtk_spin_button_set_value(
                    state->location.latitude,
                    state->regional_context.reference_latitude);
                gtk_spin_button_set_value(
                    state->location.longitude,
                    state->regional_context.reference_longitude);
                if (state->location.search_entry != NULL &&
                    state->regional_context.timezone_city[0] != '\0') {
                    gtk_editable_set_text(
                        GTK_EDITABLE(state->location.search_entry),
                        state->regional_context.timezone_city);
                }
                state->location.updating_controls = false;
            }
        }

        ss_location_metadata_transaction_end();
    } else if (timezone_changed ||
               state->location.follows_timezone_reference) {
        ss_linux_date_time_panel_set_status(
            state,
            "The system time changed, but the locality transaction lock could not be acquired.",
            true);
    }

    ss_linux_date_time_system_time_sync(state);
    ss_linux_date_time_location_update_summary(state);
    (void)ss_linux_date_time_preview_refresh(state);
}


bool ss_linux_date_time_panel_system_time_operation_applied(
    SsLinuxDateTimePanel *state,
    bool timezone_operation,
    const char *observed_timezone)
{
    if (state == NULL) {
        return false;
    }

    (void)ss_regional_context_detect(&state->regional_context);
    ss_linux_date_time_system_time_sync(state);
    ss_linux_date_time_location_update_summary(state);
    return timezone_operation &&
        state->location.metadata_present &&
        observed_timezone != NULL && observed_timezone[0] != '\0' &&
        g_strcmp0(
            state->location.metadata.timezone_id,
            observed_timezone) != 0;
}


void ss_linux_date_time_panel_system_time_service_ready(
    SsLinuxDateTimePanel *state)
{
    if (state == NULL) {
        return;
    }
    if (state->location.recovery_failed) {
        ss_linux_date_time_panel_set_status(
            state,
            "Date & Time is available, but interrupted locality metadata could not yet be recovered. The journal has been preserved for retry.",
            true);
    } else {
        ss_linux_date_time_panel_set_status(
            state,
            ss_date_time_model_has_persisted_policy(&state->model)
                ? "Using the saved system-wide temporal policy. Native Mint/Linux date and time controls are live."
                : "Using Mint/Cinnamon temporal preferences as the initial policy. Native Mint/Linux date and time controls are live.",
            false);
    }
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
        state->location.metadata = loaded;
        state->location.metadata_present = true;
        state->location.metadata_uncertain = false;
    } else if (status == SS_LOCATION_METADATA_LOAD_MISSING) {
        memset(&state->location.metadata, 0, sizeof(state->location.metadata));
        state->location.metadata_present = false;
        state->location.metadata_uncertain = false;
    } else {
        state->location.metadata_uncertain = true;
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

    if (state == NULL) {
        return G_SOURCE_REMOVE;
    }

    state->location.recovery_retry_id = 0U;
    if (ss_location_metadata_recover()) {
        state->location.recovery_failed = false;
        state->location.recovery_retry_seconds = 2U;
        location_metadata_file_changed(state);
        set_status(
            state,
            "Interrupted locality metadata was recovered.",
            false);
        return G_SOURCE_REMOVE;
    }

    state->location.recovery_failed = true;
    schedule_locality_recovery_retry(state);
    return G_SOURCE_REMOVE;
}

static void schedule_locality_recovery_retry(
    SsLinuxDateTimePanel *state)
{
    if (state == NULL ||
        state->location.recovery_retry_id != 0U ||
        !state->location.recovery_failed) {
        return;
    }

    if (state->location.recovery_retry_seconds == 0U)
        state->location.recovery_retry_seconds = 2U;
    state->location.recovery_retry_id = g_timeout_add_seconds(
        state->location.recovery_retry_seconds,
        retry_locality_recovery,
        state);
    state->location.recovery_retry_seconds =
        MIN(state->location.recovery_retry_seconds * 2U, 60U);
    g_source_set_name_by_id(
        state->location.recovery_retry_id,
        "[system-settings] locality recovery retry");
}

static void policy_file_changed(gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    InfiltratrTemporalPolicyV3 previous;
    const InfiltratrTemporalPolicyV3 *current;

    if (state == NULL) {
        return;
    }

    current = ss_date_time_model_policy(&state->model);
    if (current == NULL) {
        return;
    }
    previous = *current;
    ss_linux_date_time_location_cancel_coordinate_commit(state);
    if (ss_date_time_model_reload(&state->model)) {
        const InfiltratrTemporalPolicyV3 *policy =
            ss_date_time_model_policy(&state->model);
        const bool locality_recovery_ok =
            policy != NULL && ss_location_metadata_recover();
        SsLocationMetadata recovered_metadata = {0};
        const SsLocationMetadataLoadStatus recovered_metadata_status =
            locality_recovery_ok
                ? ss_location_metadata_load_status(&recovered_metadata)
                : SS_LOCATION_METADATA_LOAD_IO_ERROR;
        const bool recovered_metadata_present =
            recovered_metadata_status == SS_LOCATION_METADATA_LOAD_OK;
        state->location.recovery_failed = !locality_recovery_ok;
        if (locality_recovery_ok) {
            if (state->location.recovery_retry_id != 0U) {
                g_source_remove(state->location.recovery_retry_id);
                state->location.recovery_retry_id = 0U;
            }
        } else {
            schedule_locality_recovery_retry(state);
        }
        const bool changed =
            !temporal_policy_equal(&previous, policy);
        const bool manual_context_changed =
            policy != NULL &&
            (g_strcmp0(previous.clock_mode, policy->clock_mode) != 0 ||
             g_strcmp0(previous.calendar, policy->calendar) != 0);

        /*
         * Preserve a user's typed wall-time draft across unrelated external
         * changes. Only a changed clock/calendar representation invalidates
         * how that draft should be interpreted.
         */
        if (manual_context_changed) {
            ss_linux_date_time_system_time_invalidate_manual_draft(state);
        }
        if (recovered_metadata_present) {
            state->location.metadata = recovered_metadata;
            state->location.metadata_present = true;
            state->location.metadata_uncertain = false;
        } else if (locality_recovery_ok &&
                   recovered_metadata_status ==
                       SS_LOCATION_METADATA_LOAD_MISSING) {
            memset(&state->location.metadata, 0, sizeof(state->location.metadata));
            state->location.metadata_present = false;
            state->location.metadata_uncertain = false;
        } else if (locality_recovery_ok) {
            state->location.metadata_uncertain = true;
        }
        refresh_location_authority_state(state);
        sync_controls(state);
        ss_linux_date_time_preview_restart(state);
        (void)ss_linux_date_time_preview_refresh(state);
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

    state->native_format_updating = true;
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
    state->native_format_updating = false;
}

bool ss_linux_date_time_panel_desktop_uses_24h(const SsLinuxDateTimePanel *state)
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
    if (state->preview.overview_clock != NULL) {
        gtk_label_set_text(
            GTK_LABEL(state->preview.overview_clock),
            clock != NULL ? clock->name : policy->clock_mode);
    }

    calendar = infiltratr_temporal_calendar_find(policy->calendar);
    if (state->preview.overview_calendar != NULL) {
        gtk_label_set_text(
            GTK_LABEL(state->preview.overview_calendar),
            calendar != NULL ? calendar->name : policy->calendar);
    }
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
    gtk_widget_set_sensitive(GTK_WIDGET(state->location.latitude), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(state->location.longitude), TRUE);

    ss_linux_date_time_location_update_summary(state);
    update_overview_policy(state);
    sync_native_format_controls(state);
    ss_linux_date_time_system_time_sync(state);

    if (mode != NULL &&
        (mode->requires_latitude || mode->requires_longitude) &&
        (policy == NULL || !policy->location_configured) &&
        !state->regional_context.has_reference_coordinates) {
        set_status(state,
                   "This clock system needs a geographic location for a meaningful result.",
                   false);
    }
}



















static void sync_controls(SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy =
        ss_date_time_model_policy(&state->model);

    if (policy == NULL) {
        return;
    }

    state->updating_controls = true;
    state->location.updating_controls = true;
    gtk_drop_down_set_selected(
        state->clock_mode, clock_index_for_id(state, policy->clock_mode));
    gtk_drop_down_set_selected(
        state->calendar,
        calendar_index_for_id(policy->calendar));
    gtk_switch_set_active(state->show_seconds, policy->show_seconds);
    if (policy->location_configured) {
        gtk_spin_button_set_value(
            state->location.latitude, policy->latitude);
        gtk_spin_button_set_value(
            state->location.longitude, policy->longitude);
        if (state->location.search_entry != NULL &&
            ss_linux_date_time_location_metadata_matches_policy(state, policy)) {
            gtk_editable_set_text(
                GTK_EDITABLE(state->location.search_entry),
                state->location.metadata.display_name);
        }
    } else if (state->regional_context.has_reference_coordinates) {
        gtk_spin_button_set_value(
            state->location.latitude,
            state->regional_context.reference_latitude);
        gtk_spin_button_set_value(
            state->location.longitude,
            state->regional_context.reference_longitude);
        if (state->location.search_entry != NULL &&
            state->regional_context.timezone_city[0] != '\0') {
            gtk_editable_set_text(
                GTK_EDITABLE(state->location.search_entry),
                state->regional_context.timezone_city);
        }
    }
    state->location.updating_controls = false;
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

    if (state == NULL || key == NULL || state->updating_controls ||
        ss_date_time_model_is_saving(&state->model)) {
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
        ss_linux_date_time_preview_restart(state);
        set_status(
            state,
            "An external Mint/Cinnamon time preference changed and System Settings reconciled the matching setting.",
            false);
        return;
    }

    (void)ss_linux_date_time_preview_refresh(state);
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
    ss_linux_date_time_preview_restart(state);
    (void)ss_linux_date_time_preview_refresh(state);
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
    ss_linux_date_time_system_time_invalidate_manual_draft(state);
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
    ss_linux_date_time_system_time_invalidate_manual_draft(state);
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

void on_show_date_changed(GObject *object,
                                 GParamSpec *pspec G_GNUC_UNUSED,
                                 gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    bool value;

    if (state == NULL || state->native_format_updating ||
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

    if (state == NULL || state->native_format_updating ||
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

static bool migrate_legacy_native_clock(
    SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy;
    const char *native_clock_mode;

    if (state == NULL) {
        return false;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL ||
        !ss_native_clock_mode_is_legacy_default(policy->clock_mode)) {
        return true;
    }

    native_clock_mode = ss_native_clock_mode_id(
        ss_linux_date_time_panel_desktop_uses_24h(state));
    if (!ss_date_time_model_has_persisted_policy(&state->model)) {
        return ss_date_time_model_normalize_unpersisted_clock_mode(
            &state->model, native_clock_mode);
    }
    return ss_date_time_model_set_clock_mode(
        &state->model, native_clock_mode);
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

    ss_linux_date_time_preview_init(state);

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
            ss_linux_date_time_preview_dispose(state);
            ss_date_time_model_clear(&state->model);
            g_free(state);
            return NULL;
        }
        state->location.recovery_failed =
            !ss_location_metadata_recover();
    }
    {
        const SsLocationMetadataLoadStatus metadata_status =
            ss_location_metadata_load_status(&state->location.metadata);
        state->location.metadata_present =
            metadata_status == SS_LOCATION_METADATA_LOAD_OK;
        state->location.metadata_uncertain =
            metadata_status == SS_LOCATION_METADATA_LOAD_INVALID ||
            metadata_status == SS_LOCATION_METADATA_LOAD_IO_ERROR;
    }
    refresh_location_authority_state(state);

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
            ss_linux_date_time_preview_dispose(state);
            ss_date_time_model_clear(&state->model);
            g_free(state);
            return NULL;
        }
    }
    {
        g_autofree gchar *metadata_path =
            ss_location_metadata_path_alloc();
        state->location.metadata_observer =
            ss_policy_file_observer_new(
                metadata_path,
                location_metadata_file_changed,
                state);
        if (state->location.metadata_observer == NULL) {
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_FAILED,
                "Unable to observe locality metadata.");
            ss_policy_file_observer_free(state->policy_observer);
            state->policy_observer = NULL;
            ss_linux_date_time_preview_dispose(state);
            ss_date_time_model_clear(&state->model);
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
    ss_linux_date_time_preview_attach(state);

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
    } else if (state->location.recovery_failed) {
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
    state->lifetime = ss_linux_date_time_lifetime_new(state);
    ss_linux_date_time_system_time_start(state);
    ss_linux_date_time_preview_schedule_initial(state);
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

    /* Make every outstanding async completion inert before cancellation. */
    ss_linux_date_time_lifetime_invalidate(state->lifetime);

    ss_linux_date_time_preview_dispose(state);
    ss_linux_date_time_location_cancel_coordinate_commit(state);
    ss_policy_file_observer_free(state->policy_observer);
    state->policy_observer = NULL;
    ss_policy_file_observer_free(state->location.metadata_observer);
    state->location.metadata_observer = NULL;
    if (state->location.recovery_retry_id != 0U) {
        g_source_remove(state->location.recovery_retry_id);
        state->location.recovery_retry_id = 0U;
    }
    if (state->location.search_cancellable != NULL) {
        g_cancellable_cancel(state->location.search_cancellable);
        g_clear_object(&state->location.search_cancellable);
    }
    ss_linux_date_time_system_time_dispose(state);

    g_clear_pointer(&state->clock_mode_ids, g_ptr_array_unref);
    g_clear_object(&state->cinnamon_interface_settings);
    if (state->root != NULL) {
        disconnect_panel_widgets(state->root, state);
        g_clear_object(&state->root);
    }
    ss_linux_date_time_lifetime_unref(state->lifetime);
    state->lifetime = NULL;
    ss_date_time_model_clear(&state->model);
    g_free(state);
}
