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
static bool common_theme_state_valid;
static bool common_theme_dark;
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
    const bool dark = system_prefers_dark();
    const InfiltratrThemePalette *palette;
    const InfiltratrDesignMetrics *metrics;

    /*
     * GtkCssProvider is display-global. Replacing an equivalent provider every
     * time a System Settings window is reopened invalidates the complete GTK
     * style tree while the previous window may still be finishing teardown.
     * Only reinstall when the Common palette selection can actually change.
     */
    if (common_theme_provider != NULL &&
        common_theme_state_valid &&
        common_theme_dark == dark) {
        return;
    }

    palette = infiltratr_theme_resolve(INFILTRATR_THEME_SYSTEM, dark);
    metrics = infiltratr_design_metrics();
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
    gchar *heading;
    gchar *summary;
    gchar *kicker;
    gchar *detail_label;
    gchar *note;

    display = gdk_display_get_default();
    if (palette == NULL || metrics == NULL || type == NULL ||
        type->ui_family == NULL || type->brand_family == NULL ||
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
    heading = rgb_css(palette->heading_rgb);
    summary = rgb_css(palette->summary_rgb);
    kicker = rgb_css(palette->kicker_rgb);
    detail_label = rgb_css(palette->detail_label_rgb);
    note = rgb_css(palette->note_rgb);

    /*
     * System Settings adopts Common's strict first-party typography contract.
     * The package carries the three verified MB Corpo faces, so the GTK tree
     * must not name a generic fallback that can silently change suite identity.
     * Ordinary controls inherit MB Corpo S; deliberate display roles below use
     * MB Corpo A Cond.
     */
    css = g_string_new(NULL);

    g_string_append_printf(
        css,
        "* { font-family: '%s'; font-weight: %u; }\n"
        "window { background: %s; color: %s; }\n"
        ".app-shell, scrolledwindow, viewport { background: %s; }\n"
        "headerbar, .shell-header { background: %s; border-bottom: 1px solid %s; min-height: 58px; padding: 0 10px; }\n"
        ".header-brand { padding: 2px 4px; }\n"
        ".header-brand-icon { background: %s; border: 1px solid %s; border-radius: 12px; padding: 7px; box-shadow: none; }\n"
        ".header-brand-icon image { color: %s; }\n"
        ".header-brand-title { color: %s; font-size: 20px; font-weight: %u; }\n"
        ".header-brand-subtitle { color: %s; font-size: 11px; }\n"
        ".header-end { margin-left: 10px; }\n"
        ".settings-search { min-width: 180px; min-height: 34px; background: %s; color: %s; border: 1px solid %s; border-radius: 10px; padding: 8px 12px; }\n"
        ".settings-search:focus { border-color: %s; }\n"
        ".window-control { min-width: 30px; min-height: 30px; padding: 4px; background: transparent; border: 1px solid transparent; border-radius: 6px; }\n"
        ".window-control:hover { background: %s; border-color: %s; }\n"
        ".window-control-close:hover { background: %s; color: %s; }\n"
        ".settings-sidebar { background: %s; border-right: 1px solid %s; padding: 12px 9px; min-width: 205px; }\n"
        ".settings-sidebar list, .settings-sidebar row, flowbox, flowboxchild { background: transparent; }\n"
        ".settings-sidebar list { padding: 2px 0; }\n"
        ".nav-title { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.10em; margin: 0 10px 8px 10px; }\n"
        ".nav-row { min-height: 50px; border: 1px solid transparent; border-radius: 12px; padding: 7px 9px; margin: 2px 4px; }\n"
        ".nav-row:hover { background: %s; }\n"
        ".nav-row:selected { background: %s; border-color: %s; box-shadow: none; }\n"
        ".nav-row:selected .nav-primary, .nav-row:selected .nav-secondary { color: %s; }\n"
        ".nav-row:disabled { opacity: 0.42; }\n"
        ".nav-icon-well { min-width: 38px; min-height: 38px; border-radius: 10px; padding: 5px; background: %s; border: 1px solid %s; }\n"
        ".nav-icon-well image { color: %s; }\n"
        ".nav-row:selected .nav-icon-well image { color: %s; }\n"
        ".nav-primary { color: %s; font-size: 14px; font-weight: %u; }\n"
        ".nav-secondary { color: %s; font-size: 11px; }\n"
        ".sidebar-footer { border-top: 1px solid %s; padding-top: 12px; margin: 10px 8px 0 8px; }\n"
        ".sidebar-version { color: %s; font-size: 10px; padding: 5px 2px; }\n"
        "scrollbar { background: transparent; min-width: 6px; min-height: 6px; padding: 0; }\n"
        "scrollbar trough { background: transparent; }\n"
        "scrollbar slider { min-width: 4px; min-height: 20px; margin: 1px; border-radius: 2px; background: %s; }\n"
        "scrollbar slider:hover { background: %s; }\n",
        type->ui_family, (unsigned int)type->ui_regular_weight,
        background, text,
        background,
        panel, border,
        card, border, accent,
        heading, (unsigned int)type->ui_bold_weight,
        summary,
        input, text, status_border, accent,
        surface_hover, border,
        fault, accent_foreground,
        panel, border,
        kicker, (unsigned int)type->ui_bold_weight,
        surface_hover,
        selected, accent, accent_foreground,
        surface, border, accent, accent_foreground,
        text, (unsigned int)type->ui_bold_weight,
        detail_label,
        border,
        detail_label,
        subtle, accent);

    g_string_append_printf(
        css,
        ".settings-content { padding: 16px; }\n"
        ".page-header { padding: 2px 2px 4px 2px; }\n"
        ".page-icon { min-width: 54px; min-height: 54px; background: %s; border: 1px solid %s; border-radius: 12px; padding: 10px; }\n"
        ".page-icon image { color: %s; }\n"
        ".page-eyebrow { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.12em; }\n"
        ".page-title { font-family: '%s'; color: %s; font-size: 32px; font-weight: %u; }\n"
        ".page-summary { color: %s; font-size: 13px; margin-bottom: 8px; }\n"
        ".hero-card { background: %s; border: 1px solid %s; border-radius: 18px; padding: 16px 18px; box-shadow: none; }\n"
        ".hero-top { min-height: 76px; }\n"
        ".hero-live-column { min-width: 0; padding: 4px 10px 4px 0; }\n"
        ".hero-kicker { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.11em; }\n"
        ".preview-time { color: %s; font-size: 40px; font-weight: %u; }\n"
        ".preview-date { color: %s; font-size: 14px; }\n"
        ".hero-note { color: %s; font-size: 10px; margin-top: 3px; }\n"
        ".hero-badge { background: %s; border: 1px solid %s; border-radius: 999px; padding: 6px 10px; }\n"
        ".hero-badge image, .hero-badge-label { color: %s; }\n"
        ".hero-badge-label { font-size: 10px; font-weight: %u; letter-spacing: 0.10em; }\n"
        ".overview-panel { min-width: 250px; background: %s; border: 1px solid %s; border-radius: 12px; padding: 10px; }\n"
        ".overview-heading { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.11em; margin-bottom: 3px; }\n"
        ".overview-item { background: %s; border: 1px solid %s; border-radius: 10px; padding: 7px 9px; min-height: 46px; }\n"
        ".overview-item:hover { background: %s; }\n"
        ".overview-icon { background: %s; border: 1px solid %s; border-radius: 10px; padding: 7px; }\n"
        ".overview-icon image { color: %s; }\n"
        ".overview-label, .overview-key { color: %s; font-size: 10px; }\n"
        ".overview-value, .overview-data { color: %s; font-size: 13px; font-weight: %u; }\n"
        ".overview-link-button { background: transparent; color: %s; border: 0; }\n"
        ".settings-card { background: %s; border: 1px solid %s; border-top-width: 2px; border-radius: 12px; padding: 14px; box-shadow: none; }\n"
        ".location-card { border-top-color: %s; }\n"
        ".presentation-card { border-top-color: %s; }\n"
        ".system-card { border-top-color: %s; }\n"
        ".section-heading { margin-bottom: 6px; padding-bottom: 5px; border-bottom: 1px solid %s; }\n"
        ".section-icon-wrap { background: %s; border: 1px solid %s; border-radius: 10px; padding: 7px; }\n"
        ".section-icon-wrap image { color: %s; }\n"
        ".section-title { font-family: '%s'; color: %s; font-size: 17px; font-weight: %u; }\n"
        ".section-summary { color: %s; font-size: 11px; }\n"
        ".setting-row { border-radius: 10px; padding: 11px 12px; }\n"
        ".setting-row:hover { background: %s; }\n"
        ".setting-tile { background: %s; border: 1px solid %s; border-radius: 10px; padding: 8px 10px; margin: 2px 0; }\n"
        ".setting-tile:hover { background: %s; border-color: %s; }\n"
        ".setting-tile-icon { min-width: 32px; min-height: 32px; background: %s; border: 1px solid %s; border-radius: 10px; padding: 5px; }\n"
        ".setting-tile-icon image { color: %s; }\n"
        ".setting-label { color: %s; font-weight: %u; }\n"
        ".setting-description { color: %s; font-size: 11px; }\n"
        ".field-caption { color: %s; font-size: 10px; font-weight: %u; }\n"
        ".accent-note { color: %s; font-size: 11px; }\n"
        ".info-strip { background: %s; border: 1px solid %s; border-radius: 10px; padding: 7px 9px; }\n"
        ".info-strip image { color: %s; }\n"
        ".status-ok, .error, .page-status { border-radius: 10px; padding: 8px 10px; font-size: 11px; }\n"
        ".status-ok { color: %s; background: %s; border: 1px solid %s; }\n"
        ".error { color: %s; background: %s; border: 1px solid %s; }\n",
        surface, border, accent,
        kicker, (unsigned int)type->ui_bold_weight,
        type->brand_family, heading, (unsigned int)type->brand_weight,
        summary,
        card, border,
        kicker, (unsigned int)type->ui_bold_weight,
        heading, (unsigned int)type->ui_bold_weight,
        summary,
        note,
        surface, success, success,
        (unsigned int)type->ui_bold_weight,
        surface, border,
        kicker, (unsigned int)type->ui_bold_weight,
        card, border,
        surface_hover,
        surface, border, accent,
        detail_label,
        heading, (unsigned int)type->ui_bold_weight,
        accent,
        card, border,
        accent, accent, success,
        border,
        surface, border, accent,
        type->brand_family, heading, (unsigned int)type->brand_weight,
        summary,
        surface_hover,
        surface, border,
        surface_hover, subtle,
        card, border, accent,
        text, (unsigned int)type->ui_bold_weight,
        detail_label,
        detail_label, (unsigned int)type->ui_bold_weight,
        note,
        surface, border, accent,
        success, surface, status_border,
        fault, surface, fault);

    g_string_append_printf(
        css,
        ".setting-dropdown, .setting-spin, .setting-entry, .setting-button { min-height: 34px; border-radius: %upx; }\n"
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
        ".setting-switch { min-width: 38px; min-height: 20px; background: %s; border: 1px solid %s; border-radius: 10px; box-shadow: none; }\n"
        ".setting-switch:hover { background: %s; border-color: %s; }\n"
        ".setting-switch:checked { background: %s; border-color: %s; }\n"
        ".setting-switch:checked:hover { background: %s; border-color: %s; }\n"
        ".setting-switch slider { min-width: 16px; min-height: 16px; margin: 1px; background: %s; border: none; border-radius: 8px; box-shadow: none; }\n"
        ".setting-switch:checked slider { background: %s; }\n"
        ".setting-switch:disabled { opacity: 0.50; }\n"
        ".location-results { background: %s; border: 1px solid %s; border-radius: 10px; }\n"
        ".location-results row { border-radius: 10px; margin: 2px 4px; }\n"
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
        heading, accent_foreground,
        card, border,
        surface_hover);

    g_string_append_printf(
        css,
        ".home-page { padding: 16px; }\n"
        ".home-hero { min-height: 300px; padding: 0; border: 0; border-radius: 18px; box-shadow: none; }\n"
        ".hero-scene { min-width: 190px; min-height: 300px; }\n"
        ".hero-copy-overlay { min-width: 0; padding: 20px 24px; margin: 16px; border: 1px solid rgba(255,255,255,0.08); border-radius: 14px; background: rgba(0,0,0,0.52); }\n"
        ".hero-brand-overlay { margin: 16px; padding: 14px 16px; background: rgba(0,0,0,0.46); border: 1px solid %s; border-radius: 12px; box-shadow: none; }\n"
        ".hero-brand-overlay image { color: %s; }\n"
        ".home-hero-eyebrow { color: %s; font-size: 10px; font-weight: %u; letter-spacing: 0.12em; margin-bottom: 5px; }\n"
        ".home-hero-title { font-family: '%s'; color: %s; font-size: 34px; font-weight: %u; margin-bottom: 2px; }\n"
        ".home-hero-accent { font-family: '%s'; color: %s; font-size: 34px; font-weight: %u; margin-bottom: 6px; }\n"
        ".home-hero-subtitle { color: %s; font-size: 15px; margin-bottom: 4px; }\n"
        ".home-hero-mark { min-width: 190px; min-height: 182px; padding: 0; border: 1px solid %s; border-radius: 18px; background: %s; }\n"
        ".home-hero-mark image { color: %s; }\n"
        ".home-hero-mark-title { color: %s; font-size: 17px; font-weight: %u; letter-spacing: 0.08em; }\n"
        ".home-hero-mark-copy { color: %s; font-size: 10px; letter-spacing: 0.08em; }\n"
        ".home-feature-row { margin-top: 18px; }\n"
        ".home-grid, .status-grid { margin-top: 14px; }\n"
        ".home-feature { min-height: 48px; background: %s; border: 1px solid %s; border-radius: 12px; padding: 8px 12px; }\n"
        ".home-feature image { color: %s; }\n"
        ".home-feature-title { color: %s; font-weight: %u; }\n"
        ".home-feature-copy { color: %s; font-size: 10px; }\n"
        ".home-card, .status-card { background: %s; border: 1px solid %s; border-radius: 12px; padding: 16px; box-shadow: none; }\n"
        ".home-card:hover, .status-card:hover { border-color: %s; }\n"
        ".home-card-title { font-family: '%s'; color: %s; font-size: 18px; font-weight: %u; }\n"
        ".home-card-icon, .status-card-icon, .location-pin-well { background: %s; border: 1px solid %s; border-radius: 12px; padding: 8px; }\n"
        ".home-card-icon image, .status-card-icon image, .location-pin-well image { color: %s; }\n"
        ".quick-action-grid { margin-top: 2px; }\n"
        ".quick-action { min-height: 68px; padding: 8px 10px; background: %s; border: 1px solid %s; border-radius: 10px; }\n"
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
        ".overview-scene { min-width: 190px; min-height: 126px; border-radius: 12px; }\n"
        ".network-visual { min-height: 84px; border-radius: 12px; }\n"
        ".theme-preview { padding: 5px; border: 1px solid transparent; border-radius: 10px; }\n"
        ".theme-preview-selected { border-color: %s; background: %s; }\n"
        ".theme-preview-window { border: 1px solid %s; border-radius: 6px; }\n"
        ".theme-preview-label { color: %s; font-size: 10px; }\n"
        ".asset-missing { background: transparent; border: 1px solid transparent; box-shadow: none; }\n",
        border, accent,
        kicker, (unsigned int)type->ui_bold_weight,
        type->brand_family, heading, (unsigned int)type->brand_weight,
        type->brand_family, accent, (unsigned int)type->brand_weight,
        summary,
        border, surface, accent,
        heading, (unsigned int)type->ui_bold_weight,
        detail_label,
        surface, border, accent,
        heading, (unsigned int)type->ui_bold_weight,
        detail_label,
        card, border,
        accent,
        type->brand_family, heading, (unsigned int)type->brand_weight,
        surface, border, accent,
        surface, border,
        surface_hover, accent,
        surface, border, accent,
        heading, (unsigned int)type->ui_bold_weight,
        detail_label,
        accent,
        heading, (unsigned int)type->ui_bold_weight,
        summary,
        accent, accent, accent, success,
        accent, selected,
        border,
        detail_label);

    /*
     * Shared geometry comes from Common. Component-specific sizes stay local,
     * but suite-level padding and radius roles must follow the current design
     * contract instead of drifting as copied literals.
     */
    g_string_append_printf(
        css,
        ".settings-content { padding: %upx; }\n"
        ".home-page { padding: %upx; }\n"
        ".window-control, .theme-preview-window { border-radius: %upx; }\n"
        ".settings-search, .nav-icon-well, .overview-item, .overview-icon, "
        ".section-icon-wrap, .setting-row, .setting-tile, .setting-tile-icon, "
        ".info-strip, .status-ok, .error, .page-status, .location-results, "
        ".location-results row, .quick-action, .theme-preview { border-radius: %upx; }\n"
        ".header-brand-icon, .nav-row, .page-icon, .overview-panel, "
        ".settings-card, .hero-copy-overlay, .hero-brand-overlay, .home-feature, "
        ".home-card, .status-card, .home-card-icon, .status-card-icon, "
        ".location-pin-well, .overview-scene, .network-visual { border-radius: %upx; }\n"
        ".hero-card, .home-hero, .home-hero-mark { border-radius: %upx; }\n",
        (unsigned int)metrics->content_padding,
        (unsigned int)metrics->screen_padding,
        (unsigned int)metrics->small_radius,
        (unsigned int)metrics->control_radius,
        (unsigned int)metrics->card_radius,
        (unsigned int)metrics->panel_radius);

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
    common_theme_dark = dark;
    common_theme_state_valid = true;
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
    g_free(heading);
    g_free(summary);
    g_free(kicker);
    g_free(detail_label);
    g_free(note);
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