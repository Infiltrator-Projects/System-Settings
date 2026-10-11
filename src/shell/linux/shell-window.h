// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_SHELL_WINDOW_H
#define SYSTEM_SETTINGS_SHELL_WINDOW_H

#include <gtk/gtk.h>

/*
 * Stable shell entry used by the process bootstrap and black-box shell tests.
 * Module-private controller state is deliberately not exposed here.
 */
void ss_shell_window_activate(
    GtkApplication *application,
    gpointer user_data);

#endif
