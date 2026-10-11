// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main.c
 * @brief Native Linux System Settings process entry point.
 */

#include "shell-window.h"
#include "system-settings/project-info.h"

#include <gtk/gtk.h>

int main(int argc, char **argv)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    g_autoptr(GtkApplication) application = gtk_application_new(
        info->application_id,
        G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(
        application,
        "activate",
        G_CALLBACK(ss_shell_window_activate),
        NULL);
    return g_application_run(
        G_APPLICATION(application), argc, argv);
}
