// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-temporal-presentation.c
 * @brief Authoritative temporal formatting for the Home dashboard.
 *
 * The Date & Time module owns policy interpretation. The generic shell asks
 * this bridge for display strings rather than formatting civil time itself.
 */
#include "home-temporal-presentation.h"
#include "calendar-preview-provider.h"

#include "system-settings/cinnamon-interface.h"
#include "system-settings/native-clock-policy.h"
#include "system-settings/temporal-policy-store.h"

#include <string.h>

static bool desktop_uses_24h(void)
{
    g_autoptr(GSettings) settings =
        ss_cinnamon_interface_settings_new();
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

    if (policy == NULL || now == NULL) {
        return NULL;
    }

    effective_mode = policy->clock_mode;
    if (strcmp(effective_mode, "standard") == 0) {
        effective_mode = ss_native_clock_mode_id(use_24h);
    }

    mode = infiltratr_temporal_clock_mode_find(effective_mode);
    if (mode == NULL) {
        return NULL;
    }
    if ((mode->requires_latitude || mode->requires_longitude) &&
        !policy->location_configured) {
        return g_strdup_printf("%s — location required", mode->name);
    }

    unix_us = g_date_time_to_unix(now) * G_USEC_PER_SEC +
        g_date_time_get_microsecond(now);
    offset_us = g_date_time_get_utc_offset(now);
    if (!infiltratr_temporal_format_clock_mode(
            effective_mode,
            unix_us,
            (int32_t)(offset_us / G_USEC_PER_SEC),
            policy->show_seconds,
            false,
            policy->location_configured,
            policy->latitude,
            policy->longitude,
            buffer,
            sizeof(buffer),
            NULL)) {
        return NULL;
    }
    return g_strdup(buffer);
}

static gchar *format_date(
    const InfiltratrTemporalPolicyV3 *policy,
    GDateTime *now)
{
    const InfiltratrTemporalCalendarInfo *calendar;
    SsCalendarPreviewProvider *provider;
    gchar *formatted;

    if (policy == NULL || now == NULL) {
        return NULL;
    }

    if (strcmp(policy->calendar, "gregorian") == 0) {
        return g_date_time_format(now, "%A, %e %B %Y");
    }

    calendar = infiltratr_temporal_calendar_find(policy->calendar);
    if (calendar == NULL) {
        return NULL;
    }

    provider = ss_calendar_preview_provider_new();
    formatted = ss_calendar_preview_provider_format_date(
        provider,
        policy->calendar,
        g_date_time_get_year(now),
        g_date_time_get_month(now),
        g_date_time_get_day_of_month(now));
    ss_calendar_preview_provider_free(provider);

    if (formatted != NULL) {
        return formatted;
    }
    return g_strdup_printf("%s — preview unavailable", calendar->name);
}

void ss_home_temporal_presentation_clear(
    SsHomeTemporalPresentation *presentation)
{
    if (presentation == NULL) {
        return;
    }
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
    SsHomeTemporalPresentation candidate = {0};

    if (policy == NULL || now == NULL || out == NULL) {
        return false;
    }

    candidate.clock_text = format_clock(policy, now, use_24h);
    candidate.date_text = format_date(policy, now);
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

bool ss_home_temporal_presentation_now(
    SsHomeTemporalPresentation *out)
{
    const SsTemporalPolicyStore *store;
    InfiltratrTemporalPolicyV3 policy;
    g_autoptr(GDateTime) now = NULL;
    bool found = false;

    if (out == NULL) {
        return false;
    }
    ss_home_temporal_presentation_clear(out);

    if (!infiltratr_temporal_policy_v3_default(&policy)) {
        return false;
    }

    store = ss_platform_temporal_policy_store();
    if (store != NULL && store->load != NULL) {
        InfiltratrTemporalPolicyV3 loaded;
        if (infiltratr_temporal_policy_v3_default(&loaded) &&
            store->load(&loaded, &found)) {
            policy = loaded;
        }
    }

    now = g_date_time_new_now_local();
    if (now == NULL) {
        return false;
    }
    return ss_home_temporal_presentation_format(
        &policy, now, desktop_uses_24h(), out);
}
