// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-theme.c
 * @brief Common-backed GTK theme installation and live desktop-theme following.
 */
#include "linux-theme.h"

#include <gtk/gtk.h>
#include <infiltratr/core.h>
#include <infiltratr/design.h>

#include <stdbool.h>
#include <stdint.h>

static GtkCssProvider *common_theme_provider;
static bool common_theme_watch_installed;
static unsigned int common_theme_generation;

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

void ss_linux_theme_install(void)
{
    const InfiltratrThemePalette *palette =
        infiltratr_theme_resolve(INFILTRATR_THEME_SYSTEM,
                                 system_prefers_dark());
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    const InfiltratrTypography *type = infiltratr_typography();
    GtkCssProvider *provider;
    GdkDisplay *display;
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

    display = gdk_display_get_default();
    if (palette == NULL || metrics == NULL || type == NULL ||
        display == NULL) {
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

    /*
     * One selector owner per component. Keep responsive/layout corrections in
     * the same rule as their visual treatment so later patches cannot leave
     * stale cascade layers underneath the current design.
     */
    g_string_append_printf(
        css,
        "window { background: %s; color: %s; font-family: '%s', %s; font-weight: %u; }\n"
        ".app-shell, scrolledwindow, viewport { background: %s; }\n"
        "headerbar, .shell-header { background: %s; border-bottom: 1px solid %s; min-height: 58px; }\n"
        ".header-brand { padding: 2px 4px; }\n"
        ".header-brand-icon { background: %s; border: 1px solid %s; border-radius: 12px; padding: 7px; box-shadow: none; }\n"
        ".header-brand-icon image { color: %s; }\n"
        ".header-brand-title { color: %s; font-size: 20px; font-weight: %u; }\n"
        ".header-brand-subtitle { color: %s; font-size: 11px; }\n"
        ".header-end { margin-left: 10px; }\n"
        ".settings-search { min-width: 180px; background: %s; color: %s; border: 1px solid %s; border-radius: 14px; padding: 8px 12px; }\n"
        ".settings-search:focus { border-color: %s; }\n"
        ".window-control { min-width: 30px; min-height: 30px; padding: 4px; background: transparent; border: 1px solid transparent; border-radius: 8px; }\n"
        ".window-control:hover { background: %s; border-color: %s; }\n"
        ".window-control-close:hover { background: %s; color: %s; }\n"
        ".settings-sidebar { background: %s; border-right: 1px solid %s; padding: 12px 9px; min-width: 195px; }\n"
        ".settings-sidebar list, .settings-sidebar row, flowbox, flowboxchild { background: transparent; }\n"
        ".settings-sidebar list { padding: 2px 0; }\n"
        ".nav-title { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.10em; margin: 0 10px 8px 10px; }\n"
        ".nav-row { min-height: 50px; border: 1px solid transparent; border-radius: 12px; padding: 7px 9px; margin: 2px 4px; }\n"
        ".nav-row:hover { background: %s; }\n"
        ".nav-row:selected { background: %s; border-color: %s; box-shadow: none; }\n"
        ".nav-row:selected .nav-primary, .nav-row:selected .nav-secondary { color: %s; }\n"
        ".nav-row:disabled { opacity: 0.42; }\n"
        ".nav-icon-well { min-width: 38px; min-height: 38px; border-radius: 11px; padding: 5px; background: %s; border: 1px solid %s; }\n"
        ".nav-gold .nav-icon-well image { color: %s; }\n"
        ".nav-cyan .nav-icon-well image { color: %s; }\n"
        ".nav-primary { color: %s; font-size: 14px; font-weight: %u; }\n"
        ".nav-secondary { color: %s; font-size: 11px; }\n"
        ".sidebar-footer { border-top: 1px solid %s; padding-top: 12px; margin: 10px 8px 0 8px; }\n"
        ".sidebar-version { color: %s; font-size: 10px; padding: 5px 2px; }\n"
        "scrollbar { background: transparent; min-width: 10px; min-height: 10px; }\n"
        "scrollbar slider { min-width: 8px; min-height: 28px; border-radius: 999px; background: %s; }\n"
        "scrollbar slider:hover { background: %s; }\n",
        background, text, type->ui_family, type->gtk_fallback,
        (unsigned int)type->ui_regular_weight,
        background,
        panel, border,
        card, border, accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        input, text, status_border, accent,
        surface_hover, border,
        fault, accent_foreground,
        panel, border,
        muted, (unsigned int)type->ui_bold_weight,
        surface_hover,
        selected, accent, accent_foreground,
        surface, border,
        warm, accent,
        text, (unsigned int)type->ui_bold_weight,
        muted,
        border,
        muted,
        subtle, accent);

    g_string_append_printf(
        css,
        ".settings-content { padding: 18px 20px 22px 20px; }\n"
        ".page-header { padding: 2px 2px 4px 2px; }\n"
        ".page-icon { min-width: 54px; min-height: 54px; background: %s; border: 1px solid %s; border-radius: 15px; padding: 10px; }\n"
        ".page-icon image { color: %s; }\n"
        ".page-eyebrow { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.12em; }\n"
        ".page-title { color: %s; font-size: 36px; font-weight: %u; }\n"
        ".page-summary { color: %s; font-size: 13px; margin-bottom: 8px; }\n"
        ".hero-card { background: %s; border: 1px solid %s; border-radius: 20px; padding: 26px 28px; box-shadow: 0 6px 18px rgba(0,0,0,0.22); }\n"
        ".hero-top { min-height: 94px; }\n"
        ".hero-live-column { min-width: 0; padding: 4px 10px 4px 0; }\n"
        ".hero-kicker { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.11em; }\n"
        ".preview-time { color: %s; font-size: 46px; font-weight: %u; }\n"
        ".preview-date { color: %s; font-size: 14px; }\n"
        ".hero-note { color: %s; font-size: 11px; margin-top: 6px; }\n"
        ".hero-badge { background: %s; border: 1px solid %s; border-radius: 999px; padding: 6px 10px; }\n"
        ".hero-badge image, .hero-badge-label { color: %s; }\n"
        ".hero-badge-label { font-size: 10px; font-weight: %u; letter-spacing: 0.10em; }\n"
        ".overview-panel { min-width: 260px; background: %s; border: 1px solid %s; border-radius: 16px; padding: 14px; }\n"
        ".overview-heading { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.11em; margin-bottom: 3px; }\n"
        ".overview-item { background: %s; border: 1px solid %s; border-radius: 12px; padding: 10px 11px; min-height: 56px; }\n"
        ".overview-item:hover { background: %s; }\n"
        ".overview-icon { background: %s; border: 1px solid %s; border-radius: 10px; padding: 7px; }\n"
        ".overview-icon image { color: %s; }\n"
        ".overview-label, .overview-key { color: %s; font-size: 10px; }\n"
        ".overview-value, .overview-data { color: %s; font-size: 13px; font-weight: %u; }\n"
        ".overview-link-button { background: transparent; color: %s; border: 0; }\n"
        ".settings-card { background: %s; border: 1px solid %s; border-top-width: 2px; border-radius: 16px; padding: 20px; box-shadow: none; }\n"
        ".location-card { border-top-color: %s; }\n"
        ".presentation-card { border-top-color: %s; }\n"
        ".system-card { border-top-color: %s; }\n"
        ".section-heading { margin-bottom: 8px; padding-bottom: 6px; border-bottom: 1px solid %s; }\n"
        ".section-icon-wrap { background: %s; border: 1px solid %s; border-radius: 12px; padding: 9px; }\n"
        ".section-icon-wrap image { color: %s; }\n"
        ".section-title { color: %s; font-size: 17px; font-weight: %u; }\n"
        ".section-summary { color: %s; font-size: 11px; }\n"
        ".setting-row { border-radius: 10px; padding: 11px 12px; }\n"
        ".setting-row:hover { background: %s; }\n"
        ".setting-tile { background: %s; border: 1px solid %s; border-radius: 13px; padding: 12px 13px; margin: 3px 0; }\n"
        ".setting-tile:hover { background: %s; border-color: %s; }\n"
        ".setting-tile-icon { min-width: 38px; min-height: 38px; background: %s; border: 1px solid %s; border-radius: 11px; padding: 6px; }\n"
        ".setting-tile-icon image { color: %s; }\n"
        ".setting-label { color: %s; font-weight: %u; }\n"
        ".setting-description { color: %s; font-size: 11px; }\n"
        ".field-caption { color: %s; font-size: 10px; font-weight: %u; }\n"
        ".accent-note { color: %s; font-size: 11px; }\n"
        ".info-strip { background: %s; border: 1px solid %s; border-radius: 10px; padding: 9px 11px; }\n"
        ".info-strip image { color: %s; }\n"
        ".status-ok, .error, .page-status { border-radius: 9px; padding: 8px 10px; font-size: 11px; }\n"
        ".status-ok { color: %s; background: %s; border: 1px solid %s; }\n"
        ".error { color: %s; background: %s; border: 1px solid %s; }\n",
        surface, border, accent,
        accent, (unsigned int)type->ui_bold_weight,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        card, border,
        accent, (unsigned int)type->ui_bold_weight,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        muted,
        selected, accent, accent_foreground,
        (unsigned int)type->ui_bold_weight,
        surface, border,
        muted, (unsigned int)type->ui_bold_weight,
        card, border,
        surface_hover,
        surface, border, accent,
        muted,
        title, (unsigned int)type->ui_bold_weight,
        accent,
        card, border,
        warm, accent, success,
        border,
        surface, border, accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        surface_hover,
        surface, border,
        surface_hover, subtle,
        card, border, accent,
        text, (unsigned int)type->ui_bold_weight,
        muted,
        subtle, (unsigned int)type->ui_bold_weight,
        accent,
        surface, border, accent,
        success, surface, status_border,
        fault, surface, fault);

    g_string_append_printf(
        css,
        ".setting-dropdown, .setting-spin, .setting-entry, .setting-button { min-height: 38px; border-radius: %upx; }\n"
        ".setting-dropdown, .setting-spin, .setting-entry { background: %s; color: %s; border: 1px solid %s; }\n"
        ".setting-dropdown:hover, .setting-spin:hover, .setting-button:hover { background: %s; border-color: %s; }\n"
        ".setting-dropdown:focus, .setting-spin:focus, .setting-entry:focus { border-color: %s; }\n"
        ".setting-dropdown button { background: transparent; color: %s; border: none; box-shadow: none; }\n"
        ".setting-spin text { background: transparent; color: %s; }\n"
        ".setting-spin button { background: %s; color: %s; border-color: %s; box-shadow: none; }\n"
        ".setting-entry { padding: 8px 10px; }\n"
        ".setting-button { background: %s; color: %s; border: 1px solid %s; padding: 8px 12px; }\n"
        ".primary-button { background: %s; color: %s; border-color: %s; font-weight: %u; box-shadow: none; }\n"
        ".primary-button:hover { background: %s; border-color: %s; }\n"
        ".setting-switch { min-width: 44px; min-height: 24px; background: %s; border: 1px solid %s; border-radius: 12px; box-shadow: none; }\n"
        ".setting-switch:hover { background: %s; border-color: %s; }\n"
        ".setting-switch:checked { background: %s; border-color: %s; }\n"
        ".setting-switch:checked:hover { background: %s; border-color: %s; }\n"
        ".setting-switch slider { min-width: 18px; min-height: 18px; margin: 2px; background: %s; border: none; border-radius: 9px; box-shadow: none; }\n"
        ".setting-switch:checked slider { background: %s; }\n"
        ".setting-switch:disabled { opacity: 0.50; }\n"
        ".location-results { background: %s; border: 1px solid %s; border-radius: 10px; }\n"
        ".location-results row { border-radius: 9px; margin: 2px 4px; }\n"
        ".location-result-row { padding: 9px 10px; }\n"
        ".location-result-row:hover { background: %s; }\n",
        (unsigned int)metrics->control_radius,
        input, text, status_border,
        surface_hover, subtle,
        accent,
        text,
        text,
        surface, text, status_border,
        surface, text, status_border,
        accent, accent_foreground, accent,
        (unsigned int)type->ui_bold_weight,
        accent_hover, accent_hover,
        surface, status_border,
        surface_hover, subtle,
        accent, accent,
        accent_hover, accent_hover,
        title, accent_foreground,
        card, border,
        surface_hover);

    g_string_append_printf(
        css,
        ".home-page { padding: 22px 26px 28px 26px; }\n"
        ".home-hero { min-height: 210px; padding: 0; border: 0; border-radius: 20px; box-shadow: none; }\n"
        ".hero-scene { min-width: 190px; min-height: 210px; }\n"
        ".hero-copy-overlay { min-width: 0; padding: 14px 18px; margin: 12px; border-radius: 15px; background: rgba(0,0,0,0.56); }\n"
        ".hero-brand-overlay { margin: 14px; padding: 12px 14px; background: rgba(0,0,0,0.46); border: 1px solid %s; border-radius: 13px; box-shadow: none; }\n"
        ".hero-brand-overlay image { color: %s; }\n"
        ".home-hero-eyebrow { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.12em; }\n"
        ".home-hero-title { color: %s; font-size: 34px; font-weight: %u; }\n"
        ".home-hero-accent { color: %s; font-size: 34px; font-weight: %u; }\n"
        ".home-hero-subtitle { color: %s; font-size: 15px; }\n"
        ".home-hero-mark { min-width: 190px; min-height: 182px; padding: 0; border: 1px solid %s; border-radius: 18px; background: %s; }\n"
        ".home-hero-mark image { color: %s; }\n"
        ".home-hero-mark-title { color: %s; font-size: 17px; font-weight: %u; letter-spacing: 0.08em; }\n"
        ".home-hero-mark-copy { color: %s; font-size: 10px; letter-spacing: 0.08em; }\n"
        ".home-feature-row, .home-grid, .status-grid { margin-top: 14px; }\n"
        ".home-feature { min-height: 44px; background: %s; border: 1px solid %s; border-radius: 12px; padding: 7px 12px; }\n"
        ".home-feature image { color: %s; }\n"
        ".home-feature-title { color: %s; font-weight: %u; }\n"
        ".home-feature-copy { color: %s; font-size: 10px; }\n"
        ".home-card, .status-card { background: %s; border: 1px solid %s; border-radius: 17px; padding: 16px 17px; box-shadow: none; }\n"
        ".home-card:hover, .status-card:hover { border-color: %s; }\n"
        ".home-card-title { color: %s; font-size: 18px; font-weight: %u; }\n"
        ".home-card-icon, .status-card-icon, .location-pin-well { background: %s; border: 1px solid %s; border-radius: 12px; padding: 8px; }\n"
        ".home-card-icon image, .status-card-icon image, .location-pin-well image { color: %s; }\n"
        ".quick-action-grid { margin-top: 2px; }\n"
        ".quick-action { min-height: 68px; padding: 8px 10px; background: %s; border: 1px solid %s; border-radius: 13px; }\n"
        ".quick-action:hover { background: %s; border-color: %s; box-shadow: none; }\n"
        ".quick-action-icon { min-width: 42px; min-height: 42px; border-radius: 12px; padding: 6px; background: %s; border: 1px solid %s; box-shadow: none; }\n"
        ".quick-action-icon image { color: %s; }\n"
        ".quick-action-title { color: %s; font-weight: %u; }\n"
        ".quick-action-copy { color: %s; font-size: 10px; }\n"
        ".quick-action-arrow, .card-arrow { background: transparent; border: 0; color: %s; }\n"
        ".status-card-heading { margin-bottom: 8px; }\n"
        ".status-card-value, .region-country, .home-clock-value, .location-primary { color: %s; font-size: 23px; font-weight: %u; }\n"
        ".status-card-detail, .home-clock-date { color: %s; font-size: 11px; }\n"
        ".date-status-card { border-top: 2px solid %s; }\n"
        ".region-status-card { border-top: 2px solid %s; }\n"
        ".appearance-status-card { border-top: 2px solid %s; }\n"
        ".network-status-card { border-top: 2px solid %s; }\n"
        ".overview-scene { min-width: 190px; min-height: 126px; border-radius: 13px; }\n"
        ".network-visual { min-height: 84px; border-radius: 12px; }\n"
        ".theme-preview { padding: 5px; border: 1px solid transparent; border-radius: 10px; }\n"
        ".theme-preview-selected { border-color: %s; background: %s; }\n"
        ".theme-preview-window { border: 1px solid %s; border-radius: 8px; }\n"
        ".theme-preview-label { color: %s; font-size: 10px; }\n"
        ".asset-missing { background: transparent; border: 1px solid transparent; box-shadow: none; }\n",
        border, accent,
        warm, (unsigned int)type->ui_bold_weight,
        title, (unsigned int)type->ui_bold_weight,
        warm, (unsigned int)type->ui_bold_weight,
        muted,
        border, surface, accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        surface, border, accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        card, border,
        accent,
        title, (unsigned int)type->ui_bold_weight,
        surface, border, accent,
        surface, border,
        surface_hover, accent,
        surface, border, accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        warm, accent, warm, success,
        accent, selected,
        border,
        muted);

    provider = gtk_css_provider_new();
#if GTK_CHECK_VERSION(4, 12, 0)
    gtk_css_provider_load_from_string(provider, css->str);
#else
    gtk_css_provider_load_from_data(provider, css->str, -1);
#endif
    if (common_theme_provider != NULL) {
        gtk_style_context_remove_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(common_theme_provider));
        g_clear_object(&common_theme_provider);
    }
    common_theme_provider = g_object_ref(provider);
    gtk_style_context_add_provider_for_display(
        display,
        GTK_STYLE_PROVIDER(common_theme_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    ++common_theme_generation;

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

static void on_system_theme_changed(
    GObject *object G_GNUC_UNUSED,
    GParamSpec *pspec G_GNUC_UNUSED,
    gpointer user_data G_GNUC_UNUSED)
{
    ss_linux_theme_install();
}

unsigned int ss_linux_theme_generation(void)
{
    return common_theme_generation;
}

void ss_linux_theme_watch(void)
{
    GtkSettings *settings;

    if (common_theme_watch_installed) {
        return;
    }
    settings = gtk_settings_get_default();
    if (settings == NULL) {
        return;
    }
    g_signal_connect(
        settings,
        "notify::gtk-theme-name",
        G_CALLBACK(on_system_theme_changed),
        NULL);
    g_signal_connect(
        settings,
        "notify::gtk-application-prefer-dark-theme",
        G_CALLBACK(on_system_theme_changed),
        NULL);
    common_theme_watch_installed = true;
}

