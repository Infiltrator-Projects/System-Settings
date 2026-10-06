// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_SHELL_WINDOW_PRIVATE_H
#define SYSTEM_SETTINGS_SHELL_WINDOW_PRIVATE_H

#include "builtin-module-host.h"

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

    /*
     * Transitional aliases keep the existing white-box regression test source
     * buildable while runtime routing is generic. New shell code must use the
     * primary_module_* names or module_pages, never the Date & Time aliases.
     */
    union {
        GtkListBoxRow *primary_module_row;
        GtkListBoxRow *date_row;
    };
    union {
        GtkScrolledWindow *primary_module_container;
        GtkScrolledWindow *date_scroller;
    };
    union {
        bool primary_module_loaded;
        bool date_time_loaded;
    };

    gchar *query;
} ShellSearchState;

gboolean navigation_filter(GtkListBoxRow *row, gpointer user_data);
void on_search_changed(GtkSearchEntry *entry, gpointer user_data);
bool shell_open_page(ShellSearchState *state, const char *page_id);

/* Transitional test/source compatibility wrappers. Runtime routing is generic. */
void ensure_date_time_panel(ShellSearchState *state);
void open_date_time(GtkButton *button, gpointer user_data);

void on_activate(GtkApplication *application, gpointer user_data);

#endif
