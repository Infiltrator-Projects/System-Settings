// SPDX-License-Identifier: GPL-3.0-or-later
/* Include the private shell to exercise its actual activation/close handlers. */
int ss_shell_entry(int argc, char **argv);
#define main ss_shell_entry
#include "../src/shell/linux/main.c"
#undef main
#include "linux-date-time-panel-private.h"
#include <glib/gstdio.h>

static gboolean end_wait(gpointer data)
{
    g_main_loop_quit(data);
    return G_SOURCE_REMOVE;
}

int main(void)
{
    g_autofree char *root = g_dir_make_tmp("ss-shell-XXXXXX", NULL);
    g_assert_nonnull(root);
    g_setenv("XDG_CONFIG_HOME", root, TRUE);
    g_setenv("GSETTINGS_BACKEND", "memory", TRUE);
    /* No requests can reach the host's protected time service. */
    g_setenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/nonexistent/ss-test-bus", TRUE);
    gtk_init();
    g_autoptr(GtkApplication) app = gtk_application_new(
        "org.infiltrator.SystemSettings.Test", G_APPLICATION_NON_UNIQUE);
    g_assert_true(g_application_register(G_APPLICATION(app), NULL, NULL));
    for (int i = 0; i < 8; ++i) {
        on_activate(app, NULL);
        GtkWindow *window = gtk_application_get_active_window(app);
        g_assert_nonnull(window);
        g_object_ref(window);
        on_activate(app, NULL);
        g_assert_cmpuint(g_list_length(gtk_application_get_windows(app)), ==, 1U);
        SsLinuxDateTimePanel *panel = g_object_get_data(
            G_OBJECT(window), "system-settings-date-time-panel");
        g_assert_nonnull(panel);
        GtkDropDown *dropdowns[] = {panel->timezone, panel->clock_mode,
                                    panel->calendar, panel->first_day};
        for (size_t j = 0; j < G_N_ELEMENTS(dropdowns); ++j) {
            GListModel *model = gtk_drop_down_get_model(dropdowns[j]);
            g_assert_true(G_IS_LIST_MODEL(model));
            g_assert_cmpuint(g_list_model_get_n_items(model), >, 0U);
            /* Exercise items after construction; the original ownership bug
             * could survive first paint but fail here or on destruction. */
            g_autoptr(GObject) item = g_list_model_get_item(model, 0U);
            g_assert_nonnull(item);
        }
        gtk_window_close(window);
        g_assert_null(g_object_get_data(G_OBJECT(window), "system-settings-date-time-panel"));
        g_object_unref(window);
        g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);
        g_timeout_add(30U, end_wait, loop);
        g_main_loop_run(loop);
    }
    g_rmdir(root);
    return 0;
}
