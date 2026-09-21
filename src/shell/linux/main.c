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

#include "system-settings/project-info.h"

#include <gtk/gtk.h>
#include <infiltratr/core.h>
#include <infiltratr/design.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static gchar *rgb_css(uint32_t rgb)
{
    return g_strdup_printf("#%06x", (unsigned int)(rgb & UINT32_C(0xFFFFFF)));
}

static bool system_prefers_dark(void)
{
    GtkSettings *settings = gtk_settings_get_default();
    gboolean prefer_dark = FALSE;
    gchar *theme_name = NULL;
    bool dark = false;

    if (settings == NULL) {
        return false;
    }

    g_object_get(settings,
                 "gtk-application-prefer-dark-theme", &prefer_dark,
                 "gtk-theme-name", &theme_name,
                 NULL);
    dark = prefer_dark != FALSE;
    if (!dark && theme_name != NULL) {
        dark = infiltratr_ascii_contains_ci(theme_name, "dark");
    }
    g_free(theme_name);
    return dark;
}

static void install_common_theme(void)
{
    const InfiltratrThemePalette *palette =
        infiltratr_theme_resolve(INFILTRATR_THEME_SYSTEM,
                                 system_prefers_dark());
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    const InfiltratrTypography *type = infiltratr_typography();
    GtkCssProvider *provider;
    GString *css;
    gchar *background;
    gchar *panel;
    gchar *card;
    gchar *surface;
    gchar *input;
    gchar *border;
    gchar *text;
    gchar *title;
    gchar *muted;
    gchar *subtle;
    gchar *accent;
    gchar *accent_foreground;
    gchar *accent_hover;
    gchar *selected;
    gchar *titlebar;
    gchar *success;
    gchar *warning;
    gchar *fault;
    gchar *info;
    gchar *surface_hover;
    gchar *status_border;

    if (palette == NULL || metrics == NULL || type == NULL) {
        return;
    }

    background = rgb_css(palette->background_rgb);
    panel = rgb_css(palette->panel_rgb);
    card = rgb_css(palette->card_rgb);
    surface = rgb_css(palette->surface_rgb);
    input = rgb_css(palette->input_rgb);
    border = rgb_css(palette->border_rgb);
    text = rgb_css(palette->text_rgb);
    title = rgb_css(palette->title_rgb);
    muted = rgb_css(palette->muted_rgb);
    subtle = rgb_css(palette->subtle_rgb);
    accent = rgb_css(palette->neutral_accent_rgb);
    accent_foreground = rgb_css(palette->accent_foreground_rgb);
    accent_hover = rgb_css(palette->accent_hover_rgb);
    selected = rgb_css(palette->selection_background_rgb);
    titlebar = rgb_css(palette->titlebar_rgb);
    success = rgb_css(palette->success_rgb);
    warning = rgb_css(palette->warning_rgb);
    fault = rgb_css(palette->fault_rgb);
    info = rgb_css(palette->info_rgb);
    surface_hover = rgb_css(palette->surface_hover_rgb);
    status_border = rgb_css(palette->status_border_rgb);

    css = g_string_new(NULL);
    g_string_append_printf(
        css,
        "window { background: %s; color: %s; font-family: '%s', %s; font-weight: %u; }\n"
        ".titlebar-shell { background: %s; border-bottom: 1px solid %s; padding: %upx %upx; }\n"
        ".app-title { color: %s; font-size: 18px; font-weight: %u; }\n"
        ".app-subtitle { color: %s; font-size: 12px; }\n"
        ".settings-sidebar { background: %s; border-right: 1px solid %s; padding: 14px 10px; }\n"
        ".nav-title { color: %s; font-size: 11px; font-weight: %u; letter-spacing: 0.08em; margin: 4px 8px 8px 8px; }\n"
        ".nav-row { border-radius: %upx; padding: 10px 12px; }\n"
        ".nav-row:selected { background: %s; }\n"
        ".settings-content { padding: %upx; }\n"
        ".page-title { color: %s; font-size: 26px; font-weight: %u; }\n"
        ".page-summary { color: %s; font-size: 13px; margin-bottom: %upx; }\n"
        ".preview-card, .settings-card { background: %s; border: 1px solid %s; border-radius: %upx; padding: %upx; }\n"
        ".preview-time { color: %s; font-size: 30px; font-weight: %u; }\n"
        ".preview-date { color: %s; font-size: 14px; }\n"
        ".section-title { color: %s; font-size: 16px; font-weight: %u; }\n"
        ".setting-label { color: %s; font-weight: %u; }\n"
        ".setting-description { color: %s; font-size: 12px; }\n"
        ".setting-row { padding: 8px 0; }\n"
        ".divider { background: %s; min-height: 1px; }\n"
        ".accent-note { color: %s; font-size: 12px; }\n"
        ".status-ok { color: %s; font-size: 12px; }\n"
        ".error { color: %s; font-size: 12px; }\n",
        background, text, type->ui_family, type->gtk_fallback,
        (unsigned int)type->ui_regular_weight,
        titlebar, border,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->content_padding,
        title, (unsigned int)type->ui_bold_weight,
        muted, panel, border,
        muted, (unsigned int)type->ui_bold_weight,
        (unsigned int)metrics->control_radius,
        selected, (unsigned int)metrics->screen_padding,
        title, (unsigned int)type->ui_bold_weight,
        muted, (unsigned int)metrics->compact_spacing,
        card, border, (unsigned int)metrics->card_radius,
        (unsigned int)metrics->section_spacing,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        title, (unsigned int)type->ui_bold_weight,
        text, (unsigned int)type->ui_bold_weight,
        muted, border, accent, success, fault);

    /*
     * Controls deliberately use the richer Common semantic palette rather than
     * flattening GTK's internal widgets to one surface colour. In particular,
     * the old generic "switch { background: ... }" rule obscured the slider
     * on GTK 4 themes and made disabled switches look like solid black blocks.
     */
    g_string_append_printf(
        css,
        ".preview-card { border-color: %s; }\n"
        ".preview-time { color: %s; }\n"
        ".clock-card { border-left: 3px solid %s; }\n"
        ".calendar-card { border-left: 3px solid %s; }\n"
        ".location-card { border-left: 3px solid %s; }\n"
        ".system-card { border-left: 3px solid %s; }\n"
        ".nav-row:selected { border-left: 3px solid %s; }\n"
        ".setting-dropdown, .setting-spin { background: %s; color: %s; border: 1px solid %s; border-radius: %upx; box-shadow: none; }\n"
        ".setting-dropdown:hover, .setting-spin:hover { background: %s; border-color: %s; }\n"
        ".setting-dropdown:focus, .setting-spin:focus { border-color: %s; }\n"
        ".setting-dropdown button { background: transparent; color: %s; border: none; box-shadow: none; }\n"
        ".setting-spin text { background: transparent; color: %s; }\n"
        ".setting-spin button { background: %s; color: %s; border-color: %s; box-shadow: none; }\n"
        ".setting-switch { min-width: 46px; min-height: 26px; background: %s; border: 1px solid %s; border-radius: 13px; box-shadow: none; }\n"
        ".setting-switch:hover { background: %s; border-color: %s; }\n"
        ".setting-switch:checked { background: %s; border-color: %s; }\n"
        ".setting-switch:checked:hover { background: %s; border-color: %s; }\n"
        ".setting-switch slider { min-width: 20px; min-height: 20px; margin: 2px; background: %s; border: none; border-radius: 10px; box-shadow: none; }\n"
        ".setting-switch:checked slider { background: %s; }\n"
        ".setting-switch:disabled { opacity: 0.52; }\n"
        ".setting-entry { background: %s; color: %s; border: 1px solid %s; border-radius: %upx; padding: 8px 10px; }\n"
        ".setting-entry:focus { border-color: %s; }\n"
        ".setting-button { background: %s; color: %s; border: 1px solid %s; border-radius: %upx; padding: 8px 12px; }\n"
        ".setting-button:hover { background: %s; border-color: %s; }\n"
        ".location-results { background: %s; border: 1px solid %s; border-radius: %upx; }\n"
        ".location-result-row { padding: 9px 10px; }\n"
        ".location-result-row:hover { background: %s; }\n",
        accent, accent,
        accent, info, warning, success, accent,
        input, text, status_border, (unsigned int)metrics->control_radius,
        surface_hover, subtle,
        accent,
        text,
        text,
        surface, text, status_border,
        surface, status_border,
        surface_hover, subtle,
        accent, accent,
        accent_hover, accent_hover,
        title, accent_foreground,
        input, text, status_border, (unsigned int)metrics->control_radius,
        accent,
        surface, text, status_border, (unsigned int)metrics->control_radius,
        surface_hover, subtle,
        card, border, (unsigned int)metrics->control_radius,
        surface_hover);

    provider = gtk_css_provider_new();
#if GTK_CHECK_VERSION(4, 12, 0)
    gtk_css_provider_load_from_string(provider, css->str);
#else
    gtk_css_provider_load_from_data(provider, css->str, -1);
#endif
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(provider);
    g_string_free(css, TRUE);
    g_free(background);
    g_free(panel);
    g_free(card);
    g_free(surface);
    g_free(input);
    g_free(border);
    g_free(text);
    g_free(title);
    g_free(muted);
    g_free(subtle);
    g_free(accent);
    g_free(accent_foreground);
    g_free(accent_hover);
    g_free(selected);
    g_free(titlebar);
    g_free(success);
    g_free(warning);
    g_free(fault);
    g_free(info);
    g_free(surface_hover);
    g_free(status_border);
}


static GtkWidget *build_sidebar(void)
{
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *list = gtk_list_box_new();
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name(
        "preferences-system-time-symbolic");

    gtk_widget_set_size_request(sidebar, 210, -1);
    gtk_widget_add_css_class(sidebar, "settings-sidebar");
    gtk_box_append(GTK_BOX(sidebar), ss_linux_ui_make_label("SYSTEM", "nav-title"));

    gtk_image_set_pixel_size(GTK_IMAGE(icon), 18);
    gtk_box_append(GTK_BOX(row_box), icon);
    gtk_box_append(GTK_BOX(row_box), ss_linux_ui_make_label("Date & Time", NULL));
    gtk_widget_add_css_class(row, "nav-row");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
    gtk_list_box_append(GTK_LIST_BOX(list), row);
    gtk_list_box_select_row(GTK_LIST_BOX(list), GTK_LIST_BOX_ROW(row));
    gtk_box_append(GTK_BOX(sidebar), list);
    return sidebar;
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

static GtkWidget *build_header(GtkWindow *parent)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *icon = gtk_image_new_from_icon_name("preferences-system-symbolic");
    GtkWidget *identity = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *about = gtk_button_new_from_icon_name("help-about-symbolic");

    gtk_widget_add_css_class(header, "titlebar-shell");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_box_append(GTK_BOX(header), icon);
    gtk_box_append(GTK_BOX(identity),
                   ss_linux_ui_make_label(info->program_name, "app-title"));
    gtk_box_append(GTK_BOX(identity),
                   ss_linux_ui_make_label("One place for system-wide preferences",
                              "app-subtitle"));
    gtk_box_append(GTK_BOX(header), identity);

    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(header), spacer);
    gtk_widget_set_tooltip_text(about, "About System Settings");
    g_signal_connect(about, "clicked", G_CALLBACK(show_about), parent);
    gtk_box_append(GTK_BOX(header), about);
    return header;
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
        ss_linux_ui_make_label("Date & Time", "page-title"));
    gtk_label_set_wrap(GTK_LABEL(copy), TRUE);
    gtk_box_append(GTK_BOX(page), copy);
    return page;
}

static void on_activate(GtkApplication *application, gpointer user_data)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    g_autoptr(GError) panel_error = NULL;
    GtkWindow *window;
    SsLinuxDateTimePanel *date_time;
    GtkWidget *root;
    GtkWidget *body;
    GtkWidget *scroller;
    GtkWidget *panel_widget;

    (void)user_data;

    install_common_theme();

    window = GTK_WINDOW(
        gtk_application_window_new(application));
    gtk_window_set_title(window, info->program_name);
    gtk_window_set_default_size(window, 1120, 820);
    gtk_window_set_resizable(window, TRUE);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(window, root);
    gtk_box_append(GTK_BOX(root), build_header(window));

    body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(body, TRUE);
    gtk_box_append(GTK_BOX(root), body);
    gtk_box_append(GTK_BOX(body), build_sidebar());

    date_time = ss_linux_date_time_panel_new(
        window, &panel_error);
    if (date_time != NULL) {
        /*
         * The host window owns the current built-in module instance. Async
         * module operations retain a window reference while in flight, so the
         * module data cannot disappear beneath their completion callbacks.
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

    scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroller),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(scroller),
        panel_widget);
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_box_append(GTK_BOX(body), scroller);

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
