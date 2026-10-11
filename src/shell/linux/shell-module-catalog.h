// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_SHELL_MODULE_CATALOG_H
#define SYSTEM_SETTINGS_SHELL_MODULE_CATALOG_H

#include <glib.h>

typedef struct SsShellModuleMetadata {
    gchar *title;
    gchar *summary;
    gchar *icon;
    gchar *search_text;
} SsShellModuleMetadata;

void ss_shell_module_metadata_load(
    const char *manifest_filename,
    const char *page_id,
    SsShellModuleMetadata *out);
void ss_shell_module_metadata_clear(SsShellModuleMetadata *metadata);

#endif
