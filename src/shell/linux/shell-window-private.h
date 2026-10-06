// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_SHELL_WINDOW_PRIVATE_H
#define SYSTEM_SETTINGS_SHELL_WINDOW_PRIVATE_H

#include "builtin-module-host.h"

#include <gtk/gtk.h>
#include <stdbool.h>

typedef struct ShellSearchState {
    GtkListBox *list;
    GtkWindow *parent;
    GtkStack *stack;
    GtkListBoxRow *home_row;
    GtkListBoxRow *date_row;
    GtkScrolledWindow *date_scroller;
    SsBuiltinModuleHost *module_host;
    bool date_time_loaded;
    gchar *query;
} ShellSearchState;

gboolean navigation_filter(GtkListBoxRow *row, gpointer user_data);
void on_search_changed(GtkSearchEntry *entry, gpointer user_data);
void ensure_date_time_panel(ShellSearchState *state);
void open_date_time(GtkButton *button, gpointer user_data);
void on_activate(GtkApplication *application, gpointer user_data);

#endif
