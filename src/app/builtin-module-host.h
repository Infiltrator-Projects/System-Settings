// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_BUILTIN_MODULE_HOST_H
#define SYSTEM_SETTINGS_BUILTIN_MODULE_HOST_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct SsBuiltinModuleHost SsBuiltinModuleHost;

SsBuiltinModuleHost *ss_builtin_module_host_new(GtkWindow *window);
void ss_builtin_module_host_free(SsBuiltinModuleHost *host);

/*
 * Built-in module discovery metadata. The generic shell uses this registry to
 * locate each module manifest without knowing any module-specific page ID,
 * title, icon or search terms. Runtime implementation callbacks remain private
 * to the composition adapter.
 */
size_t ss_builtin_module_host_module_count(void);
const char *ss_builtin_module_host_module_page_id(size_t index);
const char *ss_builtin_module_host_module_manifest(size_t index);

bool ss_builtin_module_host_ensure_page(
    SsBuiltinModuleHost *host,
    const char *page_id,
    GtkScrolledWindow *container,
    GError **error);
bool ss_builtin_module_host_is_loaded(
    const SsBuiltinModuleHost *host,
    const char *page_id);

#endif
