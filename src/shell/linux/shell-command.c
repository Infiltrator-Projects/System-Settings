// SPDX-License-Identifier: GPL-3.0-or-later
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
/**
 * @file shell-command.c
 * @brief Trusted external-settings delegation shared by shell surfaces.
 */

#include "shell-command.h"

#include <glib/gstdio.h>
#include <stddef.h>
#include <stdlib.h>
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

static gboolean trusted_directory(const char *path)
{
    GStatBuf status;

    return path != NULL &&
           g_stat(path, &status) == 0 &&
           S_ISDIR(status.st_mode) &&
           status.st_uid == 0 &&
           mode_is_trusted(status.st_mode);
}

/*
 * realpath() removes every symlink component. Verify every directory in that
 * resolved path, not merely the directory in which the command name was
 * discovered. A distribution-managed symlink may therefore point elsewhere,
 * but no unprivileged writable directory can sit between / and the executable.
 */
static gboolean trusted_parent_chain(const char *resolved_path)
{
    g_autofree gchar *current = NULL;

    if (resolved_path == NULL || !g_path_is_absolute(resolved_path)) {
        return FALSE;
    }

    current = g_path_get_dirname(resolved_path);
    while (current != NULL) {
        g_autofree gchar *parent = NULL;

        if (!trusted_directory(current)) {
            return FALSE;
        }
        if (g_str_equal(current, G_DIR_SEPARATOR_S)) {
            return TRUE;
        }

        parent = g_path_get_dirname(current);
        if (parent == NULL || g_str_equal(parent, current)) {
            return FALSE;
        }
        g_free(current);
        current = g_steal_pointer(&parent);
    }
    return FALSE;
}

static gchar *trusted_system_executable(
    const char *directory,
    const char *candidate)
{
    GStatBuf executable_stat;
    char *resolved_native;
    gchar *resolved;

    if (directory == NULL || candidate == NULL ||
        !trusted_directory(directory)) {
        return NULL;
    }

    resolved_native = realpath(candidate, NULL);
    if (resolved_native == NULL) {
        return NULL;
    }
    resolved = g_strdup(resolved_native);
    free(resolved_native);
    if (resolved == NULL) {
        return NULL;
    }

    if (g_stat(resolved, &executable_stat) != 0 ||
        !S_ISREG(executable_stat.st_mode) ||
        executable_stat.st_uid != 0 ||
        !mode_is_trusted(executable_stat.st_mode) ||
        !trusted_parent_chain(resolved) ||
        !g_file_test(resolved, G_FILE_TEST_IS_EXECUTABLE)) {
        g_free(resolved);
        return NULL;
    }

    /*
     * Return the canonical path rather than the symlink-bearing discovery
     * path. With every parent directory root-owned and non-writable by
     * group/other users, an unprivileged process cannot redirect the checked
     * path between validation and g_subprocess_newv().
     */
    return resolved;
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
        gchar *resolved =
            trusted_system_executable(directories[index], candidate);
        if (resolved != NULL) {
            return resolved;
        }
    }
    return NULL;
}
