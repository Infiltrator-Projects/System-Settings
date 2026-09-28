// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main.c
 * @brief Native Linux System Settings application shell.
 *
 * The shell owns application lifecycle, framing, navigation and shared
 * presentation only. Date & Time domain policy/backends live in its module.
 */

#include "linux-date-time-panel.h"
#include "linux-ui-helpers.h"
#include "linux-theme.h"
#include "home-temporal-presentation.h"

#include "system-settings/project-info.h"

#include <gtk/gtk.h>
#include <infiltratr/core.h>

#include <stdbool.h>
#include <stdio.h>
#include <locale.h>
#include <sys/utsname.h>

#ifndef SYSTEM_SETTINGS_MODULE_DIR
#define SYSTEM_SETTINGS_MODULE_DIR "/usr/share/infiltrator/system-settings/modules"
#endif
#ifndef SYSTEM_SETTINGS_MODULE_SOURCE_DIR
#define SYSTEM_SETTINGS_MODULE_SOURCE_DIR ""
#endif

typedef struct {
    GtkListBox *list;
    GtkWindow *parent;
    GtkStack *stack;
    GtkListBoxRow *home_row;
    GtkListBoxRow *date_row;
    gchar *query;
} ShellSearchState;

static void show_about(GtkButton *button, gpointer user_data);

static void shell_search_state_free(gpointer data)
{
    ShellSearchState *state = data;
    if (state == NULL) {
        return;
    }
    g_free(state->query);
    g_free(state);
}

static gboolean navigation_filter(GtkListBoxRow *row, gpointer user_data)
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

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data)
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

static void on_navigation_selected(GtkListBox *box,
                                   GtkListBoxRow *row,
                                   gpointer user_data)
{
    GtkStack *stack = GTK_STACK(user_data);
    const char *page_name;

    (void)box;
    if (row == NULL || stack == NULL) {
        return;
    }

    page_name = g_object_get_data(G_OBJECT(row), "page-name");
    if (page_name != NULL) {
        gtk_stack_set_visible_child_name(stack, page_name);
    }
}

static gboolean launch_command(GtkWidget *source,
                               const char *program,
                               const char *argument)
{
    const char *argv[3] = { program, argument, NULL };
    g_autoptr(GError) error = NULL;
    g_autoptr(GSubprocess) process = NULL;

    if (program == NULL || program[0] == '\0') {
        return FALSE;
    }

    if (argument == NULL || argument[0] == '\0') {
        argv[1] = NULL;
    }

    process = g_subprocess_newv(
        argv,
        G_SUBPROCESS_FLAGS_NONE,
        &error);
    if (process == NULL) {
        if (source != NULL && error != NULL) {
            gtk_widget_set_tooltip_text(source, error->message);
        }
        return FALSE;
    }
    return TRUE;
}

static void restore_navigation_selection(ShellSearchState *state)
{
    const char *visible_name;

    if (state == NULL || state->list == NULL || state->stack == NULL) {
        return;
    }

    visible_name = gtk_stack_get_visible_child_name(state->stack);
    if (g_strcmp0(visible_name, "date-time") == 0 &&
        state->date_row != NULL) {
        gtk_list_box_select_row(state->list, state->date_row);
    } else if (state->home_row != NULL) {
        gtk_list_box_select_row(state->list, state->home_row);
    }
}

static void on_navigation_activated(GtkListBox *box,
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
    show_about_row =
        GPOINTER_TO_INT(
            g_object_get_data(G_OBJECT(row), "action-about")) != 0;

    if (program != NULL) {
        (void)launch_command(GTK_WIDGET(row), program, argument);
        restore_navigation_selection(state);
    } else if (show_about_row) {
        show_about(NULL, state->parent);
        restore_navigation_selection(state);
    }
}

static GtkWidget *make_navigation_row(const char *icon_name,
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

    gtk_widget_add_css_class(icon_wrap, "nav-icon-well");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 27);
    gtk_box_append(GTK_BOX(icon_wrap), icon);
    gtk_box_append(GTK_BOX(box), icon_wrap);
    gtk_box_append(
        GTK_BOX(copy),
        ss_linux_ui_make_label(title, "nav-primary"));
    gtk_box_append(
        GTK_BOX(copy),
        ss_linux_ui_make_label(subtitle, "nav-secondary"));
    gtk_box_append(GTK_BOX(box), copy);
    gtk_widget_add_css_class(row, "nav-row");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    g_object_set_data_full(
        G_OBJECT(row), "page-name", g_strdup(page_name), g_free);
    g_object_set_data_full(
        G_OBJECT(row), "search-text", g_strdup(search_text), g_free);
    return row;
}

static GtkWidget *make_external_navigation_row(const char *icon_name,
                                               const char *title,
                                               const char *subtitle,
                                               const char *search_text,
                                               const char *program,
                                               const char *argument,
                                               const char *accent_class)
{
    GtkWidget *row = make_navigation_row(
        icon_name,
        title,
        subtitle,
        NULL,
        search_text);
    g_autofree gchar *path =
        program != NULL ? g_find_program_in_path(program) : NULL;

    if (accent_class != NULL) {
        gtk_widget_add_css_class(row, accent_class);
    }
    g_object_set_data_full(
        G_OBJECT(row),
        "action-program",
        g_strdup(program),
        g_free);
    g_object_set_data_full(
        G_OBJECT(row),
        "action-argument",
        g_strdup(argument),
        g_free);
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
    gtk_widget_add_css_class(row, "nav-cyan");
    g_object_set_data(
        G_OBJECT(row),
        "action-about",
        GINT_TO_POINTER(1));
    return row;
}


static void append_manifest_value(
    GString *search,
    GKeyFile *key_file,
    const char *group,
    const char *key)
{
    g_autofree gchar *value = NULL;

    if (search == NULL || key_file == NULL ||
        group == NULL || key == NULL) {
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
        override_active ? override_dir : SYSTEM_SETTINGS_MODULE_DIR,
        override_active ? NULL : SYSTEM_SETTINGS_MODULE_SOURCE_DIR,
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
        g_autoptr(GString) search = NULL;

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

        return g_string_free(g_steal_pointer(&search), FALSE);
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

static GtkWidget *make_window_control(const char *icon_name,
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

    (void)parent;
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
        search,
        "Filter settings categories and their indexed keywords.");
    gtk_widget_add_css_class(search, "settings-search");
    gtk_widget_set_size_request(search, 240, -1);
    gtk_widget_add_css_class(header_end, "header-end");

    g_signal_connect(
        minimize, "clicked", G_CALLBACK(minimize_window), parent);
    g_signal_connect(
        maximize, "clicked", G_CALLBACK(toggle_maximize_window), parent);
    g_signal_connect(
        close, "clicked", G_CALLBACK(close_window), parent);
    /*
     * Keep the search field to the left of the conventional window controls.
     * Packing each item independently with GtkHeaderBar::pack_end reverses the
     * apparent order at the trailing edge. One explicit box makes the visual
     * contract deterministic: Search | Minimize | Maximize | Close.
     */
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

static GtkWidget *build_sidebar(GtkWindow *parent,
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
    g_autofree gchar *version =
        g_strdup_printf("Version %s", info->version);
    g_autofree gchar *date_search = date_time_search_text();

    gtk_widget_set_size_request(sidebar, 230, -1);
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
    gtk_widget_add_css_class(home_row, "nav-gold");

    date_row = make_navigation_row(
        "preferences-system-time-symbolic",
        "Date & Time",
        "Clock, calendar & location",
        "date-time",
        date_search);
    gtk_widget_add_css_class(date_row, "nav-gold");

    region_row = make_external_navigation_row(
        "preferences-desktop-locale-symbolic",
        "Region & Language",
        "Language, formats & input",
        "region language locale formats input",
        "mintlocale",
        NULL,
        "nav-gold");

    appearance_row = make_external_navigation_row(
        "preferences-desktop-theme-symbolic",
        "Appearance",
        "Cinnamon themes & desktop appearance",
        "appearance theme themes desktop cinnamon",
        "cinnamon-settings",
        "themes",
        "nav-gold");

    sound_row = make_external_navigation_row(
        "audio-volume-high-symbolic",
        "Sound",
        "Audio devices & volume",
        "sound audio devices volume speakers",
        "cinnamon-settings",
        "sound",
        "nav-gold");

    network_row = make_external_navigation_row(
        "network-wired-symbolic",
        "Network",
        "Wi-Fi, wired & internet",
        "network wifi wireless ethernet internet",
        "cinnamon-settings",
        "network",
        "nav-cyan");

    bluetooth_row = make_external_navigation_row(
        "bluetooth-active-symbolic",
        "Bluetooth",
        "Devices & pairing",
        "bluetooth devices pairing",
        "blueman-manager",
        NULL,
        "nav-cyan");

    power_row = make_external_navigation_row(
        "battery-good-symbolic",
        "Power",
        "Battery & power management",
        "power battery energy management",
        "cinnamon-settings",
        "power",
        "nav-gold");

    users_row = make_external_navigation_row(
        "system-users-symbolic",
        "Users & Accounts",
        "Account settings & login",
        "users accounts login password",
        "cinnamon-settings",
        "user",
        "nav-gold");

    privacy_row = make_external_navigation_row(
        "security-high-symbolic",
        "Privacy & Security",
        "Permissions & system security",
        "privacy security permissions",
        "cinnamon-settings",
        "privacy",
        "nav-gold");

    hardware_row = make_external_navigation_row(
        "computer-symbolic",
        "System Information",
        "Hardware and operating-system details",
        "hardware system information operating system details",
        "cinnamon-settings",
        "info",
        "nav-gold");

    software_row = make_external_navigation_row(
        "system-software-install-symbolic",
        "Software & Updates",
        "Updates, drivers & repositories",
        "software updates drivers repositories packages",
        "infiltrator-software",
        NULL,
        "nav-gold");

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

    gtk_list_box_set_selection_mode(
        GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
    g_signal_connect(
        list, "row-selected",
        G_CALLBACK(on_navigation_selected), stack);
    g_signal_connect(
        list, "row-activated",
        G_CALLBACK(on_navigation_activated), search_state);

    if (search_state != NULL) {
        search_state->list = GTK_LIST_BOX(list);
        search_state->parent = parent;
        search_state->stack = stack;
        search_state->home_row = GTK_LIST_BOX_ROW(home_row);
        search_state->date_row = GTK_LIST_BOX_ROW(date_row);
        gtk_list_box_set_filter_func(
            GTK_LIST_BOX(list),
            navigation_filter,
            search_state,
            NULL);
        if (search_entry != NULL) {
            g_signal_connect(
                search_entry, "search-changed",
                G_CALLBACK(on_search_changed), search_state);
        }
    }

    /*
     * Selection is established by the host after stack pages exist. Selecting
     * here used to emit row-selected while "home" had not yet been added.
     */
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(nav_scroller),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_overlay_scrolling(
        GTK_SCROLLED_WINDOW(nav_scroller), FALSE);
    gtk_widget_set_vexpand(nav_scroller, TRUE);
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(nav_scroller), list);
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

static GtkWidget *make_feature(const char *icon_name,
                               const char *title,
                               const char *copy)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

    gtk_widget_add_css_class(box, "home-feature");
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_halign(box, GTK_ALIGN_FILL);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 22);
    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(
        GTK_BOX(text),
        ss_linux_ui_make_label(title, "home-feature-title"));
    gtk_box_append(
        GTK_BOX(text),
        ss_linux_ui_make_label(copy, "home-feature-copy"));
    gtk_box_append(GTK_BOX(box), text);
    return box;
}

static GtkWidget *append_overview_row(GtkGrid *grid,
                                     int row,
                                     const char *label,
                                     const char *value)
{
    GtkWidget *key = ss_linux_ui_make_label(label, "overview-key");
    GtkWidget *data = ss_linux_ui_make_label(
        value != NULL && value[0] != '\0' ? value : "Unknown",
        "overview-data");

    gtk_widget_set_halign(key, GTK_ALIGN_START);
    gtk_widget_set_halign(data, GTK_ALIGN_START);
    gtk_grid_attach(grid, key, 0, row, 1, 1);
    gtk_grid_attach(grid, data, 1, row, 1, 1);
    return data;
}

typedef struct {
    GtkWidget *owner;
    GtkLabel *clock;
    GtkLabel *date;
    GtkLabel *system_time;
    SsHomeTemporalPresenter *presenter;
    guint source_id;
} HomeTemporalTicker;

typedef struct {
    GtkWidget *owner;
    GtkLabel *uptime;
    GtkLabel *date_timezone;
    GtkLabel *region_value;
    GtkLabel *region_detail;
    GtkLabel *appearance_value;
    GtkLabel *appearance_detail;
    GtkLabel *network_value;
    GtkLabel *network_detail;
    guint source_id;
} HomeStatusTicker;

static gboolean refresh_home_temporal(gpointer user_data)
{
    HomeTemporalTicker *ticker = user_data;
    SsHomeTemporalPresentation temporal;

    if (ticker == NULL ||
        ticker->clock == NULL ||
        ticker->date == NULL ||
        ticker->system_time == NULL) {
        return G_SOURCE_REMOVE;
    }
    ss_home_temporal_presentation_init(&temporal);
    if (ticker->presenter != NULL &&
        ss_home_temporal_presenter_format_now(
            ticker->presenter, &temporal)) {
        gtk_label_set_text(ticker->clock, temporal.clock_text);
        gtk_label_set_text(ticker->date, temporal.date_text);
        gtk_label_set_text(ticker->system_time, temporal.system_time_text);
    }
    ss_home_temporal_presentation_clear(&temporal);
    return G_SOURCE_CONTINUE;
}

static void stop_home_temporal_ticker(HomeTemporalTicker *ticker)
{
    if (ticker != NULL && ticker->source_id != 0U) {
        g_source_remove(ticker->source_id);
        ticker->source_id = 0U;
    }
}

static void start_home_temporal_ticker(HomeTemporalTicker *ticker)
{
    if (ticker == NULL || ticker->source_id != 0U) {
        return;
    }
    (void)refresh_home_temporal(ticker);
    ticker->source_id = g_timeout_add(
        250U, refresh_home_temporal, ticker);
    g_source_set_name_by_id(
        ticker->source_id,
        "[system-settings] visible Home temporal presentation");
}

static void home_temporal_mapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    start_home_temporal_ticker(user_data);
}

static void home_temporal_unmapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    stop_home_temporal_ticker(user_data);
}

static void home_temporal_ticker_free(gpointer data)
{
    HomeTemporalTicker *ticker = data;
    if (ticker == NULL) return;
    stop_home_temporal_ticker(ticker);
    ss_home_temporal_presenter_free(ticker->presenter);
    g_free(ticker);
}

static void open_date_time(GtkButton *button, gpointer user_data)
{
    ShellSearchState *state = user_data;
    (void)button;
    if (state == NULL || state->stack == NULL) return;
    gtk_stack_set_visible_child_name(state->stack, "date-time");
    if (state->list != NULL && state->date_row != NULL) {
        gtk_list_box_select_row(state->list, state->date_row);
    }
}

static void launch_external_program(GtkButton *button, gpointer user_data)
{
    (void)user_data;
    (void)launch_command(
        GTK_WIDGET(button),
        g_object_get_data(G_OBJECT(button), "action-program"),
        g_object_get_data(G_OBJECT(button), "action-argument"));
}

static GtkWidget *make_quick_action(const char *icon_name,
                                    const char *title,
                                    const char *copy)
{
    GtkWidget *button = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 11);
    GtkWidget *icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *arrow = gtk_image_new_from_icon_name("go-next-symbolic");

    gtk_widget_add_css_class(button, "quick-action");
    gtk_widget_add_css_class(icon_wrap, "quick-action-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 30);
    gtk_box_append(GTK_BOX(icon_wrap), icon);
    gtk_box_append(GTK_BOX(box), icon_wrap);
    gtk_box_append(
        GTK_BOX(text),
        ss_linux_ui_make_label(title, "quick-action-title"));
    gtk_box_append(
        GTK_BOX(text),
        ss_linux_ui_make_label(copy, "quick-action-copy"));
    gtk_widget_set_hexpand(text, TRUE);
    gtk_box_append(GTK_BOX(box), text);
    gtk_image_set_pixel_size(GTK_IMAGE(arrow), 17);
    gtk_widget_add_css_class(arrow, "quick-action-arrow");
    gtk_box_append(GTK_BOX(box), arrow);
    gtk_button_set_child(GTK_BUTTON(button), box);
    return button;
}

static GtkWidget *make_program_action(const char *icon_name,
                                      const char *title,
                                      const char *copy,
                                      const char *program,
                                      const char *argument)
{
    GtkWidget *button = make_quick_action(icon_name, title, copy);
    g_autofree gchar *path =
        program != NULL ? g_find_program_in_path(program) : NULL;

    g_object_set_data_full(
        G_OBJECT(button),
        "action-program",
        g_strdup(program),
        g_free);
    g_object_set_data_full(
        G_OBJECT(button),
        "action-argument",
        g_strdup(argument),
        g_free);

    if (path == NULL) {
        gtk_widget_set_sensitive(button, FALSE);
        gtk_widget_set_tooltip_text(
            button, "This application is not installed.");
    } else {
        g_signal_connect(
            button, "clicked",
            G_CALLBACK(launch_external_program),
            NULL);
    }
    return button;
}

static GtkWidget *make_status_card(const char *css_class,
                                   const char *icon_name,
                                   const char *title,
                                   const char *value,
                                   const char *detail)
{
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *value_label = ss_linux_ui_make_label(
        value != NULL && value[0] != '\0' ? value : "Unknown",
        "status-card-value");
    GtkWidget *detail_label = ss_linux_ui_make_label(
        detail != NULL ? detail : "",
        "status-card-detail");

    gtk_widget_add_css_class(card, "status-card");
    if (css_class != NULL && css_class[0] != '\0') {
        gtk_widget_add_css_class(card, css_class);
    }

    gtk_widget_add_css_class(heading, "status-card-heading");
    gtk_widget_add_css_class(icon_wrap, "status-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 21);
    gtk_box_append(GTK_BOX(icon_wrap), icon);
    gtk_box_append(GTK_BOX(heading), icon_wrap);
    gtk_box_append(
        GTK_BOX(heading),
        ss_linux_ui_make_label(title, "home-card-title"));
    gtk_box_append(GTK_BOX(card), heading);

    gtk_label_set_ellipsize(
        GTK_LABEL(value_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_wrap(GTK_LABEL(detail_label), TRUE);
    gtk_box_append(GTK_BOX(card), value_label);
    gtk_box_append(GTK_BOX(card), detail_label);
    g_object_set_data(G_OBJECT(card), "status-value-label", value_label);
    g_object_set_data(G_OBJECT(card), "status-detail-label", detail_label);
    return card;
}

static const char *network_connectivity_text(GNetworkConnectivity connectivity)
{
    switch (connectivity) {
    case G_NETWORK_CONNECTIVITY_LOCAL:
        return "Local network only";
    case G_NETWORK_CONNECTIVITY_LIMITED:
        return "Limited connectivity";
    case G_NETWORK_CONNECTIVITY_PORTAL:
        return "Sign-in required";
    case G_NETWORK_CONNECTIVITY_FULL:
        return "Full internet access";
    default:
        return "Connectivity unknown";
    }
}

#ifndef SYSTEM_SETTINGS_UI_ASSET_DIR
#define SYSTEM_SETTINGS_UI_ASSET_DIR "/usr/share/infiltrator/system-settings/ui"
#endif
#ifndef SYSTEM_SETTINGS_UI_SOURCE_DIR
#define SYSTEM_SETTINGS_UI_SOURCE_DIR ""
#endif

static GtkWidget *make_ui_asset_picture(const char *filename,
                                        int width,
                                        int height,
                                        const char *css_class)
{
    g_autofree gchar *path = NULL;
    GtkWidget *picture;

    if (filename == NULL || filename[0] == '\0') {
        return NULL;
    }
    const char *override_dir =
        g_getenv("SYSTEM_SETTINGS_UI_ASSET_DIR_OVERRIDE");
    const bool override_active =
        override_dir != NULL && override_dir[0] != '\0';
    const char *asset_dir = override_active
        ? override_dir
        : SYSTEM_SETTINGS_UI_ASSET_DIR;

    path = g_build_filename(asset_dir, filename, NULL);
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR) &&
        !override_active &&
        SYSTEM_SETTINGS_UI_SOURCE_DIR[0] != '\0') {
        g_clear_pointer(&path, g_free);
        path = g_build_filename(
            SYSTEM_SETTINGS_UI_SOURCE_DIR, filename, NULL);
    }
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        return NULL;
    }

    picture = gtk_picture_new_for_filename(path);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
#if GTK_CHECK_VERSION(4, 8, 0)
    gtk_picture_set_content_fit(
        GTK_PICTURE(picture), GTK_CONTENT_FIT_COVER);
#else
    gtk_picture_set_keep_aspect_ratio(
        GTK_PICTURE(picture), FALSE);
#endif
    gtk_widget_set_size_request(picture, width, height);
    if (css_class != NULL) {
        gtk_widget_add_css_class(picture, css_class);
    }
    return picture;
}

static GtkWidget *make_visual_panel(const char *filename,
                                    int width,
                                    int height,
                                    const char *css_class)
{
    GtkWidget *picture = make_ui_asset_picture(
        filename, width, height, css_class);
    GtkWidget *fallback;

    if (picture != NULL) {
        return picture;
    }

    fallback = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(fallback, "asset-missing");
    if (css_class != NULL) {
        gtk_widget_add_css_class(fallback, css_class);
    }
    gtk_widget_set_size_request(fallback, width, height);
    gtk_widget_set_tooltip_text(
        fallback, "Installed visual asset is unavailable.");
    return fallback;
}

static GtkWidget *make_theme_preview(const char *label,
                                     const char *class_name,
                                     gboolean selected)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    g_autofree gchar *asset_name =
        g_strdup_printf("%s.png", class_name);
    GtkWidget *preview = make_ui_asset_picture(
        asset_name, 76, 44, "theme-preview-window");

    if (preview == NULL) {
        preview = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(preview, "theme-preview-window");
        gtk_widget_add_css_class(preview, "asset-missing");
        gtk_widget_set_size_request(preview, 76, 44);
    }

    gtk_widget_add_css_class(box, "theme-preview");
    if (selected) {
        gtk_widget_add_css_class(box, "theme-preview-selected");
    }
    gtk_box_append(GTK_BOX(box), preview);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_box_append(
        GTK_BOX(box),
        ss_linux_ui_make_label(label, "theme-preview-label"));
    return box;
}

static gchar *format_uptime(void)
{
    g_autofree gchar *contents = NULL;
    gsize length = 0U;
    double seconds = 0.0;

    if (!g_file_get_contents("/proc/uptime", &contents, &length, NULL) ||
        contents == NULL ||
        sscanf(contents, "%lf", &seconds) != 1 ||
        seconds < 0.0) {
        return g_strdup("Unknown");
    }

    const guint64 total = (guint64)seconds;
    const guint64 days = total / 86400U;
    const guint64 hours = (total % 86400U) / 3600U;
    const guint64 minutes = (total % 3600U) / 60U;
    if (days > 0U) {
        return g_strdup_printf("%" G_GUINT64_FORMAT "d %" G_GUINT64_FORMAT "h", days, hours);
    }
    if (hours > 0U) {
        return g_strdup_printf("%" G_GUINT64_FORMAT "h %" G_GUINT64_FORMAT "m", hours, minutes);
    }
    return g_strdup_printf("%" G_GUINT64_FORMAT "m", minutes);
}

static const char *current_language_label(void)
{
    const gchar *const *languages = g_get_language_names();
    return languages != NULL && languages[0] != NULL
        ? languages[0]
        : "Unknown";
}

static const char *current_format_locale_label(void)
{
    const char *locale_name = setlocale(LC_TIME, NULL);
    return locale_name != NULL && locale_name[0] != '\0'
        ? locale_name
        : "Unknown";
}

static gboolean refresh_home_status(gpointer user_data)
{
    HomeStatusTicker *ticker = user_data;
    g_autofree gchar *uptime = NULL;
    GtkSettings *settings = gtk_settings_get_default();
    gchar *theme_name = NULL;
    gboolean prefer_dark = FALSE;
    GNetworkMonitor *network = g_network_monitor_get_default();
    gboolean online = FALSE;
    gboolean metered = FALSE;
    GNetworkConnectivity connectivity = G_NETWORK_CONNECTIVITY_LOCAL;
    const char *format_locale = current_format_locale_label();
    const char *language_name = current_language_label();
    g_autofree gchar *region_detail = NULL;
    g_autofree gchar *appearance_detail = NULL;
    g_autofree gchar *network_detail = NULL;
    g_autoptr(GTimeZone) local_zone = g_time_zone_new_local();

    if (ticker == NULL) return G_SOURCE_REMOVE;
    uptime = format_uptime();
    if (ticker->uptime != NULL) gtk_label_set_text(ticker->uptime, uptime);
    if (ticker->date_timezone != NULL && local_zone != NULL) {
        gtk_label_set_text(
            ticker->date_timezone,
            g_time_zone_get_identifier(local_zone));
    }
    if (ticker->region_value != NULL) {
        gtk_label_set_text(ticker->region_value, format_locale);
    }
    region_detail = g_strdup_printf(
        "Date/time format locale • %s • Interface language • %s",
        format_locale,
        language_name);
    if (ticker->region_detail != NULL) {
        gtk_label_set_text(ticker->region_detail, region_detail);
    }
    if (settings != NULL) {
        g_object_get(settings,
            "gtk-theme-name", &theme_name,
            "gtk-application-prefer-dark-theme", &prefer_dark,
            NULL);
    }
    if (ticker->appearance_value != NULL) {
        gtk_label_set_text(
            ticker->appearance_value,
            theme_name != NULL ? theme_name : "System theme");
    }
    appearance_detail = g_strdup_printf(
        "%s presentation", prefer_dark ? "Dark" : "Light");
    if (ticker->appearance_detail != NULL) {
        gtk_label_set_text(ticker->appearance_detail, appearance_detail);
    }
    if (network != NULL) {
        online = g_network_monitor_get_network_available(network);
        metered = g_network_monitor_get_network_metered(network);
        connectivity = g_network_monitor_get_connectivity(network);
    }
    if (ticker->network_value != NULL) {
        gtk_label_set_text(
            ticker->network_value, online ? "Connected" : "Offline");
    }
    network_detail = g_strdup_printf(
        "%s%s", network_connectivity_text(connectivity),
        metered ? " • Metered" : "");
    if (ticker->network_detail != NULL) {
        gtk_label_set_text(ticker->network_detail, network_detail);
    }
    g_free(theme_name);
    return G_SOURCE_CONTINUE;
}

static void stop_home_status_ticker(HomeStatusTicker *ticker)
{
    if (ticker != NULL && ticker->source_id != 0U) {
        g_source_remove(ticker->source_id);
        ticker->source_id = 0U;
    }
}

static void start_home_status_ticker(HomeStatusTicker *ticker)
{
    if (ticker == NULL || ticker->source_id != 0U) {
        return;
    }
    (void)refresh_home_status(ticker);
    ticker->source_id = g_timeout_add_seconds(
        5U, refresh_home_status, ticker);
    g_source_set_name_by_id(
        ticker->source_id,
        "[system-settings] visible Home system status");
}

static void home_status_mapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    start_home_status_ticker(user_data);
}

static void home_status_unmapped(
    GtkWidget *widget G_GNUC_UNUSED,
    gpointer user_data)
{
    stop_home_status_ticker(user_data);
}

static void home_status_ticker_free(gpointer data)
{
    HomeStatusTicker *ticker = data;
    if (ticker == NULL) return;
    stop_home_status_ticker(ticker);
    g_free(ticker);
}

static GtkWidget *build_home_page(
    GtkStack *stack,
    ShellSearchState *search_state)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *hero = gtk_overlay_new();
    GtkWidget *hero_scene = make_visual_panel(
        "hero-asset.png", -1, 212, "hero-scene");
    GtkWidget *hero_foreground = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    GtkWidget *hero_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *hero_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *hero_brand = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *hero_icon = gtk_image_new_from_icon_name(
        "video-display-symbolic");
    GtkWidget *features = gtk_flow_box_new();
    GtkWidget *grid = gtk_flow_box_new();
    GtkWidget *overview = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *overview_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *overview_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *overview_icon = gtk_image_new_from_icon_name(
        "video-display-symbolic");
    GtkWidget *overview_data = gtk_grid_new();
    GtkWidget *overview_body = gtk_flow_box_new();
    GtkWidget *system_time_label;
    GtkWidget *overview_scene = make_visual_panel(
        "overview-asset.png", 120, 96, "overview-scene");
    GtkWidget *quick = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *quick_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *quick_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *quick_icon = gtk_image_new_from_icon_name(
        "system-run-symbolic");
    GtkWidget *quick_grid = gtk_flow_box_new();
    GtkWidget *status_grid = gtk_flow_box_new();
    GtkWidget *date_card;
    GtkWidget *region_card;
    GtkWidget *appearance_card;
    GtkWidget *network_card;
    GtkWidget *date_open;
    GtkWidget *date_meta;
    GtkWidget *date_location;
    GtkWidget *region_body;
    GtkWidget *region_copy;
    GtkWidget *appearance_previews;
    GtkWidget *network_body;
    GtkWidget *scroller = gtk_scrolled_window_new();
    struct utsname uts;
    g_autofree gchar *os_name = g_get_os_info(G_OS_INFO_KEY_PRETTY_NAME);
    const char *desktop = g_getenv("XDG_CURRENT_DESKTOP");
    const char *host = g_get_host_name();
    const int uname_result = uname(&uts);
    const char *kernel = uname_result == 0 ? uts.release : "Unknown";
    const char *architecture = uname_result == 0 ? uts.machine : "Unknown";
    const char *format_locale = current_format_locale_label();
    const char *language_name = current_language_label();
    GtkSettings *gtk_settings = gtk_settings_get_default();
    gchar *theme_name = NULL;
    gboolean prefer_dark = FALSE;
    GNetworkMonitor *network = g_network_monitor_get_default();
    gboolean online = FALSE;
    gboolean metered = FALSE;
    GNetworkConnectivity connectivity = G_NETWORK_CONNECTIVITY_LOCAL;
    SsHomeTemporalPresentation temporal;
    g_autofree gchar *region_detail = NULL;
    g_autofree gchar *appearance_detail = NULL;
    g_autofree gchar *network_detail = NULL;
    g_autofree gchar *uptime_text = format_uptime();
    g_autoptr(GTimeZone) local_zone = g_time_zone_new_local();
    const char *timezone_name =
        local_zone != NULL ? g_time_zone_get_identifier(local_zone) : "Unknown";
    g_autofree gchar *os_display = g_strdup_printf(
        "Infiltrator OS (%s)",
        os_name != NULL ? os_name : "Linux");
    ss_home_temporal_presentation_init(&temporal);
    SsHomeTemporalPresenter *home_presenter =
        ss_home_temporal_presenter_new();
    if (home_presenter == NULL ||
        !ss_home_temporal_presenter_format_now(
            home_presenter, &temporal)) {
        temporal.clock_text = g_strdup("Unknown");
        temporal.date_text = g_strdup("");
        temporal.system_time_text = g_strdup("Unknown");
    }

    if (gtk_settings != NULL) {
        g_object_get(
            gtk_settings,
            "gtk-theme-name", &theme_name,
            "gtk-application-prefer-dark-theme", &prefer_dark,
            NULL);
    }
    if (network != NULL) {
        online = g_network_monitor_get_network_available(network);
        metered = g_network_monitor_get_network_metered(network);
        connectivity = g_network_monitor_get_connectivity(network);
    }

    region_detail = g_strdup_printf(
        "Date/time format locale • %s • Interface language • %s",
        format_locale,
        language_name);
    appearance_detail = g_strdup_printf(
        "%s presentation",
        prefer_dark ? "Dark" : "Light");
    network_detail = g_strdup_printf(
        "%s%s",
        network_connectivity_text(connectivity),
        metered ? " • Metered" : "");

    gtk_widget_add_css_class(page, "home-page");
    gtk_widget_add_css_class(hero, "home-hero");
    gtk_overlay_set_child(GTK_OVERLAY(hero), hero_scene);

    gtk_widget_add_css_class(hero_copy, "hero-copy-overlay");
    gtk_widget_set_valign(hero_copy, GTK_ALIGN_CENTER);
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label("SYSTEM CONTROL", "home-hero-eyebrow"));
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label("Welcome to", "home-hero-title"));
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label("System Settings", "home-hero-accent"));
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label(
            "Configure your system, your way.",
            "home-hero-subtitle"));

    gtk_widget_add_css_class(features, "home-feature-row");
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(features), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(features), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(features), 3U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(features), 10U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(features), 8U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(features), TRUE);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(features),
        make_feature("emblem-ok-symbolic", "Simple", "Easy to use"), -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(features),
        make_feature("security-high-symbolic", "Secure", "Built for privacy"), -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(features),
        make_feature("video-display-symbolic", "Beautiful", "A desktop you’ll love"), -1);
    gtk_box_append(GTK_BOX(hero_copy), features);

    gtk_widget_set_hexpand(hero_spacer, TRUE);
    gtk_widget_set_halign(hero_brand, GTK_ALIGN_END);
    gtk_widget_set_valign(hero_brand, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(hero_brand, "hero-brand-overlay");
    gtk_image_set_pixel_size(GTK_IMAGE(hero_icon), 48);
    gtk_box_append(GTK_BOX(hero_brand), hero_icon);
    gtk_box_append(
        GTK_BOX(hero_brand),
        ss_linux_ui_make_label("INFILTRATOR OS", "home-hero-mark-title"));
    gtk_box_append(
        GTK_BOX(hero_brand),
        ss_linux_ui_make_label(
            "GRAPHICAL SYSTEM CONTROL",
            "home-hero-mark-copy"));

    gtk_widget_set_hexpand(hero_foreground, TRUE);
    gtk_widget_set_vexpand(hero_foreground, TRUE);
    gtk_box_append(GTK_BOX(hero_foreground), hero_copy);
    gtk_box_append(GTK_BOX(hero_foreground), hero_spacer);
    gtk_box_append(GTK_BOX(hero_foreground), hero_brand);
    gtk_overlay_add_overlay(GTK_OVERLAY(hero), hero_foreground);
    gtk_box_append(GTK_BOX(page), hero);

    gtk_widget_add_css_class(grid, "home-grid");
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(grid), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(grid), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(grid), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(grid), 12U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(grid), 12U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(grid), TRUE);

    gtk_widget_add_css_class(overview, "home-card");
    gtk_widget_add_css_class(overview_icon_wrap, "home-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(overview_icon), 22);
    gtk_box_append(GTK_BOX(overview_icon_wrap), overview_icon);
    gtk_box_append(GTK_BOX(overview_heading), overview_icon_wrap);
    gtk_box_append(
        GTK_BOX(overview_heading),
        ss_linux_ui_make_label("System Overview", "home-card-title"));
    GtkWidget *overview_spacer =
        gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *updates_button =
        gtk_button_new_with_label("Check for updates →");
    gtk_widget_set_hexpand(overview_spacer, TRUE);
    gtk_box_append(GTK_BOX(overview_heading), overview_spacer);
    gtk_widget_add_css_class(updates_button, "overview-link-button");
    g_object_set_data_full(
        G_OBJECT(updates_button),
        "action-program",
        g_strdup("infiltrator-software"),
        g_free);
    g_autofree gchar *software_path =
        g_find_program_in_path("infiltrator-software");
    if (software_path == NULL) {
        gtk_widget_set_sensitive(updates_button, FALSE);
        gtk_widget_set_tooltip_text(
            updates_button, "Infiltrator Software is not installed.");
    } else {
        g_signal_connect(
            updates_button, "clicked",
            G_CALLBACK(launch_external_program), NULL);
    }
    gtk_box_append(GTK_BOX(overview_heading), updates_button);
    gtk_box_append(GTK_BOX(overview), overview_heading);
    gtk_grid_set_column_spacing(GTK_GRID(overview_data), 18);
    gtk_grid_set_row_spacing(GTK_GRID(overview_data), 8);
    append_overview_row(
        GTK_GRID(overview_data), 0, "Operating system", os_display);
    append_overview_row(
        GTK_GRID(overview_data), 1, "Kernel", kernel);
    append_overview_row(
        GTK_GRID(overview_data), 2, "Architecture", architecture);
    append_overview_row(
        GTK_GRID(overview_data), 3, "Desktop", desktop);
    append_overview_row(
        GTK_GRID(overview_data), 4, "Hostname", host);
    GtkWidget *uptime_label = append_overview_row(
        GTK_GRID(overview_data), 5, "Uptime", uptime_text);
    system_time_label = append_overview_row(
        GTK_GRID(overview_data), 6, "System time", temporal.system_time_text);
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(overview_body), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(
        GTK_FLOW_BOX(overview_body), 1U);
    gtk_flow_box_set_max_children_per_line(
        GTK_FLOW_BOX(overview_body), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(overview_body), 15U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(overview_body), 10U);
    gtk_flow_box_insert(GTK_FLOW_BOX(overview_body), overview_scene, -1);
    gtk_widget_set_hexpand(overview_data, TRUE);
    gtk_flow_box_insert(GTK_FLOW_BOX(overview_body), overview_data, -1);
    gtk_box_append(GTK_BOX(overview), overview_body);
    gtk_flow_box_insert(GTK_FLOW_BOX(grid), overview, -1);

    gtk_widget_add_css_class(quick, "home-card");
    gtk_widget_add_css_class(quick_icon_wrap, "home-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(quick_icon), 22);
    gtk_box_append(GTK_BOX(quick_icon_wrap), quick_icon);
    gtk_box_append(GTK_BOX(quick_heading), quick_icon_wrap);
    gtk_box_append(
        GTK_BOX(quick_heading),
        ss_linux_ui_make_label("Quick Actions", "home-card-title"));
    gtk_box_append(GTK_BOX(quick), quick_heading);

    gtk_widget_add_css_class(quick_grid, "quick-action-grid");
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(quick_grid), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(quick_grid), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(quick_grid), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(quick_grid), 8U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(quick_grid), 8U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(quick_grid), TRUE);

    GtkWidget *date_action = make_quick_action(
        "preferences-system-time-symbolic",
        "Set Date & Time",
        "Time zone, clock & calendar");
    gtk_widget_add_css_class(date_action, "quick-action-cyan");
    g_signal_connect(
        date_action, "clicked",
        G_CALLBACK(open_date_time), search_state);
    gtk_flow_box_insert(GTK_FLOW_BOX(quick_grid), date_action, -1);

    GtkWidget *region_action = make_program_action(
        "preferences-desktop-locale-symbolic",
        "Change Region",
        "Language, formats & location",
        "mintlocale",
        NULL);
    gtk_widget_add_css_class(region_action, "quick-action-cyan");
    gtk_flow_box_insert(GTK_FLOW_BOX(quick_grid), region_action, -1);

    GtkWidget *display_action = make_program_action(
        "video-display-symbolic",
        "Configure Display",
        "Scaling, layout & monitors",
        "cinnamon-settings",
        "display");
    gtk_widget_add_css_class(display_action, "quick-action-cyan");
    gtk_flow_box_insert(GTK_FLOW_BOX(quick_grid), display_action, -1);

    GtkWidget *software_action = make_program_action(
        "system-software-install-symbolic",
        "Software & Updates",
        "Apps, packages & updates",
        "infiltrator-software",
        NULL);
    gtk_widget_add_css_class(software_action, "quick-action-gold");
    gtk_flow_box_insert(GTK_FLOW_BOX(quick_grid), software_action, -1);

    gtk_box_append(GTK_BOX(quick), quick_grid);
    gtk_flow_box_insert(GTK_FLOW_BOX(grid), quick, -1);
    gtk_box_append(GTK_BOX(page), grid);

    gtk_widget_add_css_class(status_grid, "status-grid");
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(status_grid), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(status_grid), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(status_grid), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(status_grid), 12U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(status_grid), 12U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(status_grid), TRUE);

    date_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 9);
    gtk_widget_add_css_class(date_card, "status-card");
    gtk_widget_add_css_class(date_card, "date-status-card");
    GtkWidget *date_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *date_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *date_icon = gtk_image_new_from_icon_name(
        "preferences-system-time-symbolic");
    gtk_widget_add_css_class(date_icon_wrap, "status-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(date_icon), 22);
    gtk_box_append(GTK_BOX(date_icon_wrap), date_icon);
    gtk_box_append(GTK_BOX(date_heading), date_icon_wrap);
    gtk_box_append(
        GTK_BOX(date_heading),
        ss_linux_ui_make_label("Date & Time", "home-card-title"));
    date_open = gtk_button_new_from_icon_name("go-next-symbolic");
    gtk_widget_set_tooltip_text(date_open, "Open Date & Time");
    gtk_accessible_update_property(
        GTK_ACCESSIBLE(date_open),
        GTK_ACCESSIBLE_PROPERTY_LABEL,
        "Open Date & Time",
        -1);
    gtk_widget_add_css_class(date_open, "card-arrow");
    gtk_widget_set_halign(date_open, GTK_ALIGN_END);
    gtk_widget_set_hexpand(date_open, TRUE);
    g_signal_connect(
        date_open, "clicked",
        G_CALLBACK(open_date_time), search_state);
    gtk_box_append(GTK_BOX(date_heading), date_open);
    gtk_box_append(GTK_BOX(date_card), date_heading);

    GtkWidget *date_body = gtk_flow_box_new();
    GtkWidget *date_clock = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *date_clock_label =
        ss_linux_ui_make_label(temporal.clock_text, "home-clock-value");
    GtkWidget *date_calendar_label =
        ss_linux_ui_make_label(temporal.date_text, "home-clock-date");
    gtk_box_append(GTK_BOX(date_clock), date_clock_label);
    gtk_label_set_wrap(GTK_LABEL(date_calendar_label), TRUE);
    gtk_box_append(GTK_BOX(date_clock), date_calendar_label);
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(date_body), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(date_body), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(date_body), 3U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(date_body), 16U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(date_body), 10U);
    gtk_flow_box_insert(GTK_FLOW_BOX(date_body), date_clock, -1);

    date_meta = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *pin_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *pin = gtk_image_new_from_icon_name("mark-location-symbolic");
    gtk_widget_add_css_class(pin_wrap, "location-pin-well");
    gtk_image_set_pixel_size(GTK_IMAGE(pin), 24);
    gtk_box_append(GTK_BOX(pin_wrap), pin);
    gtk_box_append(GTK_BOX(date_meta), pin_wrap);
    date_location = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *date_timezone_label =
        ss_linux_ui_make_label(timezone_name, "location-primary");
    gtk_box_append(GTK_BOX(date_location), date_timezone_label);
    gtk_box_append(
        GTK_BOX(date_location),
        ss_linux_ui_make_label(
            "Authoritative local time zone",
            "status-card-detail"));
    gtk_box_append(GTK_BOX(date_meta), date_location);
    gtk_flow_box_insert(GTK_FLOW_BOX(date_body), date_meta, -1);

    GtkWidget *date_scene = make_visual_panel(
        "date-asset.png", 120, 78, "date-scene");
    gtk_widget_set_hexpand(date_scene, TRUE);
    gtk_widget_set_halign(date_scene, GTK_ALIGN_END);
    gtk_flow_box_insert(GTK_FLOW_BOX(date_body), date_scene, -1);
    gtk_box_append(GTK_BOX(date_card), date_body);
    gtk_flow_box_insert(GTK_FLOW_BOX(status_grid), date_card, -1);

    region_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(region_card, "status-card");
    gtk_widget_add_css_class(region_card, "region-status-card");
    GtkWidget *region_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *region_icon = gtk_image_new_from_icon_name(
        "preferences-desktop-locale-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(region_icon), 23);
    gtk_box_append(GTK_BOX(region_heading), region_icon);
    gtk_box_append(
        GTK_BOX(region_heading),
        ss_linux_ui_make_label("Region & Language", "home-card-title"));
    gtk_box_append(GTK_BOX(region_card), region_heading);
    region_body = gtk_flow_box_new();
    region_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *region_value_label =
        ss_linux_ui_make_label(format_locale, "region-country");
    GtkWidget *region_detail_label =
        ss_linux_ui_make_label(region_detail, "status-card-detail");
    gtk_label_set_ellipsize(
        GTK_LABEL(region_value_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_wrap(GTK_LABEL(region_detail_label), TRUE);
    gtk_box_append(GTK_BOX(region_copy), region_value_label);
    gtk_box_append(GTK_BOX(region_copy), region_detail_label);
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(region_body), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(region_body), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(region_body), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(region_body), 13U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(region_body), 10U);
    gtk_flow_box_insert(GTK_FLOW_BOX(region_body), region_copy, -1);
    GtkWidget *region_scene = make_ui_asset_picture(
        "region-asset.png", 220, 96, "region-scene");
    if (region_scene != NULL) {
        gtk_widget_set_hexpand(region_scene, TRUE);
        gtk_widget_set_halign(region_scene, GTK_ALIGN_END);
        gtk_flow_box_insert(GTK_FLOW_BOX(region_body), region_scene, -1);
    }
    gtk_box_append(GTK_BOX(region_card), region_body);
    gtk_flow_box_insert(GTK_FLOW_BOX(status_grid), region_card, -1);

    appearance_card = make_status_card(
        "appearance-status-card",
        "preferences-desktop-theme-symbolic",
        "Display & Appearance",
        theme_name != NULL ? theme_name : "System theme",
        appearance_detail);
    GtkWidget *appearance_value_label = g_object_get_data(
        G_OBJECT(appearance_card), "status-value-label");
    GtkWidget *appearance_detail_label = g_object_get_data(
        G_OBJECT(appearance_card), "status-detail-label");
    appearance_previews = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(appearance_previews), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(
        GTK_FLOW_BOX(appearance_previews), 1U);
    gtk_flow_box_set_max_children_per_line(
        GTK_FLOW_BOX(appearance_previews), 4U);
    gtk_flow_box_set_column_spacing(
        GTK_FLOW_BOX(appearance_previews), 6U);
    gtk_flow_box_set_row_spacing(
        GTK_FLOW_BOX(appearance_previews), 6U);
    gtk_flow_box_set_homogeneous(
        GTK_FLOW_BOX(appearance_previews), TRUE);
    /*
     * These are illustrative choices, not an authoritative theme selector.
     * Do not mark Light/Dark as selected merely because GTK currently prefers
     * one luminance; that would falsely imply Follow OS/Mercedes state.
     */
    gtk_flow_box_insert(
        GTK_FLOW_BOX(appearance_previews),
        make_theme_preview("Light", "theme-light", FALSE), -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(appearance_previews),
        make_theme_preview("Dark", "theme-dark", FALSE), -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(appearance_previews),
        make_theme_preview("Follow OS", "theme-follow", FALSE), -1);
    gtk_flow_box_insert(
        GTK_FLOW_BOX(appearance_previews),
        make_theme_preview("Mercedes Grey", "theme-mercedes", FALSE), -1);
    gtk_box_append(GTK_BOX(appearance_card), appearance_previews);
    gtk_flow_box_insert(GTK_FLOW_BOX(status_grid), appearance_card, -1);

    network_card = make_status_card(
        "network-status-card",
        "network-wired-symbolic",
        "Network",
        online ? "Connected" : "Offline",
        network_detail);
    GtkWidget *network_value_label = g_object_get_data(
        G_OBJECT(network_card), "status-value-label");
    GtkWidget *network_detail_label = g_object_get_data(
        G_OBJECT(network_card), "status-detail-label");
    network_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *network_visual = make_ui_asset_picture(
        "network-asset.png", 220, 100, "network-visual");
    if (network_visual == NULL) {
        network_visual = make_visual_panel(
            "network-asset.png", 220, 100, "network-visual");
    }
    gtk_widget_set_hexpand(network_visual, TRUE);
    gtk_widget_set_halign(network_visual, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(network_body), network_visual);
    gtk_box_append(GTK_BOX(network_card), network_body);
    gtk_flow_box_insert(GTK_FLOW_BOX(status_grid), network_card, -1);

    gtk_widget_set_valign(status_grid, GTK_ALIGN_START);
    gtk_widget_set_vexpand(status_grid, FALSE);
    gtk_box_append(GTK_BOX(page), status_grid);

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroller),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_overlay_scrolling(
        GTK_SCROLLED_WINDOW(scroller), FALSE);
    gtk_scrolled_window_set_kinetic_scrolling(
        GTK_SCROLLED_WINDOW(scroller), TRUE);
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_widget_set_valign(page, GTK_ALIGN_START);
    gtk_widget_set_vexpand(page, FALSE);
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(scroller), page);
    g_object_set_data(
        G_OBJECT(stack),
        "system-settings-home-scroller",
        scroller);

    /*
     * Home is a live status surface, not a construction-time snapshot.
     * Refresh faster than one conventional second so decimal/French seconds
     * (0.864 civil seconds each) advance without visible stalls or skips.
     * The source is owned by the Home scroller and is removed automatically
     * when that widget is finalized.
     */
    HomeTemporalTicker *ticker = g_new0(HomeTemporalTicker, 1);
    ticker->owner = scroller;
    ticker->clock = GTK_LABEL(date_clock_label);
    ticker->date = GTK_LABEL(date_calendar_label);
    ticker->system_time = GTK_LABEL(system_time_label);
    ticker->presenter = home_presenter;
    home_presenter = NULL;
    g_signal_connect(
        scroller, "map",
        G_CALLBACK(home_temporal_mapped), ticker);
    g_signal_connect(
        scroller, "unmap",
        G_CALLBACK(home_temporal_unmapped), ticker);
    g_object_set_data_full(
        G_OBJECT(scroller),
        "system-settings-home-temporal-source",
        ticker,
        home_temporal_ticker_free);

    HomeStatusTicker *status_ticker = g_new0(HomeStatusTicker, 1);
    status_ticker->owner = scroller;
    status_ticker->uptime = GTK_LABEL(uptime_label);
    status_ticker->date_timezone = GTK_LABEL(date_timezone_label);
    status_ticker->region_value = GTK_LABEL(region_value_label);
    status_ticker->region_detail = GTK_LABEL(region_detail_label);
    status_ticker->appearance_value = GTK_LABEL(appearance_value_label);
    status_ticker->appearance_detail = GTK_LABEL(appearance_detail_label);
    status_ticker->network_value = GTK_LABEL(network_value_label);
    status_ticker->network_detail = GTK_LABEL(network_detail_label);
    g_signal_connect(
        scroller, "map",
        G_CALLBACK(home_status_mapped), status_ticker);
    g_signal_connect(
        scroller, "unmap",
        G_CALLBACK(home_status_unmapped), status_ticker);
    g_object_set_data_full(
        G_OBJECT(scroller),
        "system-settings-home-status-source",
        status_ticker,
        home_status_ticker_free);

    ss_home_temporal_presenter_free(home_presenter);
    g_free(theme_name);
    ss_home_temporal_presentation_clear(&temporal);
    return scroller;
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
        comments, sizeof(comments), "%s\n\nBuild: %s",
        info->comments,
        infiltratr_build_profile_label(info->build_profile));

    dialog = gtk_about_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "About System Settings");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_about_dialog_set_program_name(
        GTK_ABOUT_DIALOG(dialog), info->program_name);
    gtk_about_dialog_set_version(
        GTK_ABOUT_DIALOG(dialog), info->version);
    gtk_about_dialog_set_comments(
        GTK_ABOUT_DIALOG(dialog), comments);
    gtk_about_dialog_set_authors(
        GTK_ABOUT_DIALOG(dialog), authors);
    gtk_about_dialog_set_website(
        GTK_ABOUT_DIALOG(dialog), info->website);
    gtk_about_dialog_set_website_label(
        GTK_ABOUT_DIALOG(dialog), "Website");
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
    gtk_about_dialog_set_wrap_license(
        GTK_ABOUT_DIALOG(dialog), TRUE);
    gtk_about_dialog_set_logo_icon_name(
        GTK_ABOUT_DIALOG(dialog), info->icon_name);
    g_signal_connect(
        dialog, "close-request",
        G_CALLBACK(about_close_requested), NULL);
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
    (void)user_data;
    /* Pending replies retain the window, but may no longer find its panel. */
    g_object_set_data(G_OBJECT(window), "system-settings-date-time-panel", NULL);
    return FALSE;
}

static void on_activate(GtkApplication *application, gpointer user_data)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    g_autoptr(GError) panel_error = NULL;
    GtkWindow *window;
    SsLinuxDateTimePanel *date_time;
    GtkWidget *root;
    GtkWidget *stack_widget;
    GtkStack *stack;
    GtkWidget *date_scroller;
    GtkWidget *panel_widget;
    GtkSearchEntry *search_entry = NULL;
    ShellSearchState *search_state;

    (void)user_data;

    window = gtk_application_get_active_window(application);
    if (window != NULL) {
        gtk_window_present(window);
        return;
    }
    ss_linux_theme_install();
    ss_linux_theme_watch();

    window = GTK_WINDOW(
        gtk_application_window_new(application));
    gtk_window_set_title(window, info->program_name);
    gtk_window_set_default_size(window, 1180, 760);
    gtk_window_set_resizable(window, TRUE);
    g_signal_connect(
        window, "close-request",
        G_CALLBACK(on_close_requested), NULL);

    gtk_window_set_titlebar(
        window, build_header(window, &search_entry));

    root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(root, "app-shell");
    gtk_window_set_child(window, root);

    stack_widget = gtk_stack_new();
    stack = GTK_STACK(stack_widget);
    gtk_stack_set_transition_type(
        stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(stack, 170U);
    gtk_widget_set_hexpand(stack_widget, TRUE);
    gtk_widget_set_vexpand(stack_widget, TRUE);

    search_state = g_new0(ShellSearchState, 1);
    g_object_set_data_full(
        G_OBJECT(window),
        "system-settings-search-state",
        search_state,
        shell_search_state_free);
    gtk_box_append(
        GTK_BOX(root),
        build_sidebar(
            window, stack, search_entry, search_state));
    g_object_set_data(
        G_OBJECT(window),
        "system-settings-navigation-list",
        search_state->list);

    date_time = ss_linux_date_time_panel_new(
        window, &panel_error);
    if (date_time != NULL) {
        /*
         * The host window owns the current built-in module instance. Async
         * module operations retain a window reference while in flight. Closing
         * detaches the panel first, so late replies find NULL rather than widgets.
         */
        g_object_set_data_full(
            G_OBJECT(window),
            "system-settings-date-time-panel",
            date_time,
            ss_linux_date_time_panel_free);
        panel_widget =
            ss_linux_date_time_panel_widget(date_time);
    } else {
        panel_widget = build_unavailable_panel(
            panel_error != NULL
                ? panel_error->message
                : "Unable to initialise Date & Time.");
    }

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
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(date_scroller),
        panel_widget);
    g_object_set_data(
        G_OBJECT(window),
        "system-settings-date-scroller",
        date_scroller);
    gtk_stack_add_named(
        stack,
        build_home_page(stack, search_state),
        "home");
    gtk_stack_add_named(
        stack,
        date_scroller,
        "date-time");
    gtk_stack_set_visible_child_name(stack, "home");
    if (search_state->list != NULL && search_state->home_row != NULL) {
        gtk_list_box_select_row(
            search_state->list, search_state->home_row);
    }
    g_object_set_data(
        G_OBJECT(window),
        "system-settings-stack",
        stack);
    gtk_box_append(GTK_BOX(root), stack_widget);

    gtk_window_present(window);
}

int main(int argc, char **argv)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    g_autoptr(GtkApplication) application =
        gtk_application_new(
            info->application_id,
            G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(
        application, "activate",
        G_CALLBACK(on_activate), NULL);
    return g_application_run(
        G_APPLICATION(application), argc, argv);
}
