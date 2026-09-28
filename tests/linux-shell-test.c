// SPDX-License-Identifier: GPL-3.0-or-later
/* Include the private shell to exercise its actual activation/close handlers. */
int ss_shell_entry(int argc, char **argv);
#define main ss_shell_entry
#include "../src/shell/linux/main.c"
#undef main
#include "linux-date-time-panel-private.h"
#include "system-settings/location-search.h"
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
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        GtkWindow *window = gtk_application_get_active_window(app);
        g_assert_nonnull(window);
        g_object_ref(window);
        on_activate(app, NULL);
        g_assert_cmpuint(g_list_length(gtk_application_get_windows(app)), ==, 1U);
        SsLinuxDateTimePanel *panel = g_object_get_data(
            G_OBJECT(window), "system-settings-date-time-panel");
        g_assert_nonnull(panel);
        GtkStack *stack = g_object_get_data(
            G_OBJECT(window), "system-settings-stack");
        g_assert_true(GTK_IS_STACK(stack));
        GtkListBox *navigation = g_object_get_data(
            G_OBJECT(window), "system-settings-navigation-list");
        g_assert_true(GTK_IS_LIST_BOX(navigation));
        GtkWidget *header_end = g_object_get_data(
            G_OBJECT(window), "system-settings-header-end");
        GtkWidget *search = g_object_get_data(
            G_OBJECT(window), "system-settings-search-entry");
        GtkWidget *minimize = g_object_get_data(
            G_OBJECT(window), "system-settings-minimize-button");
        GtkWidget *maximize = g_object_get_data(
            G_OBJECT(window), "system-settings-maximize-button");
        GtkWidget *close = g_object_get_data(
            G_OBJECT(window), "system-settings-close-button");
        g_assert_true(GTK_IS_BOX(header_end));
        g_assert_true(GTK_IS_SEARCH_ENTRY(search));
        g_assert_true(GTK_IS_BUTTON(minimize));
        g_assert_true(GTK_IS_BUTTON(maximize));
        g_assert_true(GTK_IS_BUTTON(close));
        g_assert_true(gtk_widget_get_focusable(minimize));
        g_assert_true(gtk_widget_get_focusable(maximize));
        g_assert_true(gtk_widget_get_focusable(close));
        g_assert_true(gtk_widget_get_first_child(header_end) == search);
        g_assert_true(gtk_widget_get_next_sibling(search) == minimize);
        g_assert_true(gtk_widget_get_next_sibling(minimize) == maximize);
        g_assert_true(gtk_widget_get_next_sibling(maximize) == close);
        g_assert_null(gtk_widget_get_next_sibling(close));
        GtkScrolledWindow *nav_scroller = g_object_get_data(
            G_OBJECT(window), "system-settings-navigation-scroller");
        GtkScrolledWindow *date_scroller = g_object_get_data(
            G_OBJECT(window), "system-settings-date-scroller");
        GtkScrolledWindow *home_scroller = g_object_get_data(
            G_OBJECT(stack), "system-settings-home-scroller");
        g_assert_true(GTK_IS_SCROLLED_WINDOW(nav_scroller));
        g_assert_true(GTK_IS_SCROLLED_WINDOW(date_scroller));
        g_assert_true(GTK_IS_SCROLLED_WINDOW(home_scroller));
        g_assert_false(gtk_scrolled_window_get_overlay_scrolling(nav_scroller));
        g_assert_false(gtk_scrolled_window_get_overlay_scrolling(date_scroller));
        g_assert_false(gtk_scrolled_window_get_overlay_scrolling(home_scroller));
        g_assert_nonnull(g_object_get_data(
            G_OBJECT(home_scroller),
            "system-settings-home-temporal-source"));
        g_assert_nonnull(g_object_get_data(
            G_OBJECT(home_scroller),
            "system-settings-home-status-source"));

        /*
         * A 1024-wide desktop must be able to satisfy the shell's minimum
         * allocation without horizontal scrolling. Home uses wrapping
         * FlowBoxes and Date & Time starts its fast preview only while mapped.
         */
        int minimum_width = 0;
        int natural_width = 0;
        GtkWidget *window_child = gtk_window_get_child(window);
        gtk_widget_measure(
            window_child,
            GTK_ORIENTATION_HORIZONTAL,
            -1,
            &minimum_width,
            &natural_width,
            NULL,
            NULL);
        g_assert_cmpint(minimum_width, <=, 1024);
        g_assert_cmpuint(panel->timer_id, ==, 0U);

        guint navigation_rows = 0U;
        for (GtkWidget *row = gtk_widget_get_first_child(GTK_WIDGET(navigation));
             row != NULL;
             row = gtk_widget_get_next_sibling(row)) {
            navigation_rows++;
        }
        g_assert_cmpuint(navigation_rows, >=, 13U);
        GtkWidget *home = gtk_stack_get_child_by_name(stack, "home");
        g_assert_nonnull(home);
        g_assert_true(GTK_IS_SCROLLED_WINDOW(home));
        g_assert_cmpstr(
            gtk_stack_get_visible_child_name(stack), ==, "home");
        ShellSearchState *search_state = g_object_get_data(
            G_OBJECT(window), "system-settings-search-state");
        g_assert_nonnull(search_state);
        open_date_time(NULL, search_state);
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        g_assert_cmpstr(
            gtk_stack_get_visible_child_name(stack), ==, "date-time");
        g_assert_true(
            gtk_list_box_get_selected_row(navigation) ==
            search_state->date_row);
        g_assert_cmpuint(panel->timer_id, !=, 0U);
        gtk_stack_set_visible_child_name(stack, "home");
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        g_assert_cmpuint(panel->timer_id, ==, 0U);
        gtk_editable_set_text(GTK_EDITABLE(search), "theme");
        on_search_changed(GTK_SEARCH_ENTRY(search), search_state);
        g_assert_cmpstr(search_state->query, ==, "theme");
        g_assert_false(navigation_filter(search_state->home_row, search_state));
        GtkListBoxRow *appearance_row = GTK_LIST_BOX_ROW(
            gtk_widget_get_next_sibling(
                gtk_widget_get_next_sibling(
                    gtk_widget_get_next_sibling(
                        GTK_WIDGET(search_state->home_row)))));
        g_assert_true(navigation_filter(appearance_row, search_state));
        gtk_editable_set_text(GTK_EDITABLE(search), "");
        on_search_changed(GTK_SEARCH_ENTRY(search), search_state);

        if (i == 0) {
            /*
             * Locality-to-zone inference is advisory. With the system service
             * deliberately unavailable, selecting Mooroopna must persist the
             * locality but must not persist the nearest Melbourne zone as if
             * the OS had accepted it.
             */
            GtkWidget *row = gtk_list_box_row_new();
            SsLocationSearchResult *candidate =
                g_new0(SsLocationSearchResult, 1);
            g_strlcpy(
                candidate->display_name,
                "Mooroopna, Victoria, Australia",
                sizeof(candidate->display_name));
            g_strlcpy(
                candidate->country_code,
                "AU",
                sizeof(candidate->country_code));
            candidate->latitude = -36.3949;
            candidate->longitude = 145.3610;
            g_object_set_data_full(
                G_OBJECT(row),
                "ss-location-result",
                candidate,
                g_free);
            g_object_ref_sink(row);
            on_location_result_activated(
                NULL, GTK_LIST_BOX_ROW(row), panel);

            SsLocationMetadata metadata;
            g_assert_true(ss_location_metadata_load(&metadata));
            g_assert_cmpstr(metadata.timezone_id, ==, "");
            g_assert_nonnull(strstr(
                gtk_label_get_text(GTK_LABEL(panel->status_label)),
                "Suggested time zone: Australia/Melbourne"));
            g_assert_true(ss_date_time_model_set_location(
                &panel->model, false, 0.0, 0.0));
            g_autofree gchar *metadata_file = g_build_filename(
                root, "infiltrator", "system-settings",
                "location.ini", NULL);
            g_assert_cmpint(g_remove(metadata_file), ==, 0);
            g_object_unref(row);
        }

        GtkWidget *missing_visual = make_visual_panel(
            "definitely-missing.png", 80, 40, "test-visual");
        g_assert_true(GTK_IS_BOX(missing_visual));
        g_assert_false(GTK_IS_DRAWING_AREA(missing_visual));
        g_object_ref_sink(missing_visual);
        g_object_unref(missing_visual);

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
