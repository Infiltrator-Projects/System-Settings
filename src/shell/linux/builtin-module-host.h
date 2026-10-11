// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_BUILTIN_MODULE_HOST_H
#define SYSTEM_SETTINGS_BUILTIN_MODULE_HOST_H

#include <gtk/gtk.h>
#include <stdbool.h>

typedef struct SsBuiltinModuleHost SsBuiltinModuleHost;

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

#endif
