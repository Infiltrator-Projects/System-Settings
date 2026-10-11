// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_SHELL_WINDOW_PRIVATE_H
#define SYSTEM_SETTINGS_SHELL_WINDOW_PRIVATE_H

#include "builtin-module-host.h"
#include "shell-window.h"

#include <gtk/gtk.h>
#include <stdbool.h>

typedef struct ShellModulePage {
    gchar *page_id;
    gchar *title;
    GtkListBoxRow *row;
    GtkScrolledWindow *container;
} ShellModulePage;

typedef struct ShellSearchState {
    GtkListBox *list;
    GtkWindow *parent;
    GtkStack *stack;
    GtkListBoxRow *home_row;
    SsBuiltinModuleHost *module_host;
    GHashTable *module_pages;
    const char *home_primary_page_id;

    GtkListBoxRow *primary_module_row;
    GtkScrolledWindow *primary_module_container;
    bool primary_module_loaded;

    gchar *query;
} ShellSearchState;

gboolean navigation_filter(GtkListBoxRow *row, gpointer user_data);
void on_search_changed(GtkSearchEntry *entry, gpointer user_data);
bool shell_open_page(ShellSearchState *state, const char *page_id);


#endif
