// SPDX-License-Identifier: GPL-3.0-or-later
#include "shell-window.h"

#include <gtk/gtk.h>

static void drain_main_context(void)
{
    while (g_main_context_iteration(NULL, FALSE)) {
    }
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

static void exercise_window(GtkApplication *app)
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
    drain_main_context();

    root = gtk_window_get_child(window);
    g_assert_true(subtree_has_label(root, "SYSTEM / DATE & TIME"));
    g_assert_true(subtree_has_label(root, "Location & time zone"));
    g_assert_true(subtree_has_label(root, "Clock system"));
    g_assert_true(subtree_has_label(root, "Manual date and time"));

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

    g_assert_nonnull(root);
    module_dir = g_build_filename(root, "modules", NULL);
    g_assert_cmpint(g_mkdir_with_parents(module_dir, 0700), ==, 0);
    manifest_path = g_build_filename(
        module_dir, "date-time.settings-module", NULL);
    g_assert_true(g_file_set_contents(manifest_path, manifest, -1, NULL));

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

    exercise_window(app);
    exercise_window(app);
    return 0;
}
