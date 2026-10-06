// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main.c
 * @brief Native Linux System Settings process entry point.
 *
 * Application/window construction lives in shell-window.c. Keeping the entry
 * point intentionally small prevents product UI and module ownership from
 * accumulating in the process bootstrap again.
 */

#include "home-page-private.h"
#include "linux-theme.h"
#include "shell-command.h"
#include "shell-window-private.h"

#include "system-settings/project-info.h"

#include <gtk/gtk.h>

#if defined(SYSTEM_SETTINGS_UI_SOURCE_DIR) && \
    defined(SYSTEM_SETTINGS_MODULE_SOURCE_DIR)
/*
 * linux-shell-test textually includes this bootstrap and its target supplies
 * source-tree fixture paths. Set the runtime overrides before test main() so
 * the linked shell library can keep the exact production path policy used by
 * both PGO training and the final native build.
 */
static void configure_shell_test_runtime_paths(void)
    __attribute__((constructor));

static void configure_shell_test_runtime_paths(void)
{
    (void)g_setenv(
        "SYSTEM_SETTINGS_UI_ASSET_DIR_OVERRIDE",
        SYSTEM_SETTINGS_UI_SOURCE_DIR,
        TRUE);
    (void)g_setenv(
        "SYSTEM_SETTINGS_MODULE_DIR_OVERRIDE",
        SYSTEM_SETTINGS_MODULE_SOURCE_DIR,
        TRUE);
}
#endif

int main(int argc, char **argv)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    g_autoptr(GtkApplication) application = gtk_application_new(
        info->application_id,
        G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(
        application,
        "activate",
        G_CALLBACK(on_activate),
        NULL);
    return g_application_run(
        G_APPLICATION(application), argc, argv);
}
