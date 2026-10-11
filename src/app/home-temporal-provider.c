// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file home-temporal-provider.c
 * @brief Composition-layer temporal presentation provider for the Home page.
 *
 * The generic built-in module host owns module lifecycle only. Home temporal
 * presentation is a separate composition concern, even though today's provider
 * is implemented by the Date & Time module's presenter.
 */

#include "home-temporal-provider.h"
#include "home-temporal-presenter.h"

struct SsHomeTemporalProvider {
    SsHomeTemporalPresenter *presenter;
};

SsHomeTemporalProvider *ss_home_temporal_provider_new(void)
{
    return g_new0(SsHomeTemporalProvider, 1);
}

const char *ss_home_temporal_provider_page_id(
    const SsHomeTemporalProvider *provider)
{
    return provider != NULL ? "date-time" : NULL;
}

void ss_home_temporal_provider_free(SsHomeTemporalProvider *provider)
{
    if (provider == NULL) {
        return;
    }
    ss_home_temporal_presenter_free(provider->presenter);
    g_free(provider);
}

void ss_home_temporal_snapshot_init(SsHomeTemporalSnapshot *snapshot)
{
    if (snapshot != NULL) {
        *snapshot = (SsHomeTemporalSnapshot){0};
    }
}

void ss_home_temporal_snapshot_clear(SsHomeTemporalSnapshot *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    g_clear_pointer(&snapshot->clock_text, g_free);
    g_clear_pointer(&snapshot->date_text, g_free);
    g_clear_pointer(&snapshot->system_time_text, g_free);
}

static SsHomeTemporalPresenter *ensure_presenter(
    SsHomeTemporalProvider *provider)
{
    if (provider == NULL) {
        return NULL;
    }
    if (provider->presenter == NULL) {
        provider->presenter = ss_home_temporal_presenter_new();
    }
    return provider->presenter;
}

bool ss_home_temporal_provider_format(
    SsHomeTemporalProvider *provider,
    SsHomeTemporalSnapshot *out)
{
    SsHomeTemporalPresenter *presenter;
    SsHomeTemporalPresentation presentation;

    if (out == NULL) {
        return false;
    }
    presenter = ensure_presenter(provider);
    if (presenter == NULL) {
        return false;
    }

    ss_home_temporal_presentation_init(&presentation);
    if (!ss_home_temporal_presenter_format_now(presenter, &presentation)) {
        ss_home_temporal_presentation_clear(&presentation);
        return false;
    }

    out->clock_text = g_steal_pointer(&presentation.clock_text);
    out->date_text = g_steal_pointer(&presentation.date_text);
    out->system_time_text = g_steal_pointer(&presentation.system_time_text);
    ss_home_temporal_presentation_clear(&presentation);
    return true;
}

guint ss_home_temporal_provider_refresh_interval_ms(
    SsHomeTemporalProvider *provider)
{
    SsHomeTemporalPresenter *presenter = ensure_presenter(provider);

    return presenter != NULL
        ? ss_home_temporal_presenter_refresh_interval_ms(presenter)
        : 1000U;
}
