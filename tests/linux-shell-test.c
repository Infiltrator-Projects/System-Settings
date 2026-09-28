// SPDX-License-Identifier: GPL-3.0-or-later
/* Include the private shell to exercise its actual activation/close handlers. */
int ss_shell_entry(int argc, char **argv);
#define main ss_shell_entry
#include "../src/shell/linux/main.c"
#undef main
#include "linux-date-time-panel-private.h"
#include "system-settings/location-search.h"
#include <glib/gstdio.h>
#include <string.h>

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
        g_assert_nonnull(panel->policy_observer);
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

        if (i == 0) {
            /*
             * Exercise the actual GtkAccessible properties rather than merely
             * running under an accessibility backend. Removing these labels
             * must make CI fail.
             */
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(search),
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Search settings");
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(minimize),
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Minimize");
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(maximize),
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Maximize / Restore");
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(close),
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Close");
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(search_state->home_row),
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Home");
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(search_state->home_row),
                GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                "Overview & quick access");
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(search_state->date_row),
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Date & Time");
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(panel->location_search),
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Locality search");
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(panel->latitude),
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Latitude");
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(panel->manual_date),
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Manual date");
            gtk_test_accessible_assert_property(
                GTK_ACCESSIBLE(panel->status_label),
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Date and time status");


            GtkSettings *gtk_settings = gtk_settings_get_default();
            gboolean prefer_dark = FALSE;
            const unsigned int generation =
                ss_linux_theme_generation();
            g_assert_nonnull(gtk_settings);
            g_object_get(
                gtk_settings,
                "gtk-application-prefer-dark-theme",
                &prefer_dark,
                NULL);
            g_object_set(
                gtk_settings,
                "gtk-application-prefer-dark-theme",
                prefer_dark ? FALSE : TRUE,
                NULL);
            while (g_main_context_iteration(NULL, FALSE)) {
            }
            g_assert_cmpuint(
                ss_linux_theme_generation(), >, generation);
            g_object_set(
                gtk_settings,
                "gtk-application-prefer-dark-theme",
                prefer_dark,
                NULL);
            while (g_main_context_iteration(NULL, FALSE)) {
            }
        }
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
         * Validate against the actual logical desktop budget. The scale-2
         * regression runs a 1920x1080 Xvfb screen, which becomes a 960x540
         * logical desktop under GDK_SCALE=2.
         */
        const bool scale2 =
            g_strcmp0(g_getenv("GDK_SCALE"), "2") == 0;
        const int desktop_budget_width = scale2 ? 960 : 1024;
        const int desktop_budget_height = scale2 ? 540 : 768;
        int default_width = 0;
        int default_height = 0;
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
        {
            int stack_min = 0;
            int stack_nat = 0;
            int home_min = 0;
            int home_nat = 0;
            int date_min = 0;
            int date_nat = 0;
            gtk_widget_measure(
                GTK_WIDGET(stack),
                GTK_ORIENTATION_HORIZONTAL,
                -1,
                &stack_min,
                &stack_nat,
                NULL,
                NULL);
            gtk_widget_measure(
                GTK_WIDGET(home_scroller),
                GTK_ORIENTATION_HORIZONTAL,
                -1,
                &home_min,
                &home_nat,
                NULL,
                NULL);
            gtk_widget_measure(
                GTK_WIDGET(date_scroller),
                GTK_ORIENTATION_HORIZONTAL,
                -1,
                &date_min,
                &date_nat,
                NULL,
                NULL);
            g_test_message(
                "responsive widths: root=%d stack=%d home=%d date=%d",
                minimum_width, stack_min, home_min, date_min);
        }
        int title_min_width = 0;
        int title_nat_width = 0;
        int title_min_height = 0;
        int title_nat_height = 0;
        GtkWidget *titlebar = gtk_window_get_titlebar(window);
        g_assert_nonnull(titlebar);
        gtk_widget_measure(
            titlebar,
            GTK_ORIENTATION_HORIZONTAL,
            -1,
            &title_min_width,
            &title_nat_width,
            NULL,
            NULL);
        g_assert_cmpint(
            MAX(minimum_width, title_min_width),
            <=,
            desktop_budget_width);
        gtk_window_get_default_size(
            window, &default_width, &default_height);
        if (scale2) {
            /*
             * This exact regression represents a 1920x1080 desktop at 2x
             * scaling, so its logical default must fit inside 960x540.
             */
            g_assert_cmpint(default_width, <=, 960);
            g_assert_cmpint(default_height, <=, 540);
        } else {
            GdkDisplay *display =
                gtk_widget_get_display(GTK_WIDGET(window));
            GListModel *monitors =
                display != NULL
                    ? gdk_display_get_monitors(display)
                    : NULL;
            g_assert_nonnull(monitors);
            g_assert_cmpuint(g_list_model_get_n_items(monitors), >, 0U);
            GdkSurface *surface =
                gtk_native_get_surface(GTK_NATIVE(window));
            GdkMonitor *monitor = surface != NULL
                ? gdk_display_get_monitor_at_surface(display, surface)
                : NULL;
            g_autoptr(GdkMonitor) fallback_monitor = NULL;
            if (monitor == NULL) {
                fallback_monitor =
                    GDK_MONITOR(g_list_model_get_item(monitors, 0U));
                monitor = fallback_monitor;
            }
            GdkRectangle geometry = {0};
            g_assert_nonnull(monitor);
            gdk_monitor_get_geometry(monitor, &geometry);
            g_assert_cmpint(
                default_width,
                <=,
                MIN(1180, MAX(1, geometry.width * 9 / 10)));
            g_assert_cmpint(
                default_height,
                <=,
                MIN(760, MAX(1, geometry.height * 9 / 10)));
        }

        int minimum_height = 0;
        int natural_height = 0;
        gtk_widget_measure(
            window_child,
            GTK_ORIENTATION_VERTICAL,
            1024,
            &minimum_height,
            &natural_height,
            NULL,
            NULL);
        gtk_widget_measure(
            titlebar,
            GTK_ORIENTATION_VERTICAL,
            1024,
            &title_min_height,
            &title_nat_height,
            NULL,
            NULL);
        g_test_message(
            "responsive full-window minimum: %dx%d (content %d + titlebar %d)",
            MAX(minimum_width, title_min_width),
            minimum_height + title_min_height,
            minimum_height,
            title_min_height);
        g_assert_cmpint(
            minimum_height + title_min_height,
            <=,
            desktop_budget_height);
        g_assert_cmpuint(panel->timer_id, ==, 0U);

        HomeTemporalTicker *home_temporal = g_object_get_data(
            G_OBJECT(home_scroller),
            "system-settings-home-temporal-source");
        HomeStatusTicker *home_status = g_object_get_data(
            G_OBJECT(home_scroller),
            "system-settings-home-status-source");
        g_assert_nonnull(home_temporal);
        g_assert_nonnull(home_status);
        g_assert_cmpuint(home_temporal->source_id, !=, 0U);
        g_assert_cmpuint(home_status->source_id, !=, 0U);

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
        /* Avoid waiting for the production crossfade when testing map/unmap
         * source ownership; the lifecycle contract is independent of animation. */
        gtk_stack_set_transition_duration(stack, 0U);
        open_date_time(NULL, search_state);
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        g_assert_cmpstr(
            gtk_stack_get_visible_child_name(stack), ==, "date-time");
        g_assert_true(
            gtk_list_box_get_selected_row(navigation) ==
            search_state->date_row);
        g_assert_cmpuint(panel->timer_id, !=, 0U);
        g_assert_cmpuint(home_temporal->source_id, ==, 0U);
        g_assert_cmpuint(home_status->source_id, ==, 0U);
        gtk_stack_set_visible_child_name(stack, "home");
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        g_assert_cmpuint(panel->timer_id, ==, 0U);
        g_assert_cmpuint(home_temporal->source_id, !=, 0U);
        g_assert_cmpuint(home_status->source_id, !=, 0U);
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
        gtk_editable_set_text(GTK_EDITABLE(search), "seconds");
        on_search_changed(GTK_SEARCH_ENTRY(search), search_state);
        g_assert_true(navigation_filter(search_state->date_row, search_state));
        gtk_editable_set_text(GTK_EDITABLE(search), "positivist");
        on_search_changed(GTK_SEARCH_ENTRY(search), search_state);
        g_assert_true(navigation_filter(search_state->date_row, search_state));
        gtk_editable_set_text(GTK_EDITABLE(search), "");
        on_search_changed(GTK_SEARCH_ENTRY(search), search_state);

        if (i == 0) {
            /*
             * An external writer such as system-settings-time must become
             * authoritative in the already-open panel before any later GUI
             * edit can publish a stale whole-policy snapshot.
             */
            const SsTemporalPolicyStore *store =
                ss_platform_temporal_policy_store();
            InfiltratrTemporalPolicyV3 external =
                *ss_date_time_model_policy(&panel->model);
            const char *original_calendar = external.calendar;
            const char *replacement_calendar = NULL;
            for (size_t calendar_index = 0U;
                 calendar_index < infiltratr_temporal_calendar_count();
                 ++calendar_index) {
                const InfiltratrTemporalCalendarInfo *info =
                    infiltratr_temporal_calendar_at(calendar_index);
                if (info != NULL &&
                    g_strcmp0(info->id, original_calendar) != 0) {
                    replacement_calendar = info->id;
                    break;
                }
            }
            g_assert_nonnull(replacement_calendar);
            infiltratr_copy_string(
                external.calendar,
                sizeof(external.calendar),
                replacement_calendar);
            g_assert_true(store->save(&external));
            for (unsigned int attempt = 0U; attempt < 2000U; ++attempt) {
                while (g_main_context_iteration(NULL, FALSE)) {
                }
                if (g_strcmp0(
                        ss_date_time_model_policy(&panel->model)->calendar,
                        replacement_calendar) == 0) {
                    break;
                }
                g_usleep(1000U);
            }
            g_assert_cmpstr(
                ss_date_time_model_policy(&panel->model)->calendar,
                ==,
                replacement_calendar);

            /*
             * An unrelated external policy edit must not erase a wall-time
             * draft the user is typing. Only clock/calendar context changes
             * invalidate that draft.
             */
            gtk_editable_set_text(
                GTK_EDITABLE(panel->manual_date),
                "2026-09-28");
            gtk_editable_set_text(
                GTK_EDITABLE(panel->manual_time),
                "11:42");
            panel->manual_dirty = true;
            external = *ss_date_time_model_policy(&panel->model);
            external.show_seconds = !external.show_seconds;
            g_assert_true(store->save(&external));
            for (unsigned int attempt = 0U; attempt < 2000U; ++attempt) {
                while (g_main_context_iteration(NULL, FALSE)) {
                }
                if (ss_date_time_model_policy(&panel->model)->show_seconds ==
                    external.show_seconds) {
                    break;
                }
                g_usleep(1000U);
            }
            g_assert_true(panel->manual_dirty);
            g_assert_cmpstr(
                gtk_editable_get_text(GTK_EDITABLE(panel->manual_date)),
                ==,
                "2026-09-28");
            g_assert_cmpstr(
                gtk_editable_get_text(GTK_EDITABLE(panel->manual_time)),
                ==,
                "11:42");

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

            {
                SsLocationMetadata external_metadata = metadata;
                g_strlcpy(
                    external_metadata.display_name,
                    "Externally renamed locality",
                    sizeof(external_metadata.display_name));
                g_assert_true(
                    ss_location_metadata_save(&external_metadata));
                for (unsigned int attempt = 0U;
                     attempt < 2000U;
                     ++attempt) {
                    while (g_main_context_iteration(NULL, FALSE)) {
                    }
                    if (g_strcmp0(
                            panel->location_metadata.display_name,
                            external_metadata.display_name) == 0) {
                        break;
                    }
                    g_usleep(1000U);
                }
                g_assert_true(panel->location_metadata_present);
                g_assert_cmpstr(
                    panel->location_metadata.display_name,
                    ==,
                    "Externally renamed locality");
            }
            g_assert_true(ss_date_time_model_set_location(
                &panel->model, false, 0.0, 0.0));
            g_autofree gchar *metadata_file = g_build_filename(
                root, "infiltrator", "system-settings",
                "location.ini", NULL);
            g_assert_cmpint(g_remove(metadata_file), ==, 0);
            g_object_unref(row);

            /*
             * A metadata publication failure must restore the in-memory
             * metadata snapshot as well as the temporal policy.
             */
            SsLocationMetadata previous = {0};
            g_strlcpy(
                previous.display_name,
                "Previous locality",
                sizeof(previous.display_name));
            g_strlcpy(
                previous.country_code,
                "AU",
                sizeof(previous.country_code));
            previous.latitude = -36.0;
            previous.longitude = 145.0;
            g_assert_true(ss_location_metadata_save(&previous));
            panel->location_metadata = previous;
            panel->location_metadata_present = true;
            g_assert_true(ss_date_time_model_set_location(
                &panel->model, true, previous.latitude, previous.longitude));

            g_autofree gchar *metadata_backup =
                g_strconcat(metadata_file, ".saved", NULL);
            g_assert_cmpint(g_rename(metadata_file, metadata_backup), ==, 0);
            g_assert_cmpint(g_mkdir(metadata_file, 0700), ==, 0);

            panel->updating_controls = true;
            gtk_spin_button_set_value(panel->latitude, -37.0);
            gtk_spin_button_set_value(panel->longitude, 146.0);
            panel->updating_controls = false;
            on_location_coordinate_changed(NULL, NULL, panel);
            for (unsigned int attempt = 0U;
                 panel->coordinate_commit_id != 0U && attempt < 1000U;
                 ++attempt) {
                while (g_main_context_iteration(NULL, FALSE)) {
                }
                g_usleep(1000U);
            }
            g_assert_cmpuint(panel->coordinate_commit_id, ==, 0U);
            g_assert_true(panel->location_metadata_present);
            g_assert_cmpstr(
                panel->location_metadata.display_name,
                ==,
                "Previous locality");
            g_assert_cmpfloat_with_epsilon(
                panel->location_metadata.latitude,
                previous.latitude,
                0.000001);
            g_assert_cmpfloat_with_epsilon(
                panel->location_metadata.longitude,
                previous.longitude,
                0.000001);

            g_assert_cmpint(g_rmdir(metadata_file), ==, 0);
            g_assert_cmpint(g_rename(metadata_backup, metadata_file), ==, 0);
            g_assert_cmpint(g_remove(metadata_file), ==, 0);
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
