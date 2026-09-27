// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-temporal-presentation.c
 * @brief Authoritative temporal formatting for the Home dashboard.
 *
 * Home uses a long-lived presenter so clock ticks never rediscover Calendar,
 * recreate GSettings, or reread policy storage. Policy-file and Cinnamon
 * changes invalidate cached state through native observers.
 */
#include "home-temporal-presentation.h"
#include "calendar-preview-provider.h"

#include "system-settings/cinnamon-interface.h"
#include "system-settings/native-clock-policy.h"
#include "system-settings/temporal-policy-store.h"

#include <gio/gio.h>
#include <string.h>

struct SsHomeTemporalPresenter {
    InfiltratrTemporalPolicyV3 policy;
    bool policy_valid;
    bool use_24h;
    GSettings *settings;
    gulong settings_changed_id;
    GFileMonitor *policy_monitor;
    SsCalendarPreviewProvider *calendar_provider;
};

static bool desktop_uses_24h_from_settings(GSettings *settings)
{
    bool value = true;
    if (settings != NULL) {
        (void)ss_cinnamon_interface_get_boolean(
            settings, "clock-use-24h", &value);
    }
    return value;
}

static gchar *format_clock(
    const InfiltratrTemporalPolicyV3 *policy,
    GDateTime *now,
    bool use_24h)
{
    const InfiltratrTemporalClockModeInfo *mode;
    const char *effective_mode;
    char buffer[192];
    int64_t unix_us;
    gint64 offset_us;

    if (policy == NULL || now == NULL) return NULL;
    effective_mode = policy->clock_mode;
    if (strcmp(effective_mode, "standard") == 0) {
        effective_mode = ss_native_clock_mode_id(use_24h);
    }
    mode = infiltratr_temporal_clock_mode_find(effective_mode);
    if (mode == NULL) return NULL;
    if ((mode->requires_latitude || mode->requires_longitude) &&
        !policy->location_configured) {
        return g_strdup_printf("%s — location required", mode->name);
    }

    unix_us = g_date_time_to_unix(now) * G_USEC_PER_SEC +
        g_date_time_get_microsecond(now);
    offset_us = g_date_time_get_utc_offset(now);
    if (!infiltratr_temporal_format_clock_mode(
            effective_mode, unix_us,
            (int32_t)(offset_us / G_USEC_PER_SEC),
            policy->show_seconds, false,
            policy->location_configured,
            policy->latitude, policy->longitude,
            buffer, sizeof(buffer), NULL)) {
        return NULL;
    }
    return g_strdup(buffer);
}

static gchar *format_date_with_provider(
    const InfiltratrTemporalPolicyV3 *policy,
    GDateTime *now,
    SsCalendarPreviewProvider *provider)
{
    const InfiltratrTemporalCalendarInfo *calendar;
    gchar *formatted;

    if (policy == NULL || now == NULL) return NULL;
    if (strcmp(policy->calendar, "gregorian") == 0) {
        return g_date_time_format(now, "%A, %e %B %Y");
    }
    calendar = infiltratr_temporal_calendar_find(policy->calendar);
    if (calendar == NULL) return NULL;

    formatted = ss_calendar_preview_provider_format_date(
        provider, policy->calendar,
        g_date_time_get_year(now),
        g_date_time_get_month(now),
        g_date_time_get_day_of_month(now));
    if (formatted != NULL) return formatted;
    return g_strdup_printf("%s — preview unavailable", calendar->name);
}

static bool format_into(
    const InfiltratrTemporalPolicyV3 *policy,
    GDateTime *now,
    bool use_24h,
    SsCalendarPreviewProvider *provider,
    SsHomeTemporalPresentation *out)
{
    SsHomeTemporalPresentation candidate = {0};

    if (policy == NULL || now == NULL || out == NULL) return false;
    candidate.clock_text = format_clock(policy, now, use_24h);
    candidate.date_text = format_date_with_provider(policy, now, provider);
    if (candidate.clock_text == NULL || candidate.date_text == NULL) {
        ss_home_temporal_presentation_clear(&candidate);
        return false;
    }
    candidate.system_time_text = g_strdup_printf(
        "%s  %s", candidate.date_text, candidate.clock_text);
    if (candidate.system_time_text == NULL) {
        ss_home_temporal_presentation_clear(&candidate);
        return false;
    }
    ss_home_temporal_presentation_clear(out);
    *out = candidate;
    return true;
}

void ss_home_temporal_presentation_init(
    SsHomeTemporalPresentation *presentation)
{
    if (presentation != NULL) memset(presentation, 0, sizeof(*presentation));
}

void ss_home_temporal_presentation_clear(
    SsHomeTemporalPresentation *presentation)
{
    if (presentation == NULL) return;
    g_clear_pointer(&presentation->clock_text, g_free);
    g_clear_pointer(&presentation->date_text, g_free);
    g_clear_pointer(&presentation->system_time_text, g_free);
}

bool ss_home_temporal_presentation_format(
    const InfiltratrTemporalPolicyV3 *policy,
    GDateTime *now,
    bool use_24h,
    SsHomeTemporalPresentation *out)
{
    SsCalendarPreviewProvider *provider;
    bool ok;

    if (policy == NULL || now == NULL || out == NULL) return false;
    provider = ss_calendar_preview_provider_new();
    ok = format_into(policy, now, use_24h, provider, out);
    ss_calendar_preview_provider_free(provider);
    return ok;
}

static bool presenter_reload_policy(SsHomeTemporalPresenter *presenter)
{
    const SsTemporalPolicyStore *store;
    InfiltratrTemporalPolicyV3 loaded;
    bool found = false;

    if (presenter == NULL ||
        !infiltratr_temporal_policy_v3_default(&loaded)) return false;

    store = ss_platform_temporal_policy_store();
    if (store == NULL || store->load == NULL ||
        !store->load(&loaded, &found)) {
        presenter->policy_valid = false;
        return false;
    }
    presenter->policy = loaded;
    presenter->policy_valid = true;
    return true;
}

static void on_presenter_settings_changed(
    GSettings *settings,
    gchar *key,
    gpointer user_data)
{
    SsHomeTemporalPresenter *presenter = user_data;
    if (presenter != NULL && g_strcmp0(key, "clock-use-24h") == 0) {
        presenter->use_24h = desktop_uses_24h_from_settings(settings);
    }
}

static void on_policy_directory_changed(
    GFileMonitor *monitor G_GNUC_UNUSED,
    GFile *file G_GNUC_UNUSED,
    GFile *other_file G_GNUC_UNUSED,
    GFileMonitorEvent event,
    gpointer user_data)
{
    SsHomeTemporalPresenter *presenter = user_data;
    switch (event) {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_CREATED:
    case G_FILE_MONITOR_EVENT_DELETED:
    case G_FILE_MONITOR_EVENT_MOVED:
    case G_FILE_MONITOR_EVENT_RENAMED:
        (void)presenter_reload_policy(presenter);
        break;
    default:
        break;
    }
}

SsHomeTemporalPresenter *ss_home_temporal_presenter_new(void)
{
    SsHomeTemporalPresenter *presenter =
        g_new0(SsHomeTemporalPresenter, 1);
    g_autofree gchar *policy_path = NULL;
    g_autofree gchar *policy_dir = NULL;
    g_autoptr(GFile) directory = NULL;

    presenter->settings = ss_cinnamon_interface_settings_new();
    presenter->use_24h =
        desktop_uses_24h_from_settings(presenter->settings);
    if (presenter->settings != NULL) {
        presenter->settings_changed_id = g_signal_connect(
            presenter->settings, "changed",
            G_CALLBACK(on_presenter_settings_changed), presenter);
    }
    presenter->calendar_provider = ss_calendar_preview_provider_new();
    (void)presenter_reload_policy(presenter);

    policy_path = g_build_filename(
        g_get_user_config_dir(), "infiltrator",
        "presentation.conf", NULL);
    policy_dir = g_path_get_dirname(policy_path);
    directory = g_file_new_for_path(policy_dir);
    presenter->policy_monitor =
        g_file_monitor_directory(directory, G_FILE_MONITOR_NONE, NULL, NULL);
    if (presenter->policy_monitor != NULL) {
        g_signal_connect(
            presenter->policy_monitor, "changed",
            G_CALLBACK(on_policy_directory_changed), presenter);
    }
    return presenter;
}

void ss_home_temporal_presenter_free(
    SsHomeTemporalPresenter *presenter)
{
    if (presenter == NULL) return;
    if (presenter->settings != NULL &&
        presenter->settings_changed_id != 0U) {
        g_signal_handler_disconnect(
            presenter->settings, presenter->settings_changed_id);
    }
    g_clear_object(&presenter->policy_monitor);
    g_clear_object(&presenter->settings);
    ss_calendar_preview_provider_free(presenter->calendar_provider);
    g_free(presenter);
}

bool ss_home_temporal_presenter_format_now(
    SsHomeTemporalPresenter *presenter,
    SsHomeTemporalPresentation *out)
{
    g_autoptr(GDateTime) now = NULL;
    if (presenter == NULL || out == NULL || !presenter->policy_valid) {
        return false;
    }
    now = g_date_time_new_now_local();
    return now != NULL && format_into(
        &presenter->policy, now, presenter->use_24h,
        presenter->calendar_provider, out);
}

bool ss_home_temporal_presentation_now(
    SsHomeTemporalPresentation *out)
{
    SsHomeTemporalPresenter *presenter;
    bool ok;
    if (out == NULL) return false;
    presenter = ss_home_temporal_presenter_new();
    if (presenter == NULL) return false;
    ok = ss_home_temporal_presenter_format_now(presenter, out);
    ss_home_temporal_presenter_free(presenter);
    return ok;
}
