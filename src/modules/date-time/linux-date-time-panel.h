// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_DATE_TIME_PANEL_H
#define SYSTEM_SETTINGS_LINUX_DATE_TIME_PANEL_H

#include <gtk/gtk.h>

typedef struct SsLinuxDateTimePanel SsLinuxDateTimePanel;

/*
 * This is a private built-in-module bridge, not the public module ABI.
 * GTK types are deliberately confined to the Linux shell/module integration
 * boundary while the eventual versioned public module ABI remains
 * toolkit-neutral.
 */
SsLinuxDateTimePanel *ss_linux_date_time_panel_new(
    GtkWindow *host_window,
    GError **error);
GtkWidget *ss_linux_date_time_panel_widget(
    SsLinuxDateTimePanel *panel);
void ss_linux_date_time_panel_free(gpointer data);

#endif
