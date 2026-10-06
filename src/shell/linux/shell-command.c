// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file shell-command.c
 * @brief Trusted external-settings delegation shared by shell surfaces.
 */

#include "shell-command.h"

#include <stddef.h>
#include <string.h>

static void launch_command_finished(
    GObject *process,
    GAsyncResult *result,
    gpointer user_data)
{
    GtkWidget *source = GTK_WIDGET(user_data);
    g_autoptr(GError) error = NULL;
    const gboolean success = g_subprocess_wait_check_finish(
        G_SUBPROCESS(process), result, &error);

    if (!success && source != NULL) {
        gtk_widget_set_tooltip_text(
            source,
            error != NULL
                ? error->message
                : "The delegated settings tool exited without completing.");
    }
    if (source != NULL) {
        g_object_unref(source);
    }
}

gboolean launch_command(
    GtkWidget *source,
    const char *program,
    const char *argument)
{
    const char *argv[3] = {program, argument, NULL};
    g_autoptr(GError) error = NULL;
    g_autoptr(GSubprocess) process = NULL;

    if (program == NULL || program[0] == '\0') {
        return FALSE;
    }
    if (argument == NULL || argument[0] == '\0') {
        argv[1] = NULL;
    }

    process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_NONE, &error);
    if (process == NULL) {
        if (source != NULL && error != NULL) {
            gtk_widget_set_tooltip_text(source, error->message);
            gtk_widget_set_sensitive(source, FALSE);
        }
        return FALSE;
    }

    if (source != NULL) {
        g_subprocess_wait_check_async(
            process,
            NULL,
            launch_command_finished,
            g_object_ref(source));
    }
    return TRUE;
}

gchar *find_trusted_system_program(const char *program)
{
    static const char *const directories[] = {
        "/usr/bin",
        "/usr/sbin",
        "/bin",
        "/sbin",
        "/usr/local/bin",
        "/usr/local/sbin"
    };

    if (program == NULL || program[0] == '\0' ||
        strchr(program, '/') != NULL) {
        return NULL;
    }

    for (size_t index = 0U; index < G_N_ELEMENTS(directories); ++index) {
        g_autofree gchar *candidate =
            g_build_filename(directories[index], program, NULL);
        if (g_file_test(candidate, G_FILE_TEST_IS_EXECUTABLE)) {
            return g_steal_pointer(&candidate);
        }
    }
    return NULL;
}
