// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_BUILTIN_MODULE_HOST_H
#define SYSTEM_SETTINGS_BUILTIN_MODULE_HOST_H

#include <gtk/gtk.h>
#include <stdbool.h>

typedef struct SsBuiltinModuleHost SsBuiltinModuleHost;

typedef struct SsHomeTemporalSnapshot {
    gchar *clock_text;
    gchar *date_text;
    gchar *system_time_text;
} SsHomeTemporalSnapshot;

SsBuiltinModuleHost *ss_builtin_module_host_new(GtkWindow *window);
void ss_builtin_module_host_free(SsBuiltinModuleHost *host);

bool ss_builtin_module_host_ensure_page(
    SsBuiltinModuleHost *host,
    const char *page_id,
    GtkScrolledWindow *container,
    GError **error);
bool ss_builtin_module_host_is_loaded(
    const SsBuiltinModuleHost *host,
    const char *page_id);

void ss_home_temporal_snapshot_init(SsHomeTemporalSnapshot *snapshot);
void ss_home_temporal_snapshot_clear(SsHomeTemporalSnapshot *snapshot);
bool ss_builtin_module_host_format_home_temporal(
    SsBuiltinModuleHost *host,
    SsHomeTemporalSnapshot *out);
guint ss_builtin_module_host_home_temporal_refresh_interval_ms(
    SsBuiltinModuleHost *host);

#endif
