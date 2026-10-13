// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_SHELL_WINDOW_H
#define SYSTEM_SETTINGS_SHELL_WINDOW_H

#include <gtk/gtk.h>

typedef struct SsShellActivationContext {
    /*
     * Optional manifest root supplied by an embedding/qualification harness.
     * Production bootstrap passes NULL, so installed/build-profile paths remain
     * the only runtime authorities. The pointer is borrowed only for the
     * synchronous activation call and is never retained by the window.
     */
    const char *module_manifest_directory;
} SsShellActivationContext;

/*
 * Stable shell entry used by the process bootstrap and black-box shell tests.
 * Module-private controller state is deliberately not exposed here.
 */
void ss_shell_window_activate(
    GtkApplication *application,
    gpointer user_data);

#endif
