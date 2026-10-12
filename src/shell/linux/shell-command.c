// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file shell-command.c
 * @brief Trusted external-settings delegation shared by shell surfaces.
 */

#include "shell-command.h"

#include <glib/gstdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>

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

static gboolean mode_is_trusted(mode_t mode)
{
    return (mode & (S_IWGRP | S_IWOTH)) == 0;
}

static gboolean trusted_system_executable(
    const char *directory,
    const char *candidate)
{
    GStatBuf directory_stat;
    GStatBuf executable_stat;

    if (directory == NULL || candidate == NULL ||
        g_stat(directory, &directory_stat) != 0 ||
        !S_ISDIR(directory_stat.st_mode) ||
        directory_stat.st_uid != 0 ||
        !mode_is_trusted(directory_stat.st_mode) ||
        g_stat(candidate, &executable_stat) != 0 ||
        !S_ISREG(executable_stat.st_mode) ||
        executable_stat.st_uid != 0 ||
        !mode_is_trusted(executable_stat.st_mode)) {
        return FALSE;
    }

    /*
     * g_stat() follows a final symlink, allowing normal distribution-managed
     * command links while still requiring the resolved executable itself to be
     * root-owned and non-writable by group/other users. The containing search
     * directory is independently held to the same trust requirement.
     */
    return g_file_test(candidate, G_FILE_TEST_IS_EXECUTABLE);
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
        if (trusted_system_executable(directories[index], candidate)) {
            return g_steal_pointer(&candidate);
        }
    }
    return NULL;
}
