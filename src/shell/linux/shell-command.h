// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_SHELL_COMMAND_H
#define SYSTEM_SETTINGS_SHELL_COMMAND_H

#include <gtk/gtk.h>

gchar *find_trusted_system_program(const char *program);
gboolean launch_command(
    GtkWidget *source,
    const char *program,
    const char *argument);

#endif
