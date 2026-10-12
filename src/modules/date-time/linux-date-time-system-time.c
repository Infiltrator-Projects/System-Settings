// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-system-time.c
 * @brief Protected operating-system date/time controller for Date & Time.
 *
 * Owns timedated/service lifecycle, timezone/NTP/manual-time operations,
 * cancellation generations and protected-control synchronization.
 */

#include "linux-date-time-system-time-private.h"

#include "system-settings/manual-time.h"
#include "system-settings/system-time-service.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum SsSystemTimeOperation {
    SS_SYSTEM_TIME_OPERATION_TIMEZONE,
    SS_SYSTEM_TIME_OPERATION_NTP,
    SS_SYSTEM_TIME_OPERATION_MANUAL
} SsSystemTimeOperation;

typedef struct SystemTimeUiRequest {
    SsLinuxDateTimeLifetime *lifetime;
    guint generation;
    SsSystemTimeOperation operation;
} SystemTimeUiRequest;

bool ss_linux_date_time_panel_read_system_time(
    const SsLinuxDateTimePanel *state,
    SsSystemTimeState *out)
{
    return state != NULL &&
           out != NULL &&
           ss_linux_date_time_system_time_state_const(state)->service != NULL &&
           ss_system_time_service_read(
               ss_linux_date_time_system_time_state_const(state)->service,
               out) == SS_SYSTEM_TIME_RESULT_OK;
}

static guint timezone_index_for_id(
    const SsLinuxDateTimePanel *state,
    const char *timezone_id)
{
    size_t index;

    if (state == NULL || ss_linux_date_time_system_time_state_const(state)->timezone_ids == NULL ||
        timezone_id == NULL) {
        return GTK_INVALID_LIST_POSITION;
    }

    for (index = 0U; index < ss_linux_date_time_system_time_state_const(state)->timezone_ids->len; ++index) {
        const char *candidate = g_ptr_array_index(
            ss_linux_date_time_system_time_state_const(state)->timezone_ids, (guint)index);
        if (g_strcmp0(candidate, timezone_id) == 0) {
            return (guint)index;
        }
    }
    return GTK_INVALID_LIST_POSITION;
}

static bool manual_representation_supported(
    const SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL) {
        return false;
    }

    policy = ss_linux_date_time_system_time_policy(state);
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

    if (state == NULL || ss_linux_date_time_system_time_state(state)->manual_date == NULL ||
        ss_linux_date_time_system_time_state(state)->manual_time == NULL) {
        return;
    }

    policy = ss_linux_date_time_system_time_policy(state);
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
            GTK_EDITABLE(ss_linux_date_time_system_time_state(state)->manual_date), date);
    }
    if (ss_manual_time_format(
            policy->clock_mode,
            ss_linux_date_time_panel_desktop_uses_24h(state),
            unix_microseconds,
            (int32_t)(offset_microseconds / G_USEC_PER_SEC),
            policy->show_seconds,
            time,
            sizeof(time))) {
        gtk_editable_set_text(
            GTK_EDITABLE(ss_linux_date_time_system_time_state(state)->manual_time), time);
    }
}

void ss_linux_date_time_system_time_sync(SsLinuxDateTimePanel *state)
{
    SsSystemTimeState system_state;
    guint zone_index;
    bool manual_supported;
    bool manual_visible;

    if (state == NULL) {
        return;
    }

    if (ss_linux_date_time_system_time_state(state)->service == NULL ||
        ss_system_time_service_read(
            ss_linux_date_time_system_time_state(state)->service,
            &system_state) != SS_SYSTEM_TIME_RESULT_OK) {
        if (ss_linux_date_time_system_time_state(state)->timezone != NULL) {
            gtk_widget_set_sensitive(
                GTK_WIDGET(ss_linux_date_time_system_time_state(state)->timezone), FALSE);
        }
        if (ss_linux_date_time_system_time_state(state)->network_time != NULL) {
            gtk_widget_set_sensitive(
                GTK_WIDGET(ss_linux_date_time_system_time_state(state)->network_time), FALSE);
        }
        if (ss_linux_date_time_system_time_state(state)->manual_row != NULL) {
            gtk_widget_set_visible(ss_linux_date_time_system_time_state(state)->manual_row, FALSE);
        }
        if (ss_linux_date_time_system_time_state(state)->manual_unavailable != NULL) {
            gtk_widget_set_visible(
                ss_linux_date_time_system_time_state(state)->manual_unavailable, FALSE);
        }
        if (ss_linux_date_time_system_time_state(state)->overview_timezone != NULL) {
            gtk_label_set_text(GTK_LABEL(ss_linux_date_time_system_time_state(state)->overview_timezone), "Unavailable");
        }
        if (ss_linux_date_time_system_time_state(state)->overview_sync != NULL) {
            gtk_label_set_text(GTK_LABEL(ss_linux_date_time_system_time_state(state)->overview_sync), "Unavailable");
        }
        return;
    }

    ss_linux_date_time_system_time_state(state)->updating_controls = true;

    if (ss_linux_date_time_system_time_state(state)->overview_timezone != NULL) {
        gtk_label_set_text(
            GTK_LABEL(ss_linux_date_time_system_time_state(state)->overview_timezone),
            system_state.timezone[0] != '\0'
                ? system_state.timezone
                : "Unknown");
    }
    if (ss_linux_date_time_system_time_state(state)->overview_sync != NULL) {
        gtk_label_set_text(
            GTK_LABEL(ss_linux_date_time_system_time_state(state)->overview_sync),
            system_state.ntp_enabled ? "Automatic" : "Manual");
    }

    if (ss_linux_date_time_system_time_state(state)->timezone != NULL) {
        zone_index = timezone_index_for_id(
            state, system_state.timezone);
        if (zone_index == GTK_INVALID_LIST_POSITION) {
            GtkStringList *strings = GTK_STRING_LIST(gtk_drop_down_get_model(ss_linux_date_time_system_time_state(state)->timezone));
            g_ptr_array_add(ss_linux_date_time_system_time_state(state)->timezone_ids, g_strdup(system_state.timezone));
            gtk_string_list_append(strings, system_state.timezone);
            zone_index = ss_linux_date_time_system_time_state(state)->timezone_ids->len - 1U;
        }
        if (zone_index != GTK_INVALID_LIST_POSITION) {
            gtk_drop_down_set_selected(
                ss_linux_date_time_system_time_state(state)->timezone, zone_index);
        }
        gtk_widget_set_sensitive(
            GTK_WIDGET(ss_linux_date_time_system_time_state(state)->timezone), TRUE);
    }

    if (ss_linux_date_time_system_time_state(state)->network_time != NULL) {
        gtk_switch_set_active(
            ss_linux_date_time_system_time_state(state)->network_time, system_state.ntp_enabled);
        gtk_widget_set_sensitive(
            GTK_WIDGET(ss_linux_date_time_system_time_state(state)->network_time),
            system_state.can_ntp);
    }

    manual_supported = manual_representation_supported(state);
    manual_visible = !system_state.ntp_enabled && manual_supported;

    if (ss_linux_date_time_system_time_state(state)->manual_row != NULL) {
        gtk_widget_set_visible(ss_linux_date_time_system_time_state(state)->manual_row, manual_visible);
    }
    if (ss_linux_date_time_system_time_state(state)->manual_unavailable != NULL) {
        gtk_widget_set_visible(
            ss_linux_date_time_system_time_state(state)->manual_unavailable,
            !system_state.ntp_enabled && !manual_supported);
    }

    const bool protected_operation_active =
        ss_linux_date_time_system_time_state(state)->timezone_cancellable != NULL ||
        ss_linux_date_time_system_time_state(state)->ntp_cancellable != NULL ||
        ss_linux_date_time_system_time_state(state)->manual_time_cancellable != NULL;

    if (!manual_visible) {
        /*
         * A hidden editor must never retain text authored under a different
         * clock/calendar/NTP context. Re-seed it when manual mode becomes
         * meaningful again.
         */
        ss_linux_date_time_system_time_state(state)->manual_dirty = false;
    } else {
        gtk_widget_set_sensitive(
            GTK_WIDGET(ss_linux_date_time_system_time_state(state)->manual_date), !protected_operation_active);
        gtk_widget_set_sensitive(
            GTK_WIDGET(ss_linux_date_time_system_time_state(state)->manual_time), !protected_operation_active);
        gtk_widget_set_sensitive(
            GTK_WIDGET(ss_linux_date_time_system_time_state(state)->manual_set_time), !protected_operation_active);
        if (!ss_linux_date_time_system_time_state(state)->manual_dirty) {
            fill_manual_time_entries(state);
        }
    }

    if (ss_linux_date_time_system_time_state(state)->timezone != NULL) {
        gtk_widget_set_sensitive(
            GTK_WIDGET(ss_linux_date_time_system_time_state(state)->timezone), !protected_operation_active);
    }
    if (ss_linux_date_time_system_time_state(state)->network_time != NULL) {
        gtk_widget_set_sensitive(
            GTK_WIDGET(ss_linux_date_time_system_time_state(state)->network_time),
            system_state.can_ntp && !protected_operation_active);
    }

    ss_linux_date_time_system_time_state(state)->updating_controls = false;
}

static void system_time_changed(
    SsSystemTimeService *service G_GNUC_UNUSED,
    const SsSystemTimeState *system_state,
    gpointer user_data)
{
    ss_linux_date_time_panel_reconcile_system_time_change(
        user_data, system_state);
}

static void system_time_ui_request_free(SystemTimeUiRequest *request)
{
    if (request == NULL) {
        return;
    }
    ss_linux_date_time_lifetime_unref(request->lifetime);
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
        return &ss_linux_date_time_system_time_state(state)->timezone_generation;
    case SS_SYSTEM_TIME_OPERATION_NTP:
        return &ss_linux_date_time_system_time_state(state)->ntp_generation;
    case SS_SYSTEM_TIME_OPERATION_MANUAL:
        return &ss_linux_date_time_system_time_state(state)->manual_time_generation;
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
        return &ss_linux_date_time_system_time_state(state)->timezone_cancellable;
    case SS_SYSTEM_TIME_OPERATION_NTP:
        return &ss_linux_date_time_system_time_state(state)->ntp_cancellable;
    case SS_SYSTEM_TIME_OPERATION_MANUAL:
        return &ss_linux_date_time_system_time_state(state)->manual_time_cancellable;
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
         ss_linux_date_time_system_time_state(state)->timezone_cancellable != NULL) ||
        (operation != SS_SYSTEM_TIME_OPERATION_NTP &&
         ss_linux_date_time_system_time_state(state)->ntp_cancellable != NULL) ||
        (operation != SS_SYSTEM_TIME_OPERATION_MANUAL &&
         ss_linux_date_time_system_time_state(state)->manual_time_cancellable != NULL)) {
        return NULL;
    }

    if (*cancellable != NULL) {
        g_cancellable_cancel(*cancellable);
        g_clear_object(cancellable);
    }
    *cancellable = g_cancellable_new();
    ++(*generation);

    request = g_new0(SystemTimeUiRequest, 1);
    request->lifetime =
        ss_linux_date_time_lifetime_ref(ss_linux_date_time_system_time_lifetime(state));
    request->generation = *generation;
    request->operation = operation;
    return request;
}

static void system_time_operation_complete(
    SsSystemTimeService *service G_GNUC_UNUSED,
    SsSystemTimeResult result,
    const char *error_message,
    gpointer user_data)
{
    SystemTimeUiRequest *request = user_data;
    SsLinuxDateTimePanel *state = request != NULL
        ? ss_linux_date_time_lifetime_get_panel(request->lifetime)
        : NULL;

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
        if (result == SS_SYSTEM_TIME_RESULT_OK) {
            SsSystemTimeState observed = {0};
            const char *observed_timezone = NULL;

            ss_linux_date_time_system_time_invalidate_manual_draft(state);
            if (request->operation == SS_SYSTEM_TIME_OPERATION_TIMEZONE &&
                ss_linux_date_time_panel_read_system_time(
                    state, &observed) &&
                observed.timezone[0] != '\0') {
                observed_timezone = observed.timezone;
            }
            const bool metadata_out_of_sync =
                ss_linux_date_time_panel_system_time_operation_applied(
                    state,
                    request->operation == SS_SYSTEM_TIME_OPERATION_TIMEZONE,
                    observed_timezone);

            ss_linux_date_time_panel_set_status(
                state,
                metadata_out_of_sync
                    ? "The operating-system time zone changed, but locality metadata is not yet synchronized."
                    : "Operating-system date/time settings updated.",
                metadata_out_of_sync);
        } else {
            const char *message = error_message != NULL
                ? error_message
                : ss_system_time_result_default_message(result);
            ss_linux_date_time_system_time_sync(state);
            ss_linux_date_time_panel_set_status(
                state,
                message != NULL
                    ? message
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
    if (state == NULL || ss_linux_date_time_system_time_state(state)->service == NULL ||
        timezone_id == NULL || timezone_id[0] == '\0') {
        return;
    }

    SystemTimeUiRequest *request =
        begin_system_time_operation(
            state, SS_SYSTEM_TIME_OPERATION_TIMEZONE);

    if (request == NULL) {
        ss_linux_date_time_system_time_sync(state);
        ss_linux_date_time_panel_set_status(
            state,
            "Another protected date/time change is still being applied.",
            true);
        return;
    }
    ss_linux_date_time_system_time_sync(state);
    ss_linux_date_time_panel_set_status(state, "Updating the operating-system time zone…", false);
    ss_system_time_service_set_timezone_async(
        ss_linux_date_time_system_time_state(state)->service,
        timezone_id,
        ss_linux_date_time_system_time_state(state)->timezone_cancellable,
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

    if (state == NULL || ss_linux_date_time_system_time_state(state)->updating_controls ||
        ss_linux_date_time_system_time_state(state)->timezone_ids == NULL) {
        return;
    }

    selected = gtk_drop_down_get_selected(ss_linux_date_time_system_time_state(state)->timezone);
    if (selected >= ss_linux_date_time_system_time_state(state)->timezone_ids->len) {
        return;
    }
    timezone_id = g_ptr_array_index(ss_linux_date_time_system_time_state(state)->timezone_ids, selected);
    request_system_timezone(state, timezone_id);
}

void on_network_time_changed(GObject *object,
                                    GParamSpec *pspec G_GNUC_UNUSED,
                                    gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    bool enabled;

    if (state == NULL || ss_linux_date_time_system_time_state(state)->updating_controls ||
        ss_linux_date_time_system_time_state(state)->service == NULL) {
        return;
    }

    SystemTimeUiRequest *request;

    enabled = gtk_switch_get_active(GTK_SWITCH(object)) != FALSE;
    request = begin_system_time_operation(
        state, SS_SYSTEM_TIME_OPERATION_NTP);
    if (request == NULL) {
        ss_linux_date_time_system_time_sync(state);
        ss_linux_date_time_panel_set_status(
            state,
            "Another protected date/time change is still being applied.",
            true);
        return;
    }
    ss_linux_date_time_system_time_sync(state);
    ss_linux_date_time_panel_set_status(
        state,
        enabled ? "Enabling network time…" : "Disabling network time…",
        false);
    ss_system_time_service_set_ntp_async(
        ss_linux_date_time_system_time_state(state)->service,
        enabled,
        ss_linux_date_time_system_time_state(state)->ntp_cancellable,
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

    policy = ss_linux_date_time_system_time_policy(state);
    if (policy == NULL ||
        !ss_manual_time_parse(
            policy->clock_mode,
            ss_linux_date_time_panel_desktop_uses_24h(state),
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

    if (state != NULL && !ss_linux_date_time_system_time_state(state)->updating_controls) {
        ss_linux_date_time_system_time_state(state)->manual_dirty = true;
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

    if (state == NULL || ss_linux_date_time_system_time_state(state)->service == NULL) {
        return;
    }

    date_text = gtk_editable_get_text(GTK_EDITABLE(ss_linux_date_time_system_time_state(state)->manual_date));
    time_text = gtk_editable_get_text(GTK_EDITABLE(ss_linux_date_time_system_time_state(state)->manual_time));
    if (!parse_manual_datetime(
            state, date_text, time_text, &unix_time_usec)) {
        ss_linux_date_time_panel_set_status(
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
            ss_linux_date_time_panel_set_status(
                state,
                "Another protected date/time change is still being applied.",
                true);
            return;
        }
        ss_linux_date_time_system_time_sync(state);
        ss_linux_date_time_panel_set_status(
            state,
            "Setting the operating-system date and time…",
            false);
        ss_system_time_service_set_time_async(
            ss_linux_date_time_system_time_state(state)->service,
            unix_time_usec,
            ss_linux_date_time_system_time_state(state)->manual_time_cancellable,
            system_time_operation_complete,
            request);
    }
}

static void system_time_service_ready(
    SsSystemTimeService *service,
    SsSystemTimeResult result,
    const char *error_message,
    gpointer user_data)
{
    SsLinuxDateTimeLifetime *lifetime = user_data;
    SsLinuxDateTimePanel *state =
        ss_linux_date_time_lifetime_get_panel(lifetime);

    if (state == NULL) {
        ss_system_time_service_free(service);
        ss_linux_date_time_lifetime_unref(lifetime);
        return;
    }

    /*
     * Proxy construction is complete on every callback path. Retaining this
     * completed cancellable falsely made the controller look as though a
     * connection attempt was still in flight and blocked a later restart.
     */
    g_clear_object(
        &ss_linux_date_time_system_time_state(state)->service_cancellable);

    if (service == NULL || result != SS_SYSTEM_TIME_RESULT_OK) {
        const char *message = error_message != NULL
            ? error_message
            : ss_system_time_result_default_message(result);
        ss_linux_date_time_panel_set_status(
            state,
            message != NULL
                ? message
                : "The operating-system date/time service is unavailable.",
            true);
        ss_linux_date_time_lifetime_unref(lifetime);
        return;
    }

    ss_linux_date_time_system_time_state(state)->service = service;
    ss_system_time_service_set_changed_callback(
        ss_linux_date_time_system_time_state(state)->service,
        system_time_changed,
        state);
    ss_linux_date_time_system_time_sync(state);
    SsSystemTimeState initial;
    const SsSystemTimeResult initial_result =
        ss_system_time_service_read(service, &initial);
    if (initial_result != SS_SYSTEM_TIME_RESULT_OK) {
        const char *message =
            ss_system_time_result_default_message(initial_result);
        ss_linux_date_time_panel_set_status(
            state,
            message != NULL
                ? message
                : "The operating-system date/time service is unavailable.",
            true);
        ss_linux_date_time_lifetime_unref(lifetime);
        return;
    }
    ss_linux_date_time_panel_system_time_service_ready(state);
    ss_linux_date_time_lifetime_unref(lifetime);
}

void ss_linux_date_time_system_time_invalidate_manual_draft(
    SsLinuxDateTimePanel *state)
{
    if (state != NULL) {
        ss_linux_date_time_system_time_state(state)->manual_dirty = false;
    }
}

void ss_linux_date_time_system_time_start(
    SsLinuxDateTimePanel *state)
{
    if (state == NULL || ss_linux_date_time_system_time_lifetime(state) == NULL ||
        ss_linux_date_time_system_time_state(state)->service_cancellable != NULL ||
        ss_linux_date_time_system_time_state(state)->service != NULL) {
        return;
    }

    ss_linux_date_time_system_time_state(state)->service_cancellable = g_cancellable_new();
    ss_system_time_service_new_async(
        ss_linux_date_time_system_time_state(state)->service_cancellable,
        system_time_service_ready,
        ss_linux_date_time_lifetime_ref(ss_linux_date_time_system_time_lifetime(state)));
}

void ss_linux_date_time_system_time_dispose(
    SsLinuxDateTimePanel *state)
{
    if (state == NULL) {
        return;
    }

    if (ss_linux_date_time_system_time_state(state)->service_cancellable != NULL) {
        g_cancellable_cancel(ss_linux_date_time_system_time_state(state)->service_cancellable);
        g_clear_object(&ss_linux_date_time_system_time_state(state)->service_cancellable);
    }
    if (ss_linux_date_time_system_time_state(state)->timezone_cancellable != NULL) {
        g_cancellable_cancel(ss_linux_date_time_system_time_state(state)->timezone_cancellable);
        g_clear_object(&ss_linux_date_time_system_time_state(state)->timezone_cancellable);
    }
    if (ss_linux_date_time_system_time_state(state)->ntp_cancellable != NULL) {
        g_cancellable_cancel(ss_linux_date_time_system_time_state(state)->ntp_cancellable);
        g_clear_object(&ss_linux_date_time_system_time_state(state)->ntp_cancellable);
    }
    if (ss_linux_date_time_system_time_state(state)->manual_time_cancellable != NULL) {
        g_cancellable_cancel(ss_linux_date_time_system_time_state(state)->manual_time_cancellable);
        g_clear_object(&ss_linux_date_time_system_time_state(state)->manual_time_cancellable);
    }

    g_clear_pointer(&ss_linux_date_time_system_time_state(state)->timezone_ids, g_ptr_array_unref);
    ss_system_time_service_free(ss_linux_date_time_system_time_state(state)->service);
    ss_linux_date_time_system_time_state(state)->service = NULL;
}
