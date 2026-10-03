// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-temporal-presentation.c
 * @brief Authoritative temporal formatting for the Home dashboard.
 *
 * Home uses a long-lived presenter so clock ticks never rediscover Calendar,
 * recreate GSettings, reread policy storage, or reformat a calendar date that
 * has not changed. Policy-file and Cinnamon changes invalidate cached state
 * through native observers.
 */
#include "home-temporal-presentation.h"
#include "calendar-preview-provider.h"
#include "policy-file-observer.h"

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
    SsPolicyFileObserver *policy_observer;
    guint policy_reload_retry_id;
    guint policy_reload_retry_seconds;
    gchar *policy_path;
    SsCalendarPreviewProvider *calendar_provider;
    gchar *cached_date_text;
    gchar *cached_calendar_id;
    gint cached_year;
    gint cached_month;
    gint cached_day;
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
    /*
     * @out is a pure output parameter. Do not inspect or free its previous
     * contents: callers may pass an ordinary uninitialised stack value.
     * Reusers clear their prior presentation before requesting a replacement.
     */
    *out = candidate;
    return true;
}

static gchar *presenter_format_date(
    SsHomeTemporalPresenter *presenter,
    GDateTime *now)
{
    const gint year = g_date_time_get_year(now);
    const gint month = g_date_time_get_month(now);
    const gint day = g_date_time_get_day_of_month(now);
    const bool gregorian =
        g_strcmp0(presenter->policy.calendar, "gregorian") == 0;
    g_autofree gchar *formatted = NULL;

    if (presenter->cached_date_text != NULL &&
        presenter->cached_calendar_id != NULL &&
        presenter->cached_year == year &&
        presenter->cached_month == month &&
        presenter->cached_day == day &&
        g_strcmp0(presenter->cached_calendar_id,
                  presenter->policy.calendar) == 0) {
        return g_strdup(presenter->cached_date_text);
    }

    if (gregorian) {
        formatted = g_date_time_format(now, "%A, %e %B %Y");
    } else {
        const InfiltratrTemporalCalendarInfo *calendar =
            infiltratr_temporal_calendar_find(presenter->policy.calendar);
        formatted = ss_calendar_preview_provider_format_date(
            presenter->calendar_provider,
            presenter->policy.calendar,
            year, month, day);
        if (formatted == NULL) {
            return calendar != NULL
                ? g_strdup_printf(
                      "%s — preview unavailable", calendar->name)
                : NULL;
        }
    }
    if (formatted == NULL) {
        return NULL;
    }

    g_free(presenter->cached_date_text);
    presenter->cached_date_text = g_strdup(formatted);
    g_free(presenter->cached_calendar_id);
    presenter->cached_calendar_id = g_strdup(presenter->policy.calendar);
    presenter->cached_year = year;
    presenter->cached_month = month;
    presenter->cached_day = day;
    return g_steal_pointer(&formatted);
}

static bool presenter_format_into(
    SsHomeTemporalPresenter *presenter,
    GDateTime *now,
    SsHomeTemporalPresentation *out)
{
    SsHomeTemporalPresentation candidate = {0};

    if (presenter == NULL || now == NULL || out == NULL ||
        !presenter->policy_valid) {
        return false;
    }

    candidate.clock_text = format_clock(
        &presenter->policy, now, presenter->use_24h);
    candidate.date_text = presenter_format_date(presenter, now);
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
        /*
         * A transient read/permission error must not turn a live Home clock
         * into a frozen value that still looks authoritative. Keep the last
         * successfully loaded policy and retry; only a presenter that has
         * never loaded any valid policy remains unavailable.
         */
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

static gboolean retry_policy_reload(gpointer user_data);

static void schedule_policy_reload_retry(
    SsHomeTemporalPresenter *presenter)
{
    guint delay;

    if (presenter == NULL || presenter->policy_reload_retry_id != 0U) {
        return;
    }

    delay = presenter->policy_reload_retry_seconds != 0U
        ? presenter->policy_reload_retry_seconds
        : 2U;
    presenter->policy_reload_retry_id = g_timeout_add_seconds(
        delay, retry_policy_reload, presenter);
    g_source_set_name_by_id(
        presenter->policy_reload_retry_id,
        "[system-settings] Home temporal policy reload retry");

    presenter->policy_reload_retry_seconds =
        delay < 60U ? MIN(delay * 2U, 60U) : 60U;
}

static void on_policy_file_changed(gpointer user_data)
{
    SsHomeTemporalPresenter *presenter = user_data;

    if (presenter == NULL) {
        return;
    }
    if (!presenter_reload_policy(presenter)) {
        schedule_policy_reload_retry(presenter);
    } else {
        if (presenter->policy_reload_retry_id != 0U) {
            g_source_remove(presenter->policy_reload_retry_id);
            presenter->policy_reload_retry_id = 0U;
        }
        presenter->policy_reload_retry_seconds = 2U;
    }
}

static gboolean retry_policy_reload(gpointer user_data)
{
    SsHomeTemporalPresenter *presenter = user_data;

    if (presenter == NULL) {
        return G_SOURCE_REMOVE;
    }

    presenter->policy_reload_retry_id = 0U;
    if (presenter_reload_policy(presenter)) {
        presenter->policy_reload_retry_seconds = 2U;
        return G_SOURCE_REMOVE;
    }

    schedule_policy_reload_retry(presenter);
    return G_SOURCE_REMOVE;
}

SsHomeTemporalPresenter *ss_home_temporal_presenter_new(void)
{
    SsHomeTemporalPresenter *presenter =
        g_new0(SsHomeTemporalPresenter, 1);
    g_autofree gchar *policy_path = NULL;

    presenter->policy_reload_retry_seconds = 2U;
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
    presenter->policy_path = g_steal_pointer(&policy_path);
    if (!presenter->policy_valid) {
        schedule_policy_reload_retry(presenter);
    }
    presenter->policy_observer = ss_policy_file_observer_new(
        presenter->policy_path,
        on_policy_file_changed,
        presenter);
    if (presenter->policy_observer == NULL) {
        ss_home_temporal_presenter_free(presenter);
        return NULL;
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
    ss_policy_file_observer_free(presenter->policy_observer);
    presenter->policy_observer = NULL;
    if (presenter->policy_reload_retry_id != 0U) {
        g_source_remove(presenter->policy_reload_retry_id);
        presenter->policy_reload_retry_id = 0U;
    }
    g_clear_object(&presenter->settings);
    g_clear_pointer(&presenter->policy_path, g_free);
    g_clear_pointer(&presenter->cached_date_text, g_free);
    g_clear_pointer(&presenter->cached_calendar_id, g_free);
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
    return now != NULL && presenter_format_into(presenter, now, out);
}

guint ss_home_temporal_presenter_refresh_interval_ms(
    const SsHomeTemporalPresenter *presenter)
{
    /*
     * Seconds-enabled extended clocks can advance displayed units at rates
     * other than one SI second. Sample at 250 ms so decimal, Internet and
     * sidereal-style displays cannot skip visible units; seconds-disabled
     * presentation remains at the low-cost 1 Hz cadence. Date conversion is
     * cached independently, so the higher cadence only reformats live time.
     */
    if (presenter == NULL || !presenter->policy_valid ||
        !presenter->policy.show_seconds) {
        return 1000U;
    }
    if (g_strcmp0(presenter->policy.clock_mode, "standard") == 0 ||
        g_strcmp0(presenter->policy.clock_mode, "standard-12") == 0 ||
        g_strcmp0(presenter->policy.clock_mode, "standard-24") == 0) {
        return 1000U;
    }
    return 250U;
}
