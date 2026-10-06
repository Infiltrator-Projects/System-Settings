// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file shell-window.c
 * @brief Native Linux System Settings shell/window controller.
 *
 * This unit owns application lifecycle, framing, navigation and search. It
 * knows only the generic built-in module host; module-private Date & Time
 * types and construction stay behind that composition boundary.
 */

#include "shell-window-private.h"

#include "home-page.h"
#include "linux-theme.h"
#include "linux-ui-helpers.h"
#include "shell-command.h"

#include "system-settings/project-info.h"

#include <gtk/gtk.h>
#include <infiltratr/core.h>

#include <stdbool.h>
#include <stdio.h>

#ifndef SYSTEM_SETTINGS_MODULE_DIR
#define SYSTEM_SETTINGS_MODULE_DIR "/usr/share/infiltrator/system-settings/modules"
#endif
#ifndef SYSTEM_SETTINGS_MODULE_SOURCE_DIR
#define SYSTEM_SETTINGS_MODULE_SOURCE_DIR ""
#endif

#define SS_DATE_TIME_PAGE_ID "date-time"

static void show_about(GtkButton *button, gpointer user_data);
static GtkWidget *build_unavailable_panel(const char *message);

static void shell_search_state_free(gpointer data)
{
    ShellSearchState *state = data;

    if (state == NULL) {
        return;
    }
    g_free(state->query);
    g_free(state);
}

gboolean navigation_filter(GtkListBoxRow *row, gpointer user_data)
{
    ShellSearchState *state = user_data;
    const char *search_text;

    if (state == NULL || state->query == NULL || state->query[0] == '\0') {
        return TRUE;
    }

    search_text = g_object_get_data(G_OBJECT(row), "search-text");
    return search_text != NULL &&
           infiltratr_ascii_contains_ci(search_text, state->query);
}

void on_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
    ShellSearchState *state = user_data;
    const char *text;

    if (state == NULL || state->list == NULL) {
        return;
    }

    text = gtk_editable_get_text(GTK_EDITABLE(entry));
    g_free(state->query);
    state->query = g_strdup(text != NULL ? text : "");
    gtk_list_box_invalidate_filter(state->list);
}

void ensure_date_time_panel(ShellSearchState *state)
{
    g_autoptr(GError) panel_error = NULL;

    if (state == NULL || state->date_scroller == NULL ||
        state->module_host == NULL) {
        return;
    }

    if (ss_builtin_module_host_is_loaded(
            state->module_host, SS_DATE_TIME_PAGE_ID)) {
        state->date_time_loaded = true;
        return;
    }

    if (!ss_builtin_module_host_ensure_page(
            state->module_host,
            SS_DATE_TIME_PAGE_ID,
            state->date_scroller,
            &panel_error)) {
        state->date_time_loaded = false;
        gtk_scrolled_window_set_child(
            state->date_scroller,
            build_unavailable_panel(
                panel_error != NULL
                    ? panel_error->message
                    : "Unable to initialise Date & Time."));
        return;
    }

    state->date_time_loaded = true;
}

static void on_navigation_selected(
    GtkListBox *box,
    GtkListBoxRow *row,
    gpointer user_data)
{
    ShellSearchState *state = user_data;
    const char *page_name;

    (void)box;
    if (row == NULL || state == NULL || state->stack == NULL) {
        return;
    }

    page_name = g_object_get_data(G_OBJECT(row), "page-name");
    if (page_name == NULL) {
        return;
    }
    if (g_strcmp0(page_name, SS_DATE_TIME_PAGE_ID) == 0) {
        ensure_date_time_panel(state);
    }
    gtk_stack_set_visible_child_name(state->stack, page_name);
}

static void restore_navigation_selection(ShellSearchState *state)
{
    const char *visible_name;

    if (state == NULL || state->list == NULL || state->stack == NULL) {
        return;
    }

    visible_name = gtk_stack_get_visible_child_name(state->stack);
    if (g_strcmp0(visible_name, SS_DATE_TIME_PAGE_ID) == 0 &&
        state->date_row != NULL) {
        gtk_list_box_select_row(state->list, state->date_row);
    } else if (state->home_row != NULL) {
        gtk_list_box_select_row(state->list, state->home_row);
    }
}

static void on_navigation_activated(
    GtkListBox *box,
    GtkListBoxRow *row,
    gpointer user_data)
{
    ShellSearchState *state = user_data;
    const char *program;
    const char *argument;
    gboolean show_about_row;

    (void)box;
    if (row == NULL || state == NULL) {
        return;
    }

    program = g_object_get_data(G_OBJECT(row), "action-program");
    argument = g_object_get_data(G_OBJECT(row), "action-argument");
    show_about_row = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(row), "action-about")) != 0;

    if (program != NULL) {
        (void)launch_command(GTK_WIDGET(row), program, argument);
        restore_navigation_selection(state);
    } else if (show_about_row) {
        show_about(NULL, state->parent);
        restore_navigation_selection(state);
    }
}

static GtkWidget *make_navigation_row(
    const char *icon_name,
    const char *title,
    const char *subtitle,
    const char *page_name,
    const char *search_text)
{
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 11);
    GtkWidget *icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *primary = ss_linux_ui_make_label(title, "nav-primary");
    GtkWidget *secondary = ss_linux_ui_make_label(subtitle, "nav-secondary");

    gtk_widget_add_css_class(icon_wrap, "nav-icon-well");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 27);
    gtk_box_append(GTK_BOX(icon_wrap), icon);
    gtk_box_append(GTK_BOX(box), icon_wrap);
    gtk_label_set_ellipsize(GTK_LABEL(primary), PANGO_ELLIPSIZE_END);
    gtk_label_set_ellipsize(GTK_LABEL(secondary), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(primary), 20);
    gtk_label_set_max_width_chars(GTK_LABEL(secondary), 22);
    gtk_widget_set_hexpand(copy, TRUE);
    gtk_box_append(GTK_BOX(copy), primary);
    gtk_box_append(GTK_BOX(copy), secondary);
    gtk_box_append(GTK_BOX(box), copy);
    gtk_widget_add_css_class(row, "nav-row");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    gtk_accessible_update_property(
        GTK_ACCESSIBLE(row),
        GTK_ACCESSIBLE_PROPERTY_LABEL,
        title,
        GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
        subtitle,
        -1);
    g_object_set_data_full(
        G_OBJECT(row), "page-name", g_strdup(page_name), g_free);
    g_object_set_data_full(
        G_OBJECT(row), "search-text", g_strdup(search_text), g_free);
    return row;
}

static GtkWidget *make_external_navigation_row(
    const char *icon_name,
    const char *title,
    const char *subtitle,
    const char *search_text,
    const char *program,
    const char *argument)
{
    GtkWidget *row = make_navigation_row(
        icon_name, title, subtitle, NULL, search_text);
    g_autofree gchar *path = find_trusted_system_program(program);

    g_object_set_data_full(
        G_OBJECT(row), "action-program", g_strdup(path), g_free);
    g_object_set_data_full(
        G_OBJECT(row), "action-argument", g_strdup(argument), g_free);
    if (path == NULL) {
        gtk_widget_set_sensitive(row, FALSE);
        gtk_widget_set_tooltip_text(
            row, "This system tool is not installed.");
    }
    return row;
}

static GtkWidget *make_about_navigation_row(void)
{
    GtkWidget *row = make_navigation_row(
        "help-about-symbolic",
        "About",
        "System information",
        NULL,
        "about system information version");
    g_object_set_data(
        G_OBJECT(row), "action-about", GINT_TO_POINTER(1));
    return row;
}

static void append_manifest_value(
    GString *search,
    GKeyFile *key_file,
    const char *group,
    const char *key)
{
    g_autofree gchar *value = NULL;

    if (search == NULL || key_file == NULL || group == NULL || key == NULL) {
        return;
    }
    value = g_key_file_get_string(key_file, group, key, NULL);
    if (value != NULL && value[0] != '\0') {
        g_string_append_c(search, ' ');
        g_string_append(search, value);
    }
}

static gchar *date_time_search_text(void)
{
    static const char fallback[] =
        "date time clock calendar location timezone seconds precision "
        "12 hour 24 hour decimal internet ntp network time unix binary "
        "hexadecimal julian sidereal solar roman chinese japanese gregorian "
        "hebrew islamic persian mayan french republican first day week";
    const char *override_dir =
        g_getenv("SYSTEM_SETTINGS_MODULE_DIR_OVERRIDE");
    const bool override_active =
        override_dir != NULL && override_dir[0] != '\0';
    const char *directories[] = {
        override_active ? override_dir : SYSTEM_SETTINGS_MODULE_SOURCE_DIR,
        override_active ? NULL : SYSTEM_SETTINGS_MODULE_DIR,
        NULL
    };

    for (size_t directory_index = 0U;
         directories[directory_index] != NULL;
         ++directory_index) {
        g_autofree gchar *path = NULL;
        g_autoptr(GKeyFile) key_file = NULL;
        g_auto(GStrv) groups = NULL;
        gsize group_count = 0U;
        g_autoptr(GError) error = NULL;
        GString *search = NULL;

        if (directories[directory_index][0] == '\0') {
            continue;
        }
        path = g_build_filename(
            directories[directory_index],
            "date-time.settings-module",
            NULL);
        key_file = g_key_file_new();
        if (!g_key_file_load_from_file(
                key_file, path, G_KEY_FILE_NONE, &error)) {
            continue;
        }

        search = g_string_new("date time");
        append_manifest_value(search, key_file, "Module", "Title");
        append_manifest_value(search, key_file, "Module", "Summary");
        append_manifest_value(search, key_file, "Search", "Keywords");
        groups = g_key_file_get_groups(key_file, &group_count);
        for (gsize group_index = 0U;
             group_index < group_count;
             ++group_index) {
            if (g_str_has_prefix(groups[group_index], "Target ")) {
                append_manifest_value(
                    search, key_file, groups[group_index], "Title");
                append_manifest_value(
                    search, key_file, groups[group_index], "Keywords");
            }
        }
        return g_string_free(search, FALSE);
    }

    return g_strdup(fallback);
}

static void minimize_window(GtkButton *button, gpointer user_data)
{
    (void)button;
    gtk_window_minimize(GTK_WINDOW(user_data));
}

static void toggle_maximize_window(GtkButton *button, gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);

    (void)button;
    if (gtk_window_is_maximized(window)) {
        gtk_window_unmaximize(window);
    } else {
        gtk_window_maximize(window);
    }
}

static void close_window(GtkButton *button, gpointer user_data)
{
    (void)button;
    gtk_window_close(GTK_WINDOW(user_data));
}

static GtkWidget *make_window_control(
    const char *icon_name,
    const char *tooltip,
    const char *css_class)
{
    GtkWidget *button = gtk_button_new_from_icon_name(icon_name);

    gtk_widget_add_css_class(button, "window-control");
    if (css_class != NULL) {
        gtk_widget_add_css_class(button, css_class);
    }
    gtk_widget_set_tooltip_text(button, tooltip);
    gtk_accessible_update_property(
        GTK_ACCESSIBLE(button),
        GTK_ACCESSIBLE_PROPERTY_LABEL,
        tooltip,
        -1);
    return button;
}

static GtkWidget *build_header(GtkWindow *parent, GtkSearchEntry **search_out)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    GtkWidget *header = gtk_header_bar_new();
    GtkWidget *brand = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *icon = gtk_image_new_from_icon_name(info->icon_name);
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *search = gtk_search_entry_new();
    GtkWidget *header_end = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *minimize = make_window_control(
        "window-minimize-symbolic", "Minimize", NULL);
    GtkWidget *maximize = make_window_control(
        "window-maximize-symbolic", "Maximize / Restore", NULL);
    GtkWidget *close = make_window_control(
        "window-close-symbolic", "Close", "window-control-close");
    GtkWidget *empty_title = gtk_label_new("");

    gtk_widget_add_css_class(header, "shell-header");
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), FALSE);
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), empty_title);
    gtk_widget_add_css_class(brand, "header-brand");
    gtk_widget_add_css_class(icon_wrap, "header-brand-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 28);
    gtk_box_append(GTK_BOX(icon_wrap), icon);
    gtk_box_append(GTK_BOX(brand), icon_wrap);
    gtk_box_append(
        GTK_BOX(copy),
        ss_linux_ui_make_label("System Settings", "header-brand-title"));
    gtk_box_append(
        GTK_BOX(copy),
        ss_linux_ui_make_label("Infiltrator OS", "header-brand-subtitle"));
    gtk_box_append(GTK_BOX(brand), copy);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), brand);

    gtk_search_entry_set_placeholder_text(
        GTK_SEARCH_ENTRY(search), "Filter settings navigation…");
    gtk_widget_set_tooltip_text(
        search, "Filter settings categories and their indexed keywords.");
    gtk_accessible_update_property(
        GTK_ACCESSIBLE(search),
        GTK_ACCESSIBLE_PROPERTY_LABEL,
        "Search settings",
        GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
        "Filter settings categories and their indexed keywords.",
        -1);
    gtk_widget_add_css_class(search, "settings-search");
    gtk_widget_set_size_request(search, 200, -1);
    gtk_widget_add_css_class(header_end, "header-end");
    g_signal_connect(
        minimize, "clicked", G_CALLBACK(minimize_window), parent);
    g_signal_connect(
        maximize, "clicked", G_CALLBACK(toggle_maximize_window), parent);
    g_signal_connect(close, "clicked", G_CALLBACK(close_window), parent);
    gtk_box_append(GTK_BOX(header_end), search);
    gtk_box_append(GTK_BOX(header_end), minimize);
    gtk_box_append(GTK_BOX(header_end), maximize);
    gtk_box_append(GTK_BOX(header_end), close);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), header_end);

    g_object_set_data(
        G_OBJECT(parent), "system-settings-header-end", header_end);
    g_object_set_data(
        G_OBJECT(parent), "system-settings-search-entry", search);
    g_object_set_data(
        G_OBJECT(parent), "system-settings-minimize-button", minimize);
    g_object_set_data(
        G_OBJECT(parent), "system-settings-maximize-button", maximize);
    g_object_set_data(
        G_OBJECT(parent), "system-settings-close-button", close);

    if (search_out != NULL) {
        *search_out = GTK_SEARCH_ENTRY(search);
    }
    return header;
}

static GtkWidget *build_sidebar(
    GtkWindow *parent,
    GtkStack *stack,
    GtkSearchEntry *search_entry,
    ShellSearchState *search_state)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *list = gtk_list_box_new();
    GtkWidget *nav_scroller = gtk_scrolled_window_new();
    GtkWidget *home_row;
    GtkWidget *date_row;
    GtkWidget *region_row;
    GtkWidget *appearance_row;
    GtkWidget *sound_row;
    GtkWidget *network_row;
    GtkWidget *bluetooth_row;
    GtkWidget *power_row;
    GtkWidget *users_row;
    GtkWidget *privacy_row;
    GtkWidget *hardware_row;
    GtkWidget *software_row;
    GtkWidget *about_row;
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    g_autofree gchar *version = g_strdup_printf("Version %s", info->version);
    g_autofree gchar *date_search = date_time_search_text();

    gtk_widget_set_size_request(sidebar, 205, -1);
    gtk_widget_set_hexpand(sidebar, FALSE);
    gtk_widget_add_css_class(sidebar, "settings-sidebar");
    gtk_box_append(
        GTK_BOX(sidebar),
        ss_linux_ui_make_label("SYSTEM", "nav-title"));

    home_row = make_navigation_row(
        "go-home-symbolic",
        "Home",
        "Overview & quick access",
        "home",
        "home overview quick access system");
    date_row = make_navigation_row(
        "preferences-system-time-symbolic",
        "Date & Time",
        "Clock, calendar & location",
        SS_DATE_TIME_PAGE_ID,
        date_search);
    region_row = make_external_navigation_row(
        "preferences-desktop-locale-symbolic",
        "Region & Language",
        "Language, formats & input",
        "region language locale formats input",
        "mintlocale",
        NULL);
    appearance_row = make_external_navigation_row(
        "preferences-desktop-theme-symbolic",
        "Appearance",
        "Cinnamon themes & desktop appearance",
        "appearance theme themes desktop cinnamon",
        "cinnamon-settings",
        "themes");
    sound_row = make_external_navigation_row(
        "audio-volume-high-symbolic",
        "Sound",
        "Audio devices & volume",
        "sound audio devices volume speakers",
        "cinnamon-settings",
        "sound");
    network_row = make_external_navigation_row(
        "network-wired-symbolic",
        "Network",
        "Wi-Fi, wired & internet",
        "network wifi wireless ethernet internet",
        "cinnamon-settings",
        "network");
    bluetooth_row = make_external_navigation_row(
        "bluetooth-active-symbolic",
        "Bluetooth",
        "Devices & pairing",
        "bluetooth devices pairing",
        "blueman-manager",
        NULL);
    power_row = make_external_navigation_row(
        "battery-good-symbolic",
        "Power",
        "Battery & power management",
        "power battery energy management",
        "cinnamon-settings",
        "power");
    users_row = make_external_navigation_row(
        "system-users-symbolic",
        "Users & Accounts",
        "Account settings & login",
        "users accounts login password",
        "cinnamon-settings",
        "user");
    privacy_row = make_external_navigation_row(
        "security-high-symbolic",
        "Privacy & Security",
        "Permissions & system security",
        "privacy security permissions",
        "cinnamon-settings",
        "privacy");
    hardware_row = make_external_navigation_row(
        "computer-symbolic",
        "System Information",
        "Hardware and operating-system details",
        "hardware system information operating system details",
        "cinnamon-settings",
        "info");
    software_row = make_external_navigation_row(
        "system-software-install-symbolic",
        "Software & Updates",
        "Updates, drivers & repositories",
        "software updates drivers repositories packages",
        "infiltrator-software",
        NULL);
    about_row = make_about_navigation_row();

    gtk_list_box_append(GTK_LIST_BOX(list), home_row);
    gtk_list_box_append(GTK_LIST_BOX(list), date_row);
    gtk_list_box_append(GTK_LIST_BOX(list), region_row);
    gtk_list_box_append(GTK_LIST_BOX(list), appearance_row);
    gtk_list_box_append(GTK_LIST_BOX(list), sound_row);
    gtk_list_box_append(GTK_LIST_BOX(list), network_row);
    gtk_list_box_append(GTK_LIST_BOX(list), bluetooth_row);
    gtk_list_box_append(GTK_LIST_BOX(list), power_row);
    gtk_list_box_append(GTK_LIST_BOX(list), users_row);
    gtk_list_box_append(GTK_LIST_BOX(list), privacy_row);
    gtk_list_box_append(GTK_LIST_BOX(list), hardware_row);
    gtk_list_box_append(GTK_LIST_BOX(list), software_row);
    gtk_list_box_append(GTK_LIST_BOX(list), about_row);

    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
    g_signal_connect(
        list,
        "row-selected",
        G_CALLBACK(on_navigation_selected),
        search_state);
    g_signal_connect(
        list,
        "row-activated",
        G_CALLBACK(on_navigation_activated),
        search_state);

    if (search_state != NULL) {
        search_state->list = GTK_LIST_BOX(list);
        search_state->parent = parent;
        search_state->stack = stack;
        search_state->home_row = GTK_LIST_BOX_ROW(home_row);
        search_state->date_row = GTK_LIST_BOX_ROW(date_row);
        gtk_list_box_set_filter_func(
            GTK_LIST_BOX(list), navigation_filter, search_state, NULL);
        if (search_entry != NULL) {
            g_signal_connect(
                search_entry,
                "search-changed",
                G_CALLBACK(on_search_changed),
                search_state);
        }
    }

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(nav_scroller),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_overlay_scrolling(
        GTK_SCROLLED_WINDOW(nav_scroller), FALSE);
    gtk_widget_set_vexpand(nav_scroller, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(nav_scroller), list);
    gtk_box_append(GTK_BOX(sidebar), nav_scroller);
    g_object_set_data(
        G_OBJECT(parent),
        "system-settings-navigation-scroller",
        nav_scroller);

    gtk_widget_add_css_class(footer, "sidebar-footer");
    gtk_widget_set_hexpand(footer, TRUE);
    gtk_box_append(
        GTK_BOX(footer),
        ss_linux_ui_make_label(version, "sidebar-version"));
    gtk_box_append(GTK_BOX(sidebar), footer);
    return sidebar;
}

void open_date_time(GtkButton *button, gpointer user_data)
{
    ShellSearchState *state = user_data;

    (void)button;
    if (state == NULL || state->stack == NULL) {
        return;
    }
    ensure_date_time_panel(state);
    gtk_stack_set_visible_child_name(state->stack, SS_DATE_TIME_PAGE_ID);
    if (state->list != NULL && state->date_row != NULL) {
        gtk_list_box_select_row(state->list, state->date_row);
    }
}

static void open_date_time_from_home(gpointer user_data)
{
    open_date_time(NULL, user_data);
}

static gboolean about_close_requested(GtkWindow *window, gpointer user_data)
{
    (void)user_data;
    gtk_window_destroy(window);
    return TRUE;
}

static void show_about(GtkButton *button, gpointer user_data)
{
    GtkWindow *parent = GTK_WINDOW(user_data);
    const InfiltratrProjectInfo *info = ss_project_info();
    const char *authors[] = {
        "Shannon Smith — Author and project maintainer",
        NULL
    };
    char comments[512];
    GtkWidget *dialog;

    (void)button;
    (void)snprintf(
        comments,
        sizeof(comments),
        "%s\n\nBuild: %s",
        info->comments,
        infiltratr_build_profile_label(info->build_profile));

    dialog = gtk_about_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "About System Settings");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_about_dialog_set_program_name(
        GTK_ABOUT_DIALOG(dialog), info->program_name);
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), info->version);
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), comments);
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), info->website);
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(dialog), "Website");
    gtk_about_dialog_set_copyright(
        GTK_ABOUT_DIALOG(dialog), info->copyright_text);
    gtk_about_dialog_set_license_type(
        GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_CUSTOM);
    gtk_about_dialog_set_license(
        GTK_ABOUT_DIALOG(dialog),
        "System Settings is free software licensed under the GNU General "
        "Public License version 3 or, at your option, any later version "
        "(GPL-3.0-or-later).\n\n"
        "See LICENSE in the source package for the complete licence text.");
    gtk_about_dialog_set_wrap_license(GTK_ABOUT_DIALOG(dialog), TRUE);
    gtk_about_dialog_set_logo_icon_name(
        GTK_ABOUT_DIALOG(dialog), info->icon_name);
    g_signal_connect(
        dialog,
        "close-request",
        G_CALLBACK(about_close_requested),
        NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

static GtkWidget *build_unavailable_panel(const char *message)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkWidget *copy = ss_linux_ui_make_label(
        message != NULL ? message : "Date & Time is unavailable.",
        "error");

    gtk_widget_add_css_class(page, "settings-content");
    gtk_box_append(
        GTK_BOX(page),
        ss_linux_ui_make_label("SYSTEM / DATE & TIME", "page-eyebrow"));
    gtk_box_append(
        GTK_BOX(page),
        ss_linux_ui_make_label("Date & Time", "page-title"));
    gtk_label_set_wrap(GTK_LABEL(copy), TRUE);
    gtk_box_append(GTK_BOX(page), copy);
    return page;
}

static gboolean on_close_requested(GtkWindow *window, gpointer user_data)
{
    ShellSearchState *state;

    (void)user_data;
    state = g_object_get_data(
        G_OBJECT(window), "system-settings-search-state");
    if (state != NULL) {
        state->module_host = NULL;
        state->date_time_loaded = false;
    }

    /*
     * Destroy the module composition object before GTK tears down the window.
     * Its destructor clears the compatibility lookup used by pending Date &
     * Time callbacks, so no reply can observe freed module state.
     */
    g_object_set_data(G_OBJECT(window), "system-settings-module-host", NULL);
    return FALSE;
}

static void set_adaptive_default_window_size(GtkWindow *window)
{
    int width = 1180;
    int height = 760;
    GdkDisplay *display;
    GListModel *monitors;
    GdkMonitor *monitor = NULL;
    g_autoptr(GdkMonitor) fallback_monitor = NULL;
    GdkSurface *surface;

    if (window == NULL) {
        return;
    }

    display = gtk_widget_get_display(GTK_WIDGET(window));
    surface = gtk_native_get_surface(GTK_NATIVE(window));
    if (display != NULL && surface != NULL) {
        monitor = gdk_display_get_monitor_at_surface(display, surface);
    }
    monitors = display != NULL ? gdk_display_get_monitors(display) : NULL;
    if (monitor == NULL && monitors != NULL &&
        g_list_model_get_n_items(monitors) > 0U) {
        fallback_monitor = GDK_MONITOR(g_list_model_get_item(monitors, 0U));
        monitor = fallback_monitor;
    }

    if (monitor != NULL) {
        GdkRectangle geometry = {0};
        gdk_monitor_get_geometry(monitor, &geometry);
        if (geometry.width > 0) {
            width = MIN(width, MAX(1, geometry.width * 9 / 10));
        }
        if (geometry.height > 0) {
            height = MIN(height, MAX(1, geometry.height * 9 / 10));
        }
    }
    gtk_window_set_default_size(window, width, height);
}

static void adapt_window_to_mapped_monitor(
    GtkWidget *widget,
    gpointer user_data G_GNUC_UNUSED)
{
    set_adaptive_default_window_size(GTK_WINDOW(widget));
}

void on_activate(GtkApplication *application, gpointer user_data)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    GtkWindow *window;
    GtkWidget *root;
    GtkWidget *stack_widget;
    GtkStack *stack;
    GtkWidget *date_scroller;
    GtkWidget *home_scroller;
    GtkSearchEntry *search_entry = NULL;
    ShellSearchState *search_state;
    SsBuiltinModuleHost *module_host;
    SsHomePageContext home_context;

    (void)user_data;

    window = gtk_application_get_active_window(application);
    if (window != NULL) {
        gtk_window_present(window);
        return;
    }

    ss_linux_theme_install();
    ss_linux_theme_watch();

    window = GTK_WINDOW(gtk_application_window_new(application));
    gtk_window_set_title(window, info->program_name);
    set_adaptive_default_window_size(window);
    gtk_window_set_resizable(window, TRUE);
    g_signal_connect(
        window,
        "close-request",
        G_CALLBACK(on_close_requested),
        NULL);
    g_signal_connect(
        window,
        "map",
        G_CALLBACK(adapt_window_to_mapped_monitor),
        NULL);
    gtk_window_set_titlebar(window, build_header(window, &search_entry));

    module_host = ss_builtin_module_host_new(window);
    if (module_host == NULL) {
        gtk_window_destroy(window);
        return;
    }
    g_object_set_data_full(
        G_OBJECT(window),
        "system-settings-module-host",
        module_host,
        (GDestroyNotify)ss_builtin_module_host_free);

    root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(root, "app-shell");
    gtk_window_set_child(window, root);

    stack_widget = gtk_stack_new();
    stack = GTK_STACK(stack_widget);
    gtk_stack_set_transition_type(
        stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(stack, 110U);
    gtk_widget_set_hexpand(stack_widget, TRUE);
    gtk_widget_set_vexpand(stack_widget, TRUE);

    search_state = g_new0(ShellSearchState, 1);
    search_state->module_host = module_host;
    g_object_set_data_full(
        G_OBJECT(window),
        "system-settings-search-state",
        search_state,
        shell_search_state_free);
    gtk_box_append(
        GTK_BOX(root),
        build_sidebar(window, stack, search_entry, search_state));
    g_object_set_data(
        G_OBJECT(window),
        "system-settings-navigation-list",
        search_state->list);

    date_scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(date_scroller),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_overlay_scrolling(
        GTK_SCROLLED_WINDOW(date_scroller), FALSE);
    gtk_scrolled_window_set_kinetic_scrolling(
        GTK_SCROLLED_WINDOW(date_scroller), TRUE);
    gtk_widget_set_hexpand(date_scroller, TRUE);
    gtk_widget_set_vexpand(date_scroller, TRUE);
    search_state->date_scroller = GTK_SCROLLED_WINDOW(date_scroller);
    g_object_set_data(
        G_OBJECT(window),
        "system-settings-date-scroller",
        date_scroller);

    home_context = (SsHomePageContext){
        .module_host = module_host,
        .open_date_time = open_date_time_from_home,
        .open_date_time_data = search_state
    };
    home_scroller = ss_linux_home_page_new(&home_context);
    g_object_set_data(
        G_OBJECT(stack), "system-settings-home-scroller", home_scroller);
    gtk_stack_add_named(stack, home_scroller, "home");
    gtk_stack_add_named(stack, date_scroller, SS_DATE_TIME_PAGE_ID);
    gtk_stack_set_visible_child_name(stack, "home");
    if (search_state->list != NULL && search_state->home_row != NULL) {
        gtk_list_box_select_row(search_state->list, search_state->home_row);
    }
    g_object_set_data(
        G_OBJECT(window), "system-settings-stack", stack);
    gtk_box_append(GTK_BOX(root), stack_widget);

    gtk_window_present(window);
}
