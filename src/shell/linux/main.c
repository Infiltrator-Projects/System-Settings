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
    gchar *success;
    gchar *fault;
    gchar *surface_hover;
    gchar *status_border;
    gchar *warm;

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
    success = rgb_css(palette->success_rgb);
    fault = rgb_css(palette->fault_rgb);
    surface_hover = rgb_css(palette->surface_hover_rgb);
    status_border = rgb_css(palette->status_border_rgb);
    warm = rgb_css(palette->warning_rgb);

    /*
     * The shell deliberately uses one restrained surface hierarchy:
     * background -> navigation panel -> cards -> controls. Accent colour is
     * reserved for selection, focus and primary actions rather than decorating
     * every section independently.
     */
    css = g_string_new(NULL);
    g_string_append_printf(
        css,
        "window { background: %s; color: %s; font-family: '%s', %s; font-weight: %u; }\n"
        ".app-shell, scrolledwindow, viewport { background: %s; }\n"
        ".settings-sidebar { background: %s; border-right: 1px solid %s; padding: 20px 14px 16px 14px; }\n"
        ".settings-sidebar list, .settings-sidebar row, flowbox, flowboxchild { background: transparent; }\n"
        ".brand-block { margin: 0 6px 22px 6px; }\n"
        ".brand-icon { background: %s; border: 1px solid %s; border-radius: 10px; padding: 9px; }\n"
        ".brand-title { color: %s; font-size: 17px; font-weight: %u; }\n"
        ".brand-subtitle { color: %s; font-size: 11px; }\n"
        ".nav-title { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.10em; margin: 0 8px 7px 8px; }\n"
        ".nav-row { border: 1px solid transparent; border-radius: 6px; padding: 11px 12px; margin: 3px 8px; }\n"
        ".nav-row:hover { background: %s; }\n"
        ".nav-row:selected { background: %s; border-color: %s; border-left-width: 3px; border-left-color: %s; }\n"
        ".nav-primary { color: %s; font-weight: %u; }\n"
        ".nav-secondary { color: %s; font-size: 11px; }\n"
        ".sidebar-footer { border-top: 1px solid %s; padding-top: 12px; margin-top: 12px; }\n"
        ".sidebar-version { color: %s; font-size: 10px; }\n"
        ".sidebar-about { background: transparent; color: %s; border: 1px solid transparent; border-radius: 6px; padding: 7px 9px; }\n"
        ".sidebar-about:hover { background: %s; border-color: %s; }\n"
        ".settings-content { padding: 30px 34px 24px 34px; }\n"
        ".page-eyebrow { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.12em; }\n"
        ".page-title { color: %s; font-size: 28px; font-weight: %u; }\n"
        ".page-summary { color: %s; font-size: 12px; margin-bottom: 4px; }\n"
        ".hero-card { background: %s; border: 1px solid %s; border-radius: 18px; padding: 20px 22px; }\n"
        ".hero-kicker { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.11em; }\n"
        ".preview-time { color: %s; font-size: 42px; font-weight: %u; }\n"
        ".preview-date { color: %s; font-size: 14px; }\n"
        ".hero-note { color: %s; font-size: 11px; }\n"
        ".settings-card { background: %s; border: 1px solid %s; border-radius: 10px; padding: 18px; }\n"
        ".section-heading { margin-bottom: 4px; }\n"
        ".section-icon-wrap { background: %s; border-radius: 10px; padding: 7px; }\n"
        ".section-title { color: %s; font-size: 16px; font-weight: %u; }\n"
        ".section-summary { color: %s; font-size: 11px; }\n"
        ".setting-row { padding: 10px 0; }\n"
        ".setting-label { color: %s; font-weight: %u; }\n"
        ".setting-description { color: %s; font-size: 11px; }\n"
        ".field-caption { color: %s; font-size: 10px; font-weight: %u; }\n"
        ".accent-note { color: %s; font-size: 11px; }\n"
        ".status-ok { color: %s; font-size: 11px; }\n"
        ".error { color: %s; font-size: 11px; }\n",
        background, text, type->ui_family, type->gtk_fallback,
        (unsigned int)type->ui_regular_weight,
        background,
        panel, border,
        card, border,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        muted, (unsigned int)type->ui_bold_weight,
        surface_hover,
        selected, accent, accent,
        text, (unsigned int)type->ui_bold_weight,
        muted,
        border,
        muted,
        text,
        surface_hover, border,
        accent, (unsigned int)type->ui_bold_weight,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        card, border,
        accent, (unsigned int)type->ui_bold_weight,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        muted,
        card, border,
        surface,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        text, (unsigned int)type->ui_bold_weight,
        muted,
        subtle, (unsigned int)type->ui_bold_weight,
        accent,
        success,
        fault);

    g_string_append_printf(
        css,
        ".setting-dropdown, .setting-spin { background: %s; color: %s; border: 1px solid %s; border-radius: %upx; box-shadow: none; min-height: 34px; }\n"
        ".setting-dropdown:hover, .setting-spin:hover { background: %s; border-color: %s; }\n"
        ".setting-dropdown:focus, .setting-spin:focus { border-color: %s; }\n"
        ".setting-dropdown button { background: transparent; color: %s; border: none; box-shadow: none; }\n"
        ".setting-spin text { background: transparent; color: %s; }\n"
        ".setting-spin button { background: %s; color: %s; border-color: %s; box-shadow: none; }\n"
        ".setting-switch { min-width: 44px; min-height: 24px; background: %s; border: 1px solid %s; border-radius: 12px; box-shadow: none; }\n"
        ".setting-switch:hover { background: %s; border-color: %s; }\n"
        ".setting-switch:checked { background: %s; border-color: %s; }\n"
        ".setting-switch:checked:hover { background: %s; border-color: %s; }\n"
        ".setting-switch slider { min-width: 18px; min-height: 18px; margin: 2px; background: %s; border: none; border-radius: 9px; box-shadow: none; }\n"
        ".setting-switch:checked slider { background: %s; }\n"
        ".setting-switch:disabled { opacity: 0.50; }\n"
        ".setting-entry { background: %s; color: %s; border: 1px solid %s; border-radius: %upx; padding: 8px 10px; min-height: 34px; }\n"
        ".setting-entry:focus { border-color: %s; }\n"
        ".setting-button { background: %s; color: %s; border: 1px solid %s; border-radius: %upx; padding: 8px 12px; min-height: 34px; }\n"
        ".setting-button:hover { background: %s; border-color: %s; }\n"
        ".primary-button { background: %s; color: %s; border-color: %s; font-weight: %u; }\n"
        ".primary-button:hover { background: %s; border-color: %s; }\n"
        ".location-results { background: %s; border: 1px solid %s; border-radius: 10px; }\n"
        ".location-result-row { padding: 9px 10px; }\n"
        ".location-result-row:hover { background: %s; }\n",
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
        accent, accent_foreground, accent,
        (unsigned int)type->ui_bold_weight,
        accent_hover, accent_hover,
        card, border,
        surface_hover);

    /*
     * First north-star polish pass. These rules deliberately remain styling
     * only: no placeholder settings or dead controls are introduced merely to
     * imitate the concept image. The existing working Date & Time surface gets
     * the stronger hierarchy, colour, geometry and visual confidence first.
     */
    g_string_append_printf(
        css,
        "headerbar { background: %s; border-bottom: 1px solid %s; min-height: 46px; }\n"
        ".app-shell { background-image: linear-gradient(to bottom right, %s, %s); }\n"
        ".settings-sidebar { background-image: linear-gradient(to bottom, %s, %s); padding: 18px 14px 14px 14px; }\n"
        ".brand-block { background: %s; border: 1px solid %s; border-radius: 16px; padding: 13px; margin: 0 4px 18px 4px; }\n"
        ".brand-icon { color: %s; border-radius: 13px; padding: 10px; }\n"
        ".brand-title { font-size: 21px; }\n"
        ".brand-subtitle { font-size: 12px; }\n"
        ".nav-title { color: %s; margin: 0 10px 8px 10px; }\n"
        ".nav-row { border-radius: 12px; padding: 12px 13px; margin: 4px 5px; }\n"
        ".nav-row image { color: %s; }\n"
        ".nav-row:selected { background-image: linear-gradient(to right, %s, %s); border-width: 1px; border-color: %s; }\n"
        ".nav-row:selected image { color: %s; }\n"
        ".nav-primary { font-size: 14px; }\n"
        ".settings-content { padding: 26px 30px 30px 30px; }\n"
        ".page-eyebrow { color: %s; }\n"
        ".page-title { font-size: 36px; }\n"
        ".page-summary { font-size: 13px; margin-bottom: 8px; }\n"
        ".hero-card { background-image: linear-gradient(to bottom right, %s, %s); border-color: %s; border-radius: 20px; padding: 24px 26px; box-shadow: 0 6px 18px rgba(0,0,0,0.22); }\n"
        ".hero-kicker { color: %s; }\n"
        ".preview-time { font-size: 46px; }\n"
        ".hero-note { margin-top: 6px; }\n"
        ".settings-card { border-radius: 16px; padding: 20px; box-shadow: 0 4px 14px rgba(0,0,0,0.16); }\n"
        ".section-heading { margin-bottom: 8px; }\n"
        ".section-icon-wrap { border: 1px solid %s; background: %s; border-radius: 12px; padding: 9px; }\n"
        ".section-icon-wrap image { color: %s; }\n"
        ".section-title { font-size: 17px; }\n"
        ".setting-row { border-radius: 10px; padding: 11px 12px; }\n"
        ".setting-row:hover { background: %s; }\n"
        ".setting-button, .setting-entry, .setting-dropdown, .setting-spin { min-height: 38px; }\n"
        ".primary-button { box-shadow: 0 2px 10px rgba(0,0,0,0.18); }\n"
        ".page-header { padding: 2px 2px 4px 2px; }\n"
        ".page-icon { min-width: 54px; min-height: 54px; background: %s; border: 1px solid %s; border-radius: 15px; padding: 10px; }\n"
        ".page-icon image { color: %s; }\n"
        ".hero-top { min-height: 94px; }\n"
        ".hero-badge { background: %s; border: 1px solid %s; border-radius: 999px; padding: 6px 10px; }\n"
        ".hero-badge image, .hero-badge-label { color: %s; }\n"
        ".hero-badge-label { font-size: 10px; font-weight: %u; letter-spacing: 0.10em; }\n"
        ".info-strip { background: %s; border: 1px solid %s; border-radius: 10px; padding: 9px 11px; }\n"
        ".info-strip image { color: %s; }\n"
        ".status-ok { background: %s; border: 1px solid %s; border-radius: 9px; padding: 8px 10px; }\n"
        ".error { background: %s; border: 1px solid %s; border-radius: 9px; padding: 8px 10px; }\n"
        ".settings-card > .section-heading { padding-bottom: 6px; border-bottom: 1px solid %s; }\n",
        card, border, warm,
        selected, accent, warm, (unsigned int)type->ui_bold_weight,
        surface, border, accent,
        surface, status_border,
        surface, fault,
        border);
        panel, border,
        background, panel,
        panel, background,
        card, border,
        accent,
        warm,
        muted,
        selected, card, accent,
        warm,
        warm,
        card, surface, accent,
        warm,
        warm, surface,
        warm,
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
    g_free(success);
    g_free(fault);
    g_free(surface_hover);
    g_free(status_border);
    g_free(warm);
}

static void show_about(GtkButton *button, gpointer user_data);

static GtkWidget *build_sidebar(GtkWindow *parent)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *brand = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *brand_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *brand_icon = gtk_image_new_from_icon_name(info->icon_name);
    GtkWidget *brand_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *list = gtk_list_box_new();
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name(
        "preferences-system-time-symbolic");
    GtkWidget *row_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *about = gtk_button_new_with_label("About");
    g_autofree gchar *version =
        g_strdup_printf("Version %s", info->version);

    gtk_widget_set_size_request(sidebar, 276, -1);
    gtk_widget_add_css_class(sidebar, "settings-sidebar");

    gtk_widget_add_css_class(brand, "brand-block");
    gtk_widget_add_css_class(brand_icon_wrap, "brand-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(brand_icon), 30);
    gtk_box_append(GTK_BOX(brand_icon_wrap), brand_icon);
    gtk_box_append(GTK_BOX(brand), brand_icon_wrap);
    gtk_box_append(
        GTK_BOX(brand_copy),
        ss_linux_ui_make_label("System Settings", "brand-title"));
    gtk_box_append(
        GTK_BOX(brand_copy),
        ss_linux_ui_make_label("Infiltrator OS", "brand-subtitle"));
    gtk_box_append(GTK_BOX(brand), brand_copy);
    gtk_box_append(GTK_BOX(sidebar), brand);

    gtk_box_append(
        GTK_BOX(sidebar),
        ss_linux_ui_make_label("SYSTEM", "nav-title"));

    gtk_image_set_pixel_size(GTK_IMAGE(icon), 22);
    gtk_box_append(GTK_BOX(row_box), icon);
    gtk_box_append(
        GTK_BOX(row_copy),
        ss_linux_ui_make_label("Date & Time", "nav-primary"));
    gtk_box_append(
        GTK_BOX(row_copy),
        ss_linux_ui_make_label("Clock, calendar & location", "nav-secondary"));
    gtk_box_append(GTK_BOX(row_box), row_copy);
    gtk_widget_add_css_class(row, "nav-row");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
    gtk_list_box_append(GTK_LIST_BOX(list), row);
    gtk_list_box_select_row(GTK_LIST_BOX(list), GTK_LIST_BOX_ROW(row));
    gtk_box_append(GTK_BOX(sidebar), list);

    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(sidebar), spacer);

    gtk_widget_add_css_class(footer, "sidebar-footer");
    gtk_widget_set_hexpand(footer, TRUE);
    gtk_box_append(
        GTK_BOX(footer),
        ss_linux_ui_make_label(version, "sidebar-version"));
    gtk_widget_set_hexpand(
        gtk_widget_get_first_child(footer), TRUE);
    gtk_widget_add_css_class(about, "sidebar-about");
    g_signal_connect(about, "clicked", G_CALLBACK(show_about), parent);
    gtk_box_append(GTK_BOX(footer), about);
    gtk_box_append(GTK_BOX(sidebar), footer);
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
    GtkWidget *scroller;
    GtkWidget *panel_widget;

    (void)user_data;

    window = gtk_application_get_active_window(application);
    if (window != NULL) {
        gtk_window_present(window);
        return;
    }
    install_common_theme();

    window = GTK_WINDOW(
        gtk_application_window_new(application));
    gtk_window_set_title(window, info->program_name);
    gtk_window_set_default_size(window, 1260, 860);
    gtk_window_set_resizable(window, TRUE);
    g_signal_connect(window, "close-request", G_CALLBACK(on_close_requested), NULL);

    root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(root, "app-shell");
    gtk_window_set_child(window, root);
    gtk_box_append(GTK_BOX(root), build_sidebar(window));

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
    gtk_box_append(GTK_BOX(root), scroller);

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
