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
    const bool accessibility_enabled =
        g_strcmp0(g_getenv("GTK_A11Y"), "none") != 0;
    g_autoptr(GtkApplication) app = gtk_application_new(
        "org.infiltrator.SystemSettings.Test", G_APPLICATION_NON_UNIQUE);
    unsigned int settled_theme_generation = 0U;
    g_assert_true(g_application_register(G_APPLICATION(app), NULL, NULL));
    for (int i = 0; i < 8; ++i) {
        on_activate(app, NULL);
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        if (i > 0) {
            g_assert_cmpuint(
                ss_linux_theme_generation(), ==, settled_theme_generation);
        }
        GtkWindow *window = gtk_application_get_active_window(app);
        g_assert_nonnull(window);
        g_object_ref(window);
        on_activate(app, NULL);
        g_assert_cmpuint(g_list_length(gtk_application_get_windows(app)), ==, 1U);
        ShellSearchState *runtime_search_state = g_object_get_data(
            G_OBJECT(window), "system-settings-search-state");
        g_assert_nonnull(runtime_search_state);
        g_assert_false(runtime_search_state->date_time_loaded);
        g_assert_null(g_object_get_data(
            G_OBJECT(window), "system-settings-date-time-panel"));
        ensure_date_time_panel(runtime_search_state);
        SsLinuxDateTimePanel *panel = g_object_get_data(
            G_OBJECT(window), "system-settings-date-time-panel");
        g_assert_true(runtime_search_state->date_time_loaded);
        g_assert_nonnull(panel);
        g_assert_nonnull(panel->policy_observer);
        /* Compact binary controls must never inherit row-width expansion. */
        g_assert_false(gtk_widget_compute_expand(
            GTK_WIDGET(panel->show_seconds), GTK_ORIENTATION_HORIZONTAL));
        g_assert_false(gtk_widget_compute_expand(
            GTK_WIDGET(panel->show_date), GTK_ORIENTATION_HORIZONTAL));
        {
            int switch_width = 0;
            int switch_height = 0;
            gtk_widget_get_size_request(
                GTK_WIDGET(panel->show_seconds),
                &switch_width,
                &switch_height);
            g_assert_cmpint(switch_width, ==, 38);
            g_assert_cmpint(switch_height, ==, 20);
        }
        /* Latitude and Longitude are one paired control and must share a grid. */
        GtkWidget *latitude_field = gtk_widget_get_parent(GTK_WIDGET(panel->latitude));
        GtkWidget *longitude_field = gtk_widget_get_parent(GTK_WIDGET(panel->longitude));
        g_assert_nonnull(latitude_field);
        g_assert_nonnull(longitude_field);
        g_assert_true(GTK_IS_GRID(gtk_widget_get_parent(latitude_field)));
        g_assert_true(
            gtk_widget_get_parent(latitude_field) ==
            gtk_widget_get_parent(longitude_field));
        if (i == 0) {
            InfiltratrTemporalPolicyV3 saved_policy = panel->model.policy;
            infiltratr_copy_string(
                panel->model.policy.clock_mode,
                sizeof(panel->model.policy.clock_mode),
                "standard-24");
            panel->model.policy.show_seconds = true;
            g_assert_cmpuint(
                ss_linux_date_time_panel_preview_interval_ms(panel),
                ==,
                1000U);
            infiltratr_copy_string(
                panel->model.policy.clock_mode,
                sizeof(panel->model.policy.clock_mode),
                "decimal");
            g_assert_cmpuint(
                ss_linux_date_time_panel_preview_interval_ms(panel),
                ==,
                250U);
            panel->model.policy = saved_policy;
        }
        GtkStack *stack = g_object_get_data(
            G_OBJECT(window), "system-settings-stack");
        g_assert_true(GTK_IS_STACK(stack));
        GtkListBox *navigation = g_object_get_data(
            G_OBJECT(window), "system-settings-navigation-list");
        g_assert_true(GTK_IS_LIST_BOX(navigation));
        g_assert_cmpuint(
            gtk_stack_get_transition_duration(stack), ==, 110U);
        if (i == 0) {
            g_autofree gchar *trusted_shell =
                find_trusted_system_program("sh");
            g_assert_nonnull(trusted_shell);
            g_assert_true(g_path_is_absolute(trusted_shell));
            g_assert_null(find_trusted_system_program("../sh"));
        }
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

        if (accessibility_enabled && i == 0) {
            gtk_test_accessible_assert_property(
                search, GTK_ACCESSIBLE_PROPERTY_LABEL, "Search settings");
            gtk_test_accessible_assert_property(
                minimize, GTK_ACCESSIBLE_PROPERTY_LABEL, "Minimize");
            gtk_test_accessible_assert_property(
                maximize, GTK_ACCESSIBLE_PROPERTY_LABEL, "Maximize / Restore");
            gtk_test_accessible_assert_property(
                close, GTK_ACCESSIBLE_PROPERTY_LABEL, "Close");
            gtk_test_accessible_assert_property(
                panel->location_search,
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Locality search");
            gtk_test_accessible_assert_property(
                panel->location_search_button,
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Search for locality");
            gtk_test_accessible_assert_property(
                panel->latitude, GTK_ACCESSIBLE_PROPERTY_LABEL, "Latitude");
            gtk_test_accessible_assert_property(
                panel->longitude, GTK_ACCESSIBLE_PROPERTY_LABEL, "Longitude");
            gtk_test_accessible_assert_property(
                panel->manual_date,
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Manual date");
            gtk_test_accessible_assert_property(
                panel->manual_time,
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Manual time");
            gtk_test_accessible_assert_property(
                panel->manual_set_time,
                GTK_ACCESSIBLE_PROPERTY_LABEL,
                "Set operating-system date and time");
            /*
             * The status GtkLabel intentionally exposes its current message as
             * its accessible name. Its value is dynamic, so assert the stable
             * interactive-control names above rather than pinning status text.
             */
        }

        if (i == 0) {
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
            settled_theme_generation = ss_linux_theme_generation();
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
        GtkWidget *home_hero_geometry = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-hero");
        GtkWidget *home_features_geometry = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-features");
        GtkWidget *home_grid_geometry = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-grid");
        GtkWidget *home_overview_geometry = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-overview");
        GtkWidget *home_quick_geometry = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-quick");
        GtkWidget *home_status_grid_geometry = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-status-grid");
        GtkWidget *home_date_geometry = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-date-card");
        GtkWidget *home_region_geometry = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-region-card");
        GtkWidget *home_appearance_geometry = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-appearance-card");
        GtkWidget *home_network_geometry = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-network-card");
        GtkWidget *home_appearance_previews = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-appearance-previews");
        g_assert_true(GTK_IS_OVERLAY(home_hero_geometry));
        g_assert_true(GTK_IS_FLOW_BOX(home_features_geometry));
        g_assert_true(GTK_IS_FLOW_BOX(home_grid_geometry));
        g_assert_true(GTK_IS_FLOW_BOX(home_status_grid_geometry));
        g_assert_true(GTK_IS_FLOW_BOX(home_appearance_previews));
        HomeAdaptiveLayout *adaptive_layout = g_object_get_data(
            G_OBJECT(home_scroller), "system-settings-home-adaptive-layout");
        g_assert_nonnull(adaptive_layout);
        g_assert_true(adaptive_layout->hero == home_hero_geometry);
        g_assert_cmpint(
            gtk_widget_get_margin_bottom(home_hero_geometry), ==, 28);
        g_assert_true(adaptive_layout->features == GTK_FLOW_BOX(home_features_geometry));
        g_assert_true(adaptive_layout->primary_grid == GTK_FLOW_BOX(home_grid_geometry));
        g_assert_true(adaptive_layout->status_grid == GTK_FLOW_BOX(home_status_grid_geometry));
        g_assert_true(
            adaptive_layout->appearance_previews ==
            GTK_FLOW_BOX(home_appearance_previews));
        g_assert_cmpuint(home_layout_columns_for_width(900), ==, 1U);
        g_assert_cmpuint(home_layout_columns_for_width(1400), ==, 2U);
        home_layout_apply_width(adaptive_layout, 1400);
        {
            int request_width = 0;
            int request_height = 0;
            gtk_widget_get_size_request(
                home_hero_geometry, &request_width, &request_height);
            g_assert_cmpint(request_height, ==, 270);
        }
        g_assert_cmpuint(
            gtk_flow_box_get_min_children_per_line(
                GTK_FLOW_BOX(home_features_geometry)), ==, 3U);
        g_assert_cmpuint(
            gtk_flow_box_get_min_children_per_line(
                GTK_FLOW_BOX(home_grid_geometry)), ==, 2U);
        g_assert_cmpuint(
            gtk_flow_box_get_min_children_per_line(
                GTK_FLOW_BOX(home_status_grid_geometry)), ==, 2U);
        g_assert_cmpuint(
            gtk_flow_box_get_min_children_per_line(
                GTK_FLOW_BOX(home_appearance_previews)), ==, 4U);
        home_layout_apply_width(adaptive_layout, 900);
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        {
            int request_width = 0;
            int request_height = 0;
            gtk_widget_get_size_request(
                home_hero_geometry, &request_width, &request_height);
            g_assert_cmpint(request_height, ==, 390);
        }
        g_assert_cmpuint(
            gtk_flow_box_get_min_children_per_line(
                GTK_FLOW_BOX(home_features_geometry)), ==, 1U);
        g_assert_cmpuint(
            gtk_flow_box_get_min_children_per_line(
                GTK_FLOW_BOX(home_grid_geometry)), ==, 1U);
        g_assert_cmpuint(
            gtk_flow_box_get_min_children_per_line(
                GTK_FLOW_BOX(home_status_grid_geometry)), ==, 1U);
        g_assert_cmpuint(
            gtk_flow_box_get_min_children_per_line(
                GTK_FLOW_BOX(home_appearance_previews)), ==, 1U);
        g_assert_nonnull(home_overview_geometry);
        g_assert_nonnull(home_quick_geometry);
        g_assert_nonnull(home_date_geometry);
        g_assert_nonnull(home_region_geometry);
        g_assert_nonnull(home_appearance_geometry);
        g_assert_nonnull(home_network_geometry);
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
        GtkWidget *sidebar =
            gtk_widget_get_first_child(window_child);
        g_assert_nonnull(sidebar);
        /*
         * A descendant in each navigation row expands so its labels can
         * ellipsise cleanly. That expansion must stop at the sidebar boundary;
         * otherwise GtkBox divides spare window width between the sidebar and
         * the content stack and the dashboard becomes dramatically too narrow.
         */
        g_assert_false(
            gtk_widget_compute_expand(
                sidebar, GTK_ORIENTATION_HORIZONTAL));
        g_assert_true(
            gtk_widget_compute_expand(
                GTK_WIDGET(stack), GTK_ORIENTATION_HORIZONTAL));
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

        /*
         * Regress the actual failure visible at the compact default window:
         * all three feature tiles must finish before the System Overview /
         * Quick Actions flow box begins. Preferred-size checks alone missed
         * this because GtkOverlay can report a sufficient request yet still
         * allocate an overlay descendant beyond the main child's lower edge.
         */
        {
            graphene_rect_t feature_bounds = GRAPHENE_RECT_INIT(0, 0, 0, 0);
            graphene_rect_t grid_bounds = GRAPHENE_RECT_INIT(0, 0, 0, 0);
            g_assert_true(gtk_widget_compute_bounds(
                home_features_geometry,
                GTK_WIDGET(home_scroller),
                &feature_bounds));
            g_assert_true(gtk_widget_compute_bounds(
                home_grid_geometry,
                GTK_WIDGET(home_scroller),
                &grid_bounds));
            g_test_message(
                "Home bounds: feature bottom=%.1f grid top=%.1f hero request=%d",
                feature_bounds.origin.y + feature_bounds.size.height,
                grid_bounds.origin.y,
                390);
            g_assert_true(
                feature_bounds.origin.y + feature_bounds.size.height
                <= grid_bounds.origin.y);

            /*
             * Compact CI windows intentionally stack the two desktop columns;
             * the breakpoint logic above is separately exercised in both
             * states. The three canonical System/Day/Night previews are a
             * compact horizontal strip on the desktop layout and must never
             * regress to stale fourth-theme content.
             */
            guint preview_count = 0U;
            for (GtkWidget *child =
                     gtk_widget_get_first_child(home_appearance_previews);
                 child != NULL;
                 child = gtk_widget_get_next_sibling(child)) {
                preview_count++;
            }
            g_assert_cmpuint(preview_count, ==, 3U);
        }
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

        /*
         * Regression for the 1920x1080 Home screenshot where System Overview
         * and Quick Actions were painted over the hero. GtkOverlay excludes
         * overlay children from measurement unless explicitly requested.
         */
        GtkWidget *home_page =
            gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(home));
        if (GTK_IS_VIEWPORT(home_page)) {
            home_page = gtk_viewport_get_child(GTK_VIEWPORT(home_page));
        }
        GtkWidget *home_hero =
            home_page != NULL
                ? gtk_widget_get_first_child(home_page)
                : NULL;
        g_assert_true(GTK_IS_OVERLAY(home_hero));
        GtkWidget *hero_scene =
            gtk_overlay_get_child(GTK_OVERLAY(home_hero));
        GtkWidget *hero_foreground = NULL;
        for (GtkWidget *child = gtk_widget_get_first_child(home_hero);
             child != NULL;
             child = gtk_widget_get_next_sibling(child)) {
            if (child != hero_scene) {
                hero_foreground = child;
                break;
            }
        }
        g_assert_nonnull(hero_foreground);
        g_assert_true(
            gtk_overlay_get_measure_overlay(
                GTK_OVERLAY(home_hero), hero_foreground));
        {
            int hero_min_height = 0;
            int hero_nat_height = 0;
            int foreground_min_height = 0;
            int foreground_nat_height = 0;
            gtk_widget_measure(
                hero_foreground,
                GTK_ORIENTATION_VERTICAL,
                900,
                &foreground_min_height,
                &foreground_nat_height,
                NULL,
                NULL);
            gtk_widget_measure(
                home_hero,
                GTK_ORIENTATION_VERTICAL,
                900,
                &hero_min_height,
                &hero_nat_height,
                NULL,
                NULL);
            g_assert_cmpint(
                hero_min_height, >=, foreground_min_height);
            g_assert_cmpint(
                hero_nat_height, >=, foreground_nat_height);
        }
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
