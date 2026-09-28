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
        ".settings-content { padding: 22px 24px 22px 24px; }\n"
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
     * North-star polish layers presentation over Common's semantic palette.
     * These rules remain styling only: no placeholder settings or dead
     * controls are introduced merely to imitate the concept image.
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
        ".settings-card > .section-heading { padding-bottom: 6px; border-bottom: 1px solid %s; }\n"
        ".hero-card { padding: 26px 28px; }\n"
        ".hero-live-column { min-width: 340px; padding: 4px 10px 4px 0; }\n"
        ".overview-panel { background: %s; border: 1px solid %s; border-radius: 16px; padding: 14px; }\n"
        ".overview-heading { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.11em; margin-bottom: 3px; }\n"
        ".overview-item { background: %s; border: 1px solid %s; border-radius: 12px; padding: 10px 11px; min-height: 56px; }\n"
        ".overview-item:hover { background: %s; }\n"
        ".overview-icon { background: %s; border: 1px solid %s; border-radius: 10px; padding: 7px; }\n"
        ".overview-icon image { color: %s; }\n"
        ".overview-label { color: %s; font-size: 9px; letter-spacing: 0.06em; }\n"
        ".overview-value { color: %s; font-size: 13px; font-weight: %u; }\n"
        ".location-results row { border-radius: 9px; margin: 2px 4px; }\n"
        ".settings-card { border-top-width: 2px; }\n",
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
        surface_hover,
        card, border, warm,
        selected, accent, warm, (unsigned int)type->ui_bold_weight,
        surface, border, accent,
        surface, status_border,
        surface, fault,
        border,
        surface, border,
        warm, (unsigned int)type->ui_bold_weight,
        card, border,
        surface_hover,
        surface, border,
        accent,
        muted,
        title, (unsigned int)type->ui_bold_weight);

    g_string_append_printf(
        css,
        ".setting-tile { background: %s; border: 1px solid %s; border-radius: 13px; padding: 12px 13px; margin: 3px 0; }\n"
        ".setting-tile:hover { background: %s; border-color: %s; }\n"
        ".setting-tile-icon { min-width: 38px; min-height: 38px; background: %s; border: 1px solid %s; border-radius: 11px; padding: 6px; }\n"
        ".setting-tile-icon image { color: %s; }\n"
        ".location-card { border-top-color: %s; }\n"
        ".presentation-card { border-top-color: %s; }\n"
        ".system-card { border-top-color: %s; }\n"
        ".location-card .section-icon-wrap image { color: %s; }\n"
        ".presentation-card .section-icon-wrap image { color: %s; }\n"
        ".system-card .section-icon-wrap image { color: %s; }\n",
        surface, border,
        surface_hover, subtle,
        card, border,
        accent,
        warm,
        accent,
        success,
        warm,
        accent,
        success);

    g_string_append_printf(
        css,
        ".shell-header { background-image: linear-gradient(to right, %s, %s); border-bottom: 1px solid %s; padding: 6px 10px; }\n"
        ".header-brand { padding: 2px 4px; }\n"
        ".header-brand-icon { background: %s; border: 1px solid %s; border-radius: 12px; padding: 7px; }\n"
        ".header-brand-icon image { color: %s; }\n"
        ".header-brand-title { color: %s; font-size: 20px; font-weight: %u; }\n"
        ".header-brand-subtitle { color: %s; font-size: 11px; }\n"
        ".settings-search { min-width: 210px; background: %s; color: %s; border: 1px solid %s; border-radius: 14px; padding: 8px 12px; }\n"
        ".settings-search:focus { border-color: %s; }\n"
        ".header-end { margin-left: 10px; }\n"
        ".home-page { padding: 22px 26px 28px 26px; }\n"
        ".home-hero { background-image: linear-gradient(115deg, %s, %s); border: 1px solid %s; border-radius: 20px; padding: 26px 28px; }\n"
        ".home-hero-eyebrow { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.12em; }\n"
        ".home-hero-title { color: %s; font-size: 34px; font-weight: %u; }\n"
        ".home-hero-accent { color: %s; font-size: 34px; font-weight: %u; }\n"
        ".home-hero-subtitle { color: %s; font-size: 15px; }\n"
        ".home-hero-mark { min-width: 150px; min-height: 150px; background: %s; border: 1px solid %s; border-radius: 75px; padding: 24px; }\n"
        ".home-hero-mark image { color: %s; }\n",
        panel, background, border,
        card, border, accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        input, text, status_border,
        accent,
        card, panel, border,
        warm, (unsigned int)type->ui_bold_weight,
        title, (unsigned int)type->ui_bold_weight,
        warm, (unsigned int)type->ui_bold_weight,
        muted,
        surface, border,
        accent);

    g_string_append_printf(
        css,
        ".home-feature-row { margin-top: 10px; }\n"
        ".home-feature { min-height: 44px; background: %s; border: 1px solid %s; border-radius: 12px; padding: 7px 12px; }\n"
        ".home-feature image { color: %s; }\n"
        ".home-feature-title { color: %s; font-weight: %u; }\n"
        ".home-feature-copy { color: %s; font-size: 10px; }\n"
        ".home-grid { margin-top: 14px; }\n"
        ".home-card { background: %s; border: 1px solid %s; border-radius: 17px; padding: 18px; }\n"
        ".home-card-title { color: %s; font-size: 18px; font-weight: %u; }\n"
        ".home-card-icon { background: %s; border: 1px solid %s; border-radius: 12px; padding: 8px; }\n"
        ".home-card-icon image { color: %s; }\n"
        ".overview-key { color: %s; font-size: 11px; }\n"
        ".overview-data { color: %s; font-size: 12px; font-weight: %u; }\n"
        ".quick-action { background: %s; border: 1px solid %s; border-radius: 13px; padding: 12px; }\n"
        ".quick-action:hover { background: %s; border-color: %s; }\n"
        ".quick-action image { color: %s; }\n"
        ".quick-action-title { color: %s; font-weight: %u; }\n"
        ".quick-action-copy { color: %s; font-size: 10px; }\n"
        ".home-date-card { margin-top: 14px; border-top: 2px solid %s; }\n"
        ".home-date-copy { color: %s; font-size: 13px; }\n",
        surface, border,
        accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        card, border,
        title, (unsigned int)type->ui_bold_weight,
        surface, border,
        warm,
        muted,
        title, (unsigned int)type->ui_bold_weight,
        surface, border,
        surface_hover, accent,
        accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        warm,
        muted);

    g_string_append_printf(
        css,
        ".home-hero-mark { min-width: 270px; min-height: 156px; border-radius: 20px; background-image: linear-gradient(135deg, %s, %s); }\n"
        ".home-hero-mark image { color: %s; }\n"
        ".home-hero-mark-title { color: %s; font-size: 17px; font-weight: %u; letter-spacing: 0.08em; }\n"
        ".home-hero-mark-copy { color: %s; font-size: 10px; letter-spacing: 0.08em; }\n"
        ".quick-action-grid { margin-top: 2px; }\n"
        ".quick-action { min-height: 66px; }\n"
        ".status-grid { margin-top: 14px; }\n"
        ".status-card { background: %s; border: 1px solid %s; border-radius: 17px; padding: 17px 18px; min-height: 132px; }\n"
        ".status-card:hover { border-color: %s; }\n"
        ".status-card-heading { margin-bottom: 8px; }\n"
        ".status-card-value { color: %s; font-size: 24px; font-weight: %u; }\n"
        ".status-card-detail { color: %s; font-size: 11px; }\n"
        ".status-card-icon { background: %s; border: 1px solid %s; border-radius: 11px; padding: 7px; }\n"
        ".status-card-icon image { color: %s; }\n"
        ".date-status-card { border-top: 2px solid %s; }\n"
        ".region-status-card { border-top: 2px solid %s; }\n"
        ".appearance-status-card { border-top: 2px solid %s; }\n"
        ".network-status-card { border-top: 2px solid %s; }\n"
        ".network-online { color: %s; font-size: 24px; font-weight: %u; }\n"
        ".network-offline { color: %s; font-size: 24px; font-weight: %u; }\n",
        surface, selected,
        accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        card, border,
        accent,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        surface, border,
        accent,
        warm,
        accent,
        warm,
        success,
        success, (unsigned int)type->ui_bold_weight,
        fault, (unsigned int)type->ui_bold_weight);

    g_string_append_printf(
        css,
        ".settings-sidebar { min-width: 300px; }\n"
        ".settings-sidebar list { padding: 2px 0; }\n"
        ".nav-row { min-height: 52px; }\n"
        ".nav-row image { min-width: 30px; }\n"
        ".nav-row.nav-gold image { color: %s; }\n"
        ".nav-row.nav-cyan image { color: %s; }\n"
        ".nav-row.nav-green image { color: %s; }\n"
        ".nav-row:disabled { opacity: 0.42; }\n"
        ".nav-row:selected { box-shadow: inset 0 0 0 1px %s; }\n"
        ".nav-row:selected .nav-primary { color: %s; }\n"
        ".sidebar-footer { margin: 10px 8px 0 8px; }\n"
        ".sidebar-version { padding: 5px 2px; }\n",
        warm,
        accent,
        success,
        accent,
        title);

    g_string_append_printf(
        css,
        ".home-hero { padding: 18px 20px; }\n"
        ".home-hero-mark { min-width: 390px; min-height: 182px; padding: 0; border-radius: 18px; }\n"
        ".hero-scene { min-width: 390px; min-height: 182px; }\n"
        ".hero-brand-overlay { background: rgba(4,12,19,0.76); border: 1px solid %s; border-radius: 13px; padding: 10px 14px; margin: 12px; }\n"
        ".hero-brand-overlay image { color: %s; }\n"
        ".overview-body { margin-top: 2px; }\n"
        ".overview-scene { min-width: 190px; min-height: 126px; border-radius: 13px; }\n"
        ".status-card { min-height: 0; padding: 16px 17px; }\n"
        ".status-card-secondary { color: %s; font-size: 14px; font-weight: %u; }\n"
        ".region-country { color: %s; font-size: 23px; font-weight: %u; }\n"
        ".region-flag { border-radius: 11px; }\n",
        border,
        accent,
        title, (unsigned int)type->ui_bold_weight,
        title, (unsigned int)type->ui_bold_weight);

    g_string_append_printf(
        css,
        ".theme-preview { padding: 5px; border: 1px solid transparent; border-radius: 10px; }\n"
        ".theme-preview-selected { border-color: %s; background: %s; }\n"
        ".theme-preview-window { border: 1px solid %s; border-radius: 8px; }\n"

        ".theme-preview-label { color: %s; font-size: 10px; }\n"
        ".network-visual { min-height: 84px; border-radius: 12px; }\n"
        ".date-status-card, .region-status-card, .appearance-status-card, .network-status-card { min-height: 186px; }\n",
        accent, selected,
        border,
        muted);

    g_string_append_printf(
        css,
        ".window-control { min-width: 30px; min-height: 30px; padding: 4px; background: transparent; border: 1px solid transparent; border-radius: 8px; }\n"
        ".window-control:hover { background: %s; border-color: %s; }\n"
        ".window-control-close:hover { background: %s; color: %s; }\n"
        "scrollbar { background: transparent; min-width: 10px; min-height: 10px; }\n"
        "scrollbar slider { min-width: 8px; min-height: 28px; border-radius: 999px; background: %s; }\n"
        "scrollbar slider:hover { background: %s; }\n",
        surface_hover, border,
        fault, accent_foreground,
        subtle,
        accent);

    g_string_append_printf(
        css,
        ".shell-header { background: %s; border-bottom: 1px solid %s; min-height: 58px; }\n"
        ".header-brand-icon { box-shadow: none; }\n"
        ".settings-sidebar { padding: 12px 9px; min-width: 220px; }\n"
        ".nav-row { min-height: 50px; padding: 7px 9px; margin: 2px 4px; }\n"
        ".nav-icon-well { min-width: 38px; min-height: 38px; border-radius: 11px; padding: 5px; background: %s; border: 1px solid %s; }\n"
        ".nav-gold .nav-icon-well, .nav-cyan .nav-icon-well { box-shadow: none; }\n"
        ".nav-row:selected { background: %s; border-color: %s; box-shadow: none; }\n"
        ".nav-row:selected .nav-primary, .nav-row:selected .nav-secondary { color: %s; }\n"
        ".home-hero { min-height: 210px; padding: 0; border: 0; box-shadow: none; }\n"
        ".hero-scene { min-height: 210px; }\n"
        ".hero-copy-overlay { min-width: 0; padding: 14px 18px; margin: 12px; border-radius: 15px; background: rgba(0,0,0,0.56); }\n"
        ".hero-brand-overlay { margin: 14px; padding: 12px 14px; background: rgba(0,0,0,0.46); border-color: %s; box-shadow: none; }\n"
        ".home-hero-title, .home-hero-accent { font-size: 34px; }\n"
        ".home-feature { background: %s; border-color: %s; }\n"
        ".quick-action { min-height: 68px; padding: 8px 10px; }\n"
        ".quick-action-icon { min-width: 42px; min-height: 42px; border-radius: 12px; padding: 6px; background: %s; border: 1px solid %s; box-shadow: none; }\n"
        ".quick-action-cyan, .quick-action-gold { background: %s; border-color: %s; }\n"
        ".quick-action:hover { box-shadow: none; }\n"
        ".hero-live-column { min-width: 220px; }\n"
        ".overview-panel { min-width: 260px; }\n"
        ".home-card, .status-card { background: %s; box-shadow: none; }\n"
        ".home-card:hover, .status-card:hover { border-color: %s; }\n"
        ".region-status-card, .network-status-card, .appearance-status-card, .date-status-card { background: %s; }\n"
        ".settings-content { padding: 18px 20px 22px 20px; }\n"
        ".hero-copy-overlay, .hero-live-column { min-width: 0; }\n"
        ".home-hero-mark { min-width: 190px; }\n"
        ".asset-missing { background: transparent; border: 1px solid transparent; box-shadow: none; }\n",
        panel, border,
        surface, border,
        selected, accent, accent_foreground,
        border,
        surface, border,
        surface, border,
        card, border,
        card, accent,
        card);

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

