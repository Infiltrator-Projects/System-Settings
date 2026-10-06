// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-builtin-module-host.c
 * @brief Composition boundary between the generic Linux shell and built-in modules.
 *
 * This is deliberately the only production translation unit outside the
 * Date & Time module that knows its private GTK bridge. The shell consumes the
 * generic host API and therefore no longer constructs module-private objects.
 */

#include "builtin-module-host.h"

#include "home-temporal-presentation.h"
#include "linux-date-time-panel.h"

#include <gio/gio.h>

#define SS_DATE_TIME_PAGE_ID "date-time"
#define SS_DATE_TIME_PANEL_DATA_KEY "system-settings-date-time-panel"

struct SsBuiltinModuleHost {
    GtkWindow *window;
    SsLinuxDateTimePanel *date_time;
    SsHomeTemporalPresenter *home_temporal;
};

static bool is_date_time_page(const char *page_id)
{
    return g_strcmp0(page_id, SS_DATE_TIME_PAGE_ID) == 0;
}

SsBuiltinModuleHost *ss_builtin_module_host_new(GtkWindow *window)
{
    SsBuiltinModuleHost *host;

    if (window == NULL) {
        return NULL;
    }
    host = g_new0(SsBuiltinModuleHost, 1);
    host->window = window;
    return host;
}

void ss_builtin_module_host_free(SsBuiltinModuleHost *host)
{
    if (host == NULL) {
        return;
    }

    if (host->window != NULL &&
        g_object_get_data(
            G_OBJECT(host->window), SS_DATE_TIME_PANEL_DATA_KEY) ==
            host->date_time) {
        /*
         * Pending module replies retain the window and re-resolve this key
         * before touching module state. Clear it before destroying the panel.
         */
        g_object_set_data(
            G_OBJECT(host->window), SS_DATE_TIME_PANEL_DATA_KEY, NULL);
    }

    ss_linux_date_time_panel_free(host->date_time);
    ss_home_temporal_presenter_free(host->home_temporal);
    g_free(host);
}

bool ss_builtin_module_host_ensure_page(
    SsBuiltinModuleHost *host,
    const char *page_id,
    GtkScrolledWindow *container,
    GError **error)
{
    GtkWidget *panel_widget;

    if (host == NULL || container == NULL) {
        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_INVALID_ARGUMENT,
            "Invalid built-in module host or page container.");
        return false;
    }
    if (!is_date_time_page(page_id)) {
        g_set_error(
            error,
            G_IO_ERROR,
            G_IO_ERROR_NOT_SUPPORTED,
            "Unknown built-in System Settings page: %s",
            page_id != NULL ? page_id : "(null)");
        return false;
    }

    if (host->date_time == NULL) {
        host->date_time = ss_linux_date_time_panel_new(host->window, error);
        if (host->date_time == NULL) {
            return false;
        }
        /* Non-owning compatibility/lifetime lookup used by async callbacks. */
        g_object_set_data(
            G_OBJECT(host->window),
            SS_DATE_TIME_PANEL_DATA_KEY,
            host->date_time);
    }

    panel_widget = ss_linux_date_time_panel_widget(host->date_time);
    if (panel_widget == NULL) {
        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_FAILED,
            "The built-in Date & Time module did not provide a panel.");
        return false;
    }

    if (gtk_scrolled_window_get_child(container) != panel_widget) {
        gtk_scrolled_window_set_child(container, panel_widget);
    }
    return true;
}

bool ss_builtin_module_host_is_loaded(
    const SsBuiltinModuleHost *host,
    const char *page_id)
{
    return host != NULL && is_date_time_page(page_id) &&
           host->date_time != NULL;
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

static SsHomeTemporalPresenter *ensure_home_temporal_presenter(
    SsBuiltinModuleHost *host)
{
    if (host == NULL) {
        return NULL;
    }
    if (host->home_temporal == NULL) {
        host->home_temporal = ss_home_temporal_presenter_new();
    }
    return host->home_temporal;
}

bool ss_builtin_module_host_format_home_temporal(
    SsBuiltinModuleHost *host,
    SsHomeTemporalSnapshot *out)
{
    SsHomeTemporalPresenter *presenter;
    SsHomeTemporalPresentation presentation;

    if (out == NULL) {
        return false;
    }
    presenter = ensure_home_temporal_presenter(host);
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

guint ss_builtin_module_host_home_temporal_refresh_interval_ms(
    SsBuiltinModuleHost *host)
{
    SsHomeTemporalPresenter *presenter =
        ensure_home_temporal_presenter(host);

    return presenter != NULL
        ? ss_home_temporal_presenter_refresh_interval_ms(presenter)
        : 1000U;
}
