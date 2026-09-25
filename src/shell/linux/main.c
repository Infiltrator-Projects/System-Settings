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
#include <sys/utsname.h>

typedef struct {
    GtkListBox *list;
    GtkWindow *parent;
    GtkStack *stack;
    GtkListBoxRow *home_row;
    GtkListBoxRow *date_row;
    gchar *query;
} ShellSearchState;

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
        ".settings-search { min-width: 290px; background: %s; color: %s; border: 1px solid %s; border-radius: 14px; padding: 8px 12px; }\n"
        ".settings-search:focus { border-color: %s; }\n"
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
        ".home-feature-row { margin-top: 16px; }\n"
        ".home-feature { background: %s; border: 1px solid %s; border-radius: 12px; padding: 10px 12px; }\n"
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
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_box_append(GTK_BOX(box), icon);
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

static GtkWidget *build_header(GtkWindow *parent, GtkSearchEntry **search_out)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    GtkWidget *header = gtk_header_bar_new();
    GtkWidget *brand = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *icon = gtk_image_new_from_icon_name(info->icon_name);
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *search = gtk_search_entry_new();
    GtkWidget *empty_title = gtk_label_new("");

    (void)parent;
    gtk_widget_add_css_class(header, "shell-header");
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
        GTK_SEARCH_ENTRY(search), "Search settings…");
    gtk_widget_add_css_class(search, "settings-search");
    gtk_widget_set_size_request(search, 320, -1);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), search);

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
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    g_autofree gchar *version =
        g_strdup_printf("Version %s", info->version);

    gtk_widget_set_size_request(sidebar, 312, -1);
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
        "date time clock calendar location timezone");
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
        "Display & Appearance",
        "Theme, scaling & desktop",
        "display appearance theme scaling desktop",
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
        "Hardware",
        "Devices, drivers & system info",
        "hardware devices drivers system info",
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

    gtk_list_box_select_row(
        GTK_LIST_BOX(list), GTK_LIST_BOX_ROW(home_row));
    gtk_box_append(GTK_BOX(sidebar), list);

    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(sidebar), spacer);

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
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_widget_add_css_class(box, "home-feature");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
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

static void append_overview_row(GtkGrid *grid,
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
}

static void open_date_time(GtkButton *button, gpointer user_data)
{
    (void)button;
    gtk_stack_set_visible_child_name(GTK_STACK(user_data), "date-time");
}

static void focus_settings_search(GtkButton *button, gpointer user_data)
{
    (void)button;
    if (user_data != NULL) {
        gtk_widget_grab_focus(GTK_WIDGET(user_data));
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
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_widget_add_css_class(button, "quick-action");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 28);
    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(
        GTK_BOX(text),
        ss_linux_ui_make_label(title, "quick-action-title"));
    gtk_box_append(
        GTK_BOX(text),
        ss_linux_ui_make_label(copy, "quick-action-copy"));
    gtk_box_append(GTK_BOX(box), text);
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

static GtkWidget *build_home_page(GtkWindow *parent,
                                  GtkStack *stack,
                                  GtkSearchEntry *search_entry)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *hero = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 22);
    GtkWidget *hero_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *hero_mark = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *hero_icon = gtk_image_new_from_icon_name(
        "video-display-symbolic");
    GtkWidget *features = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *overview = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *overview_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *overview_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *overview_icon = gtk_image_new_from_icon_name(
        "video-display-symbolic");
    GtkWidget *overview_data = gtk_grid_new();
    GtkWidget *quick = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *quick_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *quick_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *quick_icon = gtk_image_new_from_icon_name(
        "system-run-symbolic");
    GtkWidget *quick_grid = gtk_grid_new();
    GtkWidget *status_grid = gtk_grid_new();
    GtkWidget *date_card;
    GtkWidget *region_card;
    GtkWidget *appearance_card;
    GtkWidget *network_card;
    GtkWidget *date_open;
    GtkWidget *date_spacer;
    GtkWidget *scroller = gtk_scrolled_window_new();
    struct utsname uts;
    g_autofree gchar *os_name = g_get_os_info(G_OS_INFO_KEY_PRETTY_NAME);
    const char *desktop = g_getenv("XDG_CURRENT_DESKTOP");
    const char *host = g_get_host_name();
    const char *kernel = uname(&uts) == 0 ? uts.release : "Unknown";
    const char *architecture = uname(&uts) == 0 ? uts.machine : "Unknown";
    const gchar *const *languages = g_get_language_names();
    const char *locale_name =
        languages != NULL && languages[0] != NULL
            ? languages[0]
            : "Unknown";
    GtkSettings *gtk_settings = gtk_settings_get_default();
    gchar *theme_name = NULL;
    gboolean prefer_dark = FALSE;
    GNetworkMonitor *network = g_network_monitor_get_default();
    gboolean online = FALSE;
    gboolean metered = FALSE;
    GNetworkConnectivity connectivity = G_NETWORK_CONNECTIVITY_LOCAL;
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree gchar *time_text =
        now != NULL ? g_date_time_format(now, "%X") : g_strdup("Unknown");
    g_autofree gchar *date_text =
        now != NULL ? g_date_time_format(now, "%A, %e %B %Y") : g_strdup("");
    g_autofree gchar *region_detail = NULL;
    g_autofree gchar *appearance_detail = NULL;
    g_autofree gchar *network_detail = NULL;

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

    region_detail = g_strdup_printf("Locale %s", locale_name);
    appearance_detail = g_strdup_printf(
        "%s presentation",
        prefer_dark ? "Dark" : "Light");
    network_detail = g_strdup_printf(
        "%s%s",
        network_connectivity_text(connectivity),
        metered ? " • Metered" : "");

    gtk_widget_add_css_class(page, "home-page");
    gtk_widget_add_css_class(hero, "home-hero");
    gtk_widget_set_hexpand(hero_copy, TRUE);
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
    gtk_box_set_homogeneous(GTK_BOX(features), TRUE);
    gtk_box_append(
        GTK_BOX(features),
        make_feature("emblem-ok-symbolic", "Simple", "Easy to use"));
    gtk_box_append(
        GTK_BOX(features),
        make_feature("security-high-symbolic", "Secure", "Built for privacy"));
    gtk_box_append(
        GTK_BOX(features),
        make_feature("video-display-symbolic", "Beautiful", "A desktop you’ll love"));
    gtk_box_append(GTK_BOX(hero_copy), features);
    gtk_box_append(GTK_BOX(hero), hero_copy);

    gtk_widget_add_css_class(hero_mark, "home-hero-mark");
    gtk_widget_set_valign(hero_mark, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(hero_mark, GTK_ALIGN_CENTER);
    gtk_image_set_pixel_size(GTK_IMAGE(hero_icon), 78);
    gtk_box_append(GTK_BOX(hero_mark), hero_icon);
    gtk_box_append(
        GTK_BOX(hero_mark),
        ss_linux_ui_make_label("INFILTRATOR OS", "home-hero-mark-title"));
    gtk_box_append(
        GTK_BOX(hero_mark),
        ss_linux_ui_make_label(
            "GRAPHICAL SYSTEM CONTROL",
            "home-hero-mark-copy"));
    gtk_box_append(GTK_BOX(hero), hero_mark);
    gtk_box_append(GTK_BOX(page), hero);

    gtk_widget_add_css_class(grid, "home-grid");
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

    gtk_widget_add_css_class(overview, "home-card");
    gtk_widget_add_css_class(overview_icon_wrap, "home-card-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(overview_icon), 22);
    gtk_box_append(GTK_BOX(overview_icon_wrap), overview_icon);
    gtk_box_append(GTK_BOX(overview_heading), overview_icon_wrap);
    gtk_box_append(
        GTK_BOX(overview_heading),
        ss_linux_ui_make_label("System Overview", "home-card-title"));
    gtk_box_append(GTK_BOX(overview), overview_heading);
    gtk_grid_set_column_spacing(GTK_GRID(overview_data), 18);
    gtk_grid_set_row_spacing(GTK_GRID(overview_data), 8);
    append_overview_row(
        GTK_GRID(overview_data), 0, "Operating system", os_name);
    append_overview_row(
        GTK_GRID(overview_data), 1, "Kernel", kernel);
    append_overview_row(
        GTK_GRID(overview_data), 2, "Architecture", architecture);
    append_overview_row(
        GTK_GRID(overview_data), 3, "Desktop", desktop);
    append_overview_row(
        GTK_GRID(overview_data), 4, "Hostname", host);
    gtk_box_append(GTK_BOX(overview), overview_data);
    gtk_grid_attach(GTK_GRID(grid), overview, 0, 0, 1, 1);

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
    gtk_grid_set_column_spacing(GTK_GRID(quick_grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(quick_grid), 8);
    gtk_grid_set_column_homogeneous(GTK_GRID(quick_grid), TRUE);

    GtkWidget *date_action = make_quick_action(
        "preferences-system-time-symbolic",
        "Set Date & Time",
        "Time zone, clock & calendar");
    g_signal_connect(
        date_action, "clicked",
        G_CALLBACK(open_date_time), stack);
    gtk_grid_attach(GTK_GRID(quick_grid), date_action, 0, 0, 1, 1);

    GtkWidget *region_action = make_program_action(
        "preferences-desktop-locale-symbolic",
        "Change Region",
        "Language, formats & location",
        "mintlocale",
        NULL);
    gtk_grid_attach(GTK_GRID(quick_grid), region_action, 1, 0, 1, 1);

    GtkWidget *display_action = make_program_action(
        "video-display-symbolic",
        "Configure Display",
        "Scaling, layout & monitors",
        "cinnamon-settings",
        "display");
    gtk_grid_attach(GTK_GRID(quick_grid), display_action, 0, 1, 1, 1);

    GtkWidget *software_action = make_program_action(
        "system-software-install-symbolic",
        "Software & Updates",
        "Apps, packages & updates",
        "infiltrator-software",
        NULL);
    gtk_grid_attach(GTK_GRID(quick_grid), software_action, 1, 1, 1, 1);

    gtk_box_append(GTK_BOX(quick), quick_grid);
    gtk_grid_attach(GTK_GRID(grid), quick, 1, 0, 1, 1);
    gtk_box_append(GTK_BOX(page), grid);

    gtk_widget_add_css_class(status_grid, "status-grid");
    gtk_grid_set_column_spacing(GTK_GRID(status_grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(status_grid), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(status_grid), TRUE);

    date_card = make_status_card(
        "date-status-card",
        "preferences-system-time-symbolic",
        "Date & Time",
        time_text,
        date_text);
    date_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(date_spacer, TRUE);
    gtk_box_append(GTK_BOX(date_card), date_spacer);
    date_open = gtk_button_new_with_label("Open Date & Time");
    gtk_widget_add_css_class(date_open, "primary-button");
    gtk_widget_set_halign(date_open, GTK_ALIGN_START);
    g_signal_connect(
        date_open, "clicked",
        G_CALLBACK(open_date_time), stack);
    gtk_box_append(GTK_BOX(date_card), date_open);
    gtk_grid_attach(GTK_GRID(status_grid), date_card, 0, 0, 1, 1);

    region_card = make_status_card(
        "region-status-card",
        "preferences-desktop-locale-symbolic",
        "Region & Language",
        locale_name,
        region_detail);
    gtk_grid_attach(GTK_GRID(status_grid), region_card, 1, 0, 1, 1);

    appearance_card = make_status_card(
        "appearance-status-card",
        "preferences-desktop-theme-symbolic",
        "Display & Appearance",
        theme_name != NULL ? theme_name : "System theme",
        appearance_detail);
    gtk_grid_attach(GTK_GRID(status_grid), appearance_card, 0, 1, 1, 1);

    network_card = make_status_card(
        "network-status-card",
        "network-wired-symbolic",
        "Network",
        online ? "Connected" : "Offline",
        network_detail);
    gtk_widget_add_css_class(
        gtk_widget_get_next_sibling(
            gtk_widget_get_first_child(network_card)),
        online ? "network-online" : "network-offline");
    gtk_grid_attach(GTK_GRID(status_grid), network_card, 1, 1, 1, 1);

    gtk_box_append(GTK_BOX(page), status_grid);

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroller),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(scroller), page);

    g_free(theme_name);
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
    install_common_theme();

    window = GTK_WINDOW(
        gtk_application_window_new(application));
    gtk_window_set_title(window, info->program_name);
    gtk_window_set_default_size(window, 1420, 900);
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
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(date_scroller),
        panel_widget);
    gtk_stack_add_named(
        stack,
        build_home_page(window, stack, search_entry),
        "home");
    gtk_stack_add_named(
        stack,
        date_scroller,
        "date-time");
    gtk_stack_set_visible_child_name(stack, "home");
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
