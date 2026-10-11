// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-preview.c
 * @brief Live Date & Time presentation isolated from the main controller.
 *
 * This unit compiles against a narrow preview facade. It cannot include the
 * complete panel-private controller and therefore cannot reach locality,
 * protected system-time or unrelated widget/controller state by accident.
 */

#include "linux-date-time-preview-private.h"

#include "system-settings/native-clock-policy.h"

#include <infiltratr/temporal.h>

#include <stdint.h>
#include <string.h>

static SsCalendarPreviewProvider *ensure_calendar_preview_provider(
    SsLinuxDateTimePanel *state)
{
    if (state == NULL) {
        return NULL;
    }

    if (ss_linux_date_time_preview_state(state)->calendar_provider == NULL) {
        ss_linux_date_time_preview_state(state)->calendar_provider =
            ss_calendar_preview_provider_new();
    }
    return ss_linux_date_time_preview_state(state)->calendar_provider;
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

    policy = ss_date_time_model_policy(ss_linux_date_time_preview_model(state));
    if (policy != NULL && policy->location_configured) {
        *latitude = policy->latitude;
        *longitude = policy->longitude;
        return true;
    }

    if (ss_linux_date_time_preview_regional_context(state)->has_reference_coordinates) {
        *latitude = ss_linux_date_time_preview_regional_context(state)->reference_latitude;
        *longitude = ss_linux_date_time_preview_regional_context(state)->reference_longitude;
        return true;
    }

    return false;
}

static bool format_preview_at(SsLinuxDateTimePanel *state,
                              GDateTime *now,
                              char *buffer,
                              size_t capacity)
{
    const InfiltratrTemporalPolicyV3 *policy =
        ss_date_time_model_policy(ss_linux_date_time_preview_model(state));
    const InfiltratrTemporalClockModeInfo *mode;
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
            ss_linux_date_time_preview_desktop_uses_24h(state));
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
    return infiltratr_temporal_format_clock_mode_localized(
        g_get_language_names()[0], effective_mode,
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

gboolean ss_linux_date_time_preview_refresh(gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;
    const InfiltratrTemporalPolicyV3 *policy;
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    char clock_text[160];

    if (state == NULL || ss_linux_date_time_preview_state(state)->clock_label == NULL ||
        ss_linux_date_time_preview_state(state)->date_label == NULL || now == NULL) {
        return G_SOURCE_CONTINUE;
    }

    policy = ss_date_time_model_policy(ss_linux_date_time_preview_model(state));
    if (policy == NULL) {
        return G_SOURCE_CONTINUE;
    }

    if (format_preview_at(state, now, clock_text, sizeof(clock_text)) &&
        g_strcmp0(
            gtk_label_get_text(GTK_LABEL(ss_linux_date_time_preview_state(state)->clock_label)),
            clock_text) != 0) {
        gtk_label_set_text(GTK_LABEL(ss_linux_date_time_preview_state(state)->clock_label), clock_text);
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
    const bool date_cache_hit =
        ss_linux_date_time_preview_state(state)->date_cache_valid &&
        ss_linux_date_time_preview_state(state)->date_year == year &&
        ss_linux_date_time_preview_state(state)->date_month == month &&
        ss_linux_date_time_preview_state(state)->date_day == day &&
        g_strcmp0(ss_linux_date_time_preview_state(state)->date_calendar, policy->calendar) == 0;

    if (!date_cache_hit) {
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
                gtk_label_get_text(GTK_LABEL(ss_linux_date_time_preview_state(state)->date_label)),
                summary) != 0) {
            gtk_label_set_text(GTK_LABEL(ss_linux_date_time_preview_state(state)->date_label), summary);
        }
        if (cacheable) {
            ss_linux_date_time_preview_state(state)->date_year = year;
            ss_linux_date_time_preview_state(state)->date_month = month;
            ss_linux_date_time_preview_state(state)->date_day = day;
            g_free(ss_linux_date_time_preview_state(state)->date_calendar);
            ss_linux_date_time_preview_state(state)->date_calendar = g_strdup(policy->calendar);
            ss_linux_date_time_preview_state(state)->date_cache_valid =
                ss_linux_date_time_preview_state(state)->date_calendar != NULL;
        }
    }

    return G_SOURCE_CONTINUE;
}

static gboolean refresh_preview_once(gpointer user_data)
{
    SsLinuxDateTimePanel *state = user_data;

    if (state != NULL) {
        ss_linux_date_time_preview_state(state)->idle_id = 0U;
        (void)ss_linux_date_time_preview_refresh(state);
    }
    return G_SOURCE_REMOVE;
}

static void stop_preview_timer(SsLinuxDateTimePanel *state)
{
    if (state != NULL && ss_linux_date_time_preview_state(state)->timer_id != 0U) {
        g_source_remove(ss_linux_date_time_preview_state(state)->timer_id);
        ss_linux_date_time_preview_state(state)->timer_id = 0U;
    }
}

guint ss_linux_date_time_panel_preview_interval_ms(
    const SsLinuxDateTimePanel *state)
{
    const InfiltratrTemporalPolicyV3 *policy =
        state != NULL
            ? ss_date_time_model_policy(ss_linux_date_time_preview_model(state))
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

void ss_linux_date_time_preview_restart(SsLinuxDateTimePanel *state)
{
    if (state == NULL || ss_linux_date_time_preview_root(state) == NULL ||
        !gtk_widget_get_mapped(ss_linux_date_time_preview_root(state))) {
        return;
    }
    stop_preview_timer(state);
    ss_linux_date_time_preview_state(state)->timer_id = g_timeout_add(
        ss_linux_date_time_panel_preview_interval_ms(state),
        refresh_preview,
        state);
    g_source_set_name_by_id(
        ss_linux_date_time_preview_state(state)->timer_id,
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
    (void)ss_linux_date_time_preview_refresh(state);
    if (ss_linux_date_time_preview_state(state)->timer_id == 0U) {
        ss_linux_date_time_preview_state(state)->timer_id = g_timeout_add(
            ss_linux_date_time_panel_preview_interval_ms(state),
            refresh_preview,
            state);
        g_source_set_name_by_id(
            ss_linux_date_time_preview_state(state)->timer_id,
            "[system-settings] visible Date & Time preview");
    }
}

static void on_panel_unmapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    stop_preview_timer(user_data);
}

void ss_linux_date_time_preview_init(SsLinuxDateTimePanel *panel)
{
    (void)ensure_calendar_preview_provider(panel);
}

void ss_linux_date_time_preview_attach(SsLinuxDateTimePanel *panel)
{
    GtkWidget *root = ss_linux_date_time_preview_root(panel);

    if (root == NULL) {
        return;
    }
    g_signal_connect(root, "map", G_CALLBACK(on_panel_mapped), panel);
    g_signal_connect(root, "unmap", G_CALLBACK(on_panel_unmapped), panel);
}

void ss_linux_date_time_preview_schedule_initial(SsLinuxDateTimePanel *panel)
{
    SsLinuxDateTimePreviewState *preview =
        ss_linux_date_time_preview_state(panel);

    if (preview != NULL && preview->idle_id == 0U) {
        preview->idle_id = g_idle_add(refresh_preview_once, panel);
    }
}

void ss_linux_date_time_preview_dispose(SsLinuxDateTimePanel *panel)
{
    SsLinuxDateTimePreviewState *preview =
        ss_linux_date_time_preview_state(panel);

    if (preview == NULL) {
        return;
    }
    stop_preview_timer(panel);
    if (preview->idle_id != 0U) {
        g_source_remove(preview->idle_id);
        preview->idle_id = 0U;
    }
    g_clear_pointer(&preview->date_calendar, g_free);
    ss_calendar_preview_provider_free(preview->calendar_provider);
    preview->calendar_provider = NULL;
    preview->date_cache_valid = false;
}
