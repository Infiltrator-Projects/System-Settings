// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell-window.h"
#include "shell-command.h"

#include <gtk/gtk.h>

typedef struct ExerciseExpectations {
    gboolean persisted;
    guint clock_index;
    gboolean show_seconds;
} ExerciseExpectations;

static void drain_main_context(void)
{
    while (g_main_context_iteration(NULL, FALSE)) {
    }
}

static void drain_main_context_for(guint milliseconds)
{
    const gint64 deadline =
        g_get_monotonic_time() + ((gint64)milliseconds * 1000);

    do {
        drain_main_context();
        g_usleep(1000U);
    } while (g_get_monotonic_time() < deadline);
    drain_main_context();
}

static gboolean subtree_has_label(GtkWidget *widget, const char *text)
{
    if (widget == NULL || text == NULL) {
        return FALSE;
    }
    if (GTK_IS_LABEL(widget) &&
        g_strcmp0(gtk_label_get_text(GTK_LABEL(widget)), text) == 0) {
        return TRUE;
    }
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        if (subtree_has_label(child, text)) {
            return TRUE;
        }
    }
    return FALSE;
}

static GtkWidget *find_widget_type(GtkWidget *widget, GType type)
{
    if (widget == NULL) {
        return NULL;
    }
    if (G_TYPE_CHECK_INSTANCE_TYPE(widget, type)) {
        return widget;
    }
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = find_widget_type(child, type);
        if (match != NULL) {
            return match;
        }
    }
    return NULL;
}

/*
 * Walk children first so the smallest presentation subtree containing the
 * requested setting label wins. This keeps the regression black-box: it does
 * not depend on private controller pointers or widget object-data keys.
 */
static GtkWidget *find_control_for_label(
    GtkWidget *widget,
    const char *label,
    GType control_type)
{
    if (widget == NULL || label == NULL) {
        return NULL;
    }

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match =
            find_control_for_label(child, label, control_type);
        if (match != NULL) {
            return match;
        }
    }

    if (subtree_has_label(widget, label)) {
        return find_widget_type(widget, control_type);
    }
    return NULL;
}

static GtkWidget *find_widget_tooltip(GtkWidget *widget, const char *tooltip)
{
    if (widget == NULL || tooltip == NULL) {
        return NULL;
    }
    if (g_strcmp0(gtk_widget_get_tooltip_text(widget), tooltip) == 0) {
        return widget;
    }
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = find_widget_tooltip(child, tooltip);
        if (match != NULL) {
            return match;
        }
    }
    return NULL;
}

static GtkListBoxRow *find_row_with_label(GtkWidget *widget, const char *label)
{
    if (widget == NULL) {
        return NULL;
    }
    if (GTK_IS_LIST_BOX_ROW(widget) && subtree_has_label(widget, label)) {
        return GTK_LIST_BOX_ROW(widget);
    }
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkListBoxRow *match = find_row_with_label(child, label);
        if (match != NULL) {
            return match;
        }
    }
    return NULL;
}

static void exercise_window(
    GtkApplication *app,
    ExerciseExpectations *expectations)
{
    ss_shell_window_activate(app, NULL);
    drain_main_context();

    GtkWindow *window = gtk_application_get_active_window(app);
    g_assert_nonnull(window);
    g_object_ref(window);

    ss_shell_window_activate(app, NULL);
    drain_main_context();
    g_assert_cmpuint(g_list_length(gtk_application_get_windows(app)), ==, 1U);

    GtkWidget *root = gtk_window_get_child(window);
    g_assert_nonnull(root);
    g_assert_true(subtree_has_label(root, "System Overview"));
    g_assert_true(subtree_has_label(root, "Quick Actions"));
    g_assert_true(subtree_has_label(root, "Configure your system, your way."));

    /*
     * The titlebar is deliberately outside gtk_window_get_child(). Search the
     * public GtkWindow widget tree rather than depending on private object-data
     * keys or exact header composition.
     */
    g_assert_nonnull(find_widget_type(GTK_WIDGET(window), GTK_TYPE_SEARCH_ENTRY));
    g_assert_nonnull(find_widget_tooltip(GTK_WIDGET(window), "Minimize"));
    g_assert_nonnull(
        find_widget_tooltip(GTK_WIDGET(window), "Maximize / Restore"));
    g_assert_nonnull(find_widget_tooltip(GTK_WIDGET(window), "Close"));

    /*
     * The test manifest intentionally renames the navigation entry. This is a
     * black-box regression proving the shell no longer hard-codes Date & Time
     * presentation metadata while the same built-in implementation still
     * loads behind the registry page ID.
     */
    GtkListBoxRow *date_row = find_row_with_label(root, "Temporal Test Module");
    g_assert_nonnull(date_row);
    g_assert_true(subtree_has_label(GTK_WIDGET(date_row), "Manifest-driven test module"));
    GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(date_row));
    while (parent != NULL && !GTK_IS_LIST_BOX(parent)) {
        parent = gtk_widget_get_parent(parent);
    }
    g_assert_true(GTK_IS_LIST_BOX(parent));
    gtk_list_box_select_row(GTK_LIST_BOX(parent), date_row);
    drain_main_context_for(20U);

    root = gtk_window_get_child(window);
    g_assert_true(subtree_has_label(root, "SYSTEM / DATE & TIME"));
    g_assert_true(subtree_has_label(root, "Location & time zone"));
    g_assert_true(subtree_has_label(root, "Clock system"));
    g_assert_true(subtree_has_label(root, "Manual date and time"));

    GtkDropDown *clock = GTK_DROP_DOWN(
        find_control_for_label(root, "Clock system", GTK_TYPE_DROP_DOWN));
    GtkSwitch *seconds = GTK_SWITCH(
        find_control_for_label(root, "Show seconds", GTK_TYPE_SWITCH));
    GtkDropDown *timezone = GTK_DROP_DOWN(
        find_control_for_label(root, "Time zone", GTK_TYPE_DROP_DOWN));
    GtkSwitch *network_time = GTK_SWITCH(
        find_control_for_label(root, "Network time", GTK_TYPE_SWITCH));

    g_assert_nonnull(clock);
    g_assert_nonnull(seconds);
    g_assert_nonnull(timezone);
    g_assert_nonnull(network_time);

    /* The private test bus is intentionally absent. Protected controls must
     * fail closed while ordinary presentation policy remains usable. */
    g_assert_false(gtk_widget_get_sensitive(GTK_WIDGET(timezone)));
    g_assert_false(gtk_widget_get_sensitive(GTK_WIDGET(network_time)));

    if (!expectations->persisted) {
        GListModel *clock_model = gtk_drop_down_get_model(clock);
        const guint clock_count = g_list_model_get_n_items(clock_model);
        const guint current_clock = gtk_drop_down_get_selected(clock);

        g_assert_cmpuint(clock_count, >, 1U);
        expectations->clock_index = (current_clock + 1U) % clock_count;
        expectations->show_seconds =
            gtk_switch_get_active(seconds) == FALSE;

        gtk_switch_set_active(seconds, expectations->show_seconds);
        gtk_drop_down_set_selected(clock, expectations->clock_index);
        drain_main_context_for(50U);

        g_assert_cmpuint(
            gtk_drop_down_get_selected(clock),
            ==,
            expectations->clock_index);
        g_assert_cmpint(
            gtk_switch_get_active(seconds) != FALSE,
            ==,
            expectations->show_seconds != FALSE);

        g_autofree gchar *policy_path = g_build_filename(
            g_get_user_config_dir(),
            "infiltrator",
            "presentation.conf",
            NULL);
        g_assert_true(g_file_test(policy_path, G_FILE_TEST_IS_REGULAR));
        expectations->persisted = TRUE;
    } else {
        /* Closing and recreating the complete shell must reconstruct controls
         * from authoritative persisted policy rather than stale widget state. */
        g_assert_cmpuint(
            gtk_drop_down_get_selected(clock),
            ==,
            expectations->clock_index);
        g_assert_cmpint(
            gtk_switch_get_active(seconds) != FALSE,
            ==,
            expectations->show_seconds != FALSE);
    }

    gtk_window_close(window);
    for (unsigned int attempt = 0U; attempt < 1000U; ++attempt) {
        drain_main_context();
        if (g_list_length(gtk_application_get_windows(app)) == 0U) {
            break;
        }
        g_usleep(1000U);
    }
    g_assert_cmpuint(g_list_length(gtk_application_get_windows(app)), ==, 0U);
    g_object_unref(window);
}

int main(void)
{
    static const char manifest[] =
        "[Module]\n"
        "Id=org.infiltrator.settings.date-time\n"
        "ApiVersion=1\n"
        "Library=date-time\n"
        "Category=system\n"
        "Order=40\n"
        "Title=Temporal Test Module\n"
        "Summary=Manifest-driven test module\n"
        "Icon=preferences-system-time\n"
        "\n"
        "[Search]\n"
        "Keywords=temporal;manifest;clock\n";
    g_autofree gchar *root = g_dir_make_tmp("ss-shell-XXXXXX", NULL);
    g_autofree gchar *module_dir = NULL;
    g_autofree gchar *manifest_path = NULL;
    g_autofree gchar *trusted_shell = NULL;
    ExerciseExpectations expectations = {0};

    g_assert_nonnull(root);
    module_dir = g_build_filename(root, "modules", NULL);
    g_assert_cmpint(g_mkdir_with_parents(module_dir, 0700), ==, 0);
    manifest_path = g_build_filename(
        module_dir, "date-time.settings-module", NULL);
    g_assert_true(g_file_set_contents(manifest_path, manifest, -1, NULL));

    /* Resolver output must be a canonical trusted executable, never a caller-
     * supplied path or a symlink-bearing discovery pathname. */
    trusted_shell = find_trusted_system_program("sh");
    g_assert_nonnull(trusted_shell);
    g_assert_true(g_path_is_absolute(trusted_shell));
    g_assert_false(g_file_test(trusted_shell, G_FILE_TEST_IS_SYMLINK));
    g_assert_null(find_trusted_system_program("../sh"));
    g_assert_null(find_trusted_system_program("/bin/sh"));

    g_setenv("XDG_CONFIG_HOME", root, TRUE);
    g_setenv("GSETTINGS_BACKEND", "memory", TRUE);
    g_setenv(
        "DBUS_SYSTEM_BUS_ADDRESS",
        "unix:path=/nonexistent/ss-test-bus",
        TRUE);
    g_setenv(
        "SYSTEM_SETTINGS_UI_ASSET_DIR_OVERRIDE",
        SYSTEM_SETTINGS_UI_SOURCE_DIR,
        TRUE);
    g_setenv(
        "SYSTEM_SETTINGS_MODULE_DIR_OVERRIDE",
        module_dir,
        TRUE);

    gtk_init();
    g_autoptr(GtkApplication) app = gtk_application_new(
        "org.infiltrator.SystemSettings.Test",
        G_APPLICATION_NON_UNIQUE);
    g_assert_true(g_application_register(G_APPLICATION(app), NULL, NULL));

    exercise_window(app, &expectations);
    exercise_window(app, &expectations);
    return 0;
}
