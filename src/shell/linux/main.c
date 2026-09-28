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
#include "home-temporal-presentation.h"

#include "system-settings/project-info.h"

#include <gtk/gtk.h>
#include <infiltratr/core.h>
#include <infiltratr/design.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <locale.h>
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
        GTK_SEARCH_ENTRY(search), "Search settings…");
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
    if (ticker->owner != NULL &&
        !gtk_widget_get_mapped(ticker->owner)) {
        return G_SOURCE_CONTINUE;
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

static void home_temporal_ticker_free(gpointer data)
{
    HomeTemporalTicker *ticker = data;
    if (ticker == NULL) return;
    ss_home_temporal_presenter_free(ticker->presenter);
    g_free(ticker);
}

static void remove_source(gpointer data)
{
    const guint source_id = GPOINTER_TO_UINT(data);
    if (source_id != 0U) g_source_remove(source_id);
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
    if (ticker->owner != NULL &&
        !gtk_widget_get_mapped(ticker->owner)) {
        return G_SOURCE_CONTINUE;
    }
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
    GtkWidget *features = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *grid = gtk_flow_box_new();
    GtkWidget *overview = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *overview_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *overview_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *overview_icon = gtk_image_new_from_icon_name(
        "video-display-symbolic");
    GtkWidget *overview_data = gtk_grid_new();
    GtkWidget *overview_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
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
    gtk_box_append(GTK_BOX(overview_body), overview_scene);
    gtk_widget_set_hexpand(overview_data, TRUE);
    gtk_box_append(GTK_BOX(overview_body), overview_data);
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

    GtkWidget *date_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *date_clock = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *date_clock_label =
        ss_linux_ui_make_label(temporal.clock_text, "home-clock-value");
    GtkWidget *date_calendar_label =
        ss_linux_ui_make_label(temporal.date_text, "home-clock-date");
    gtk_box_append(GTK_BOX(date_clock), date_clock_label);
    gtk_box_append(GTK_BOX(date_clock), date_calendar_label);
    gtk_box_append(GTK_BOX(date_body), date_clock);

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
    gtk_box_append(GTK_BOX(date_body), date_meta);

    GtkWidget *date_scene = make_visual_panel(
        "date-asset.png", 120, 78, "date-scene");
    gtk_widget_set_hexpand(date_scene, TRUE);
    gtk_widget_set_halign(date_scene, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(date_body), date_scene);
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
    region_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 13);
    region_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *region_value_label =
        ss_linux_ui_make_label(format_locale, "region-country");
    GtkWidget *region_detail_label =
        ss_linux_ui_make_label(region_detail, "status-card-detail");
    gtk_box_append(GTK_BOX(region_copy), region_value_label);
    gtk_box_append(GTK_BOX(region_copy), region_detail_label);
    gtk_box_append(GTK_BOX(region_body), region_copy);
    GtkWidget *region_scene = make_ui_asset_picture(
        "region-asset.png", 220, 96, "region-scene");
    if (region_scene != NULL) {
        gtk_widget_set_hexpand(region_scene, TRUE);
        gtk_widget_set_halign(region_scene, GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(region_body), region_scene);
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
    appearance_previews = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_set_homogeneous(GTK_BOX(appearance_previews), TRUE);
    /*
     * These are illustrative choices, not an authoritative theme selector.
     * Do not mark Light/Dark as selected merely because GTK currently prefers
     * one luminance; that would falsely imply Follow OS/Mercedes state.
     */
    gtk_box_append(
        GTK_BOX(appearance_previews),
        make_theme_preview("Light", "theme-light", FALSE));
    gtk_box_append(
        GTK_BOX(appearance_previews),
        make_theme_preview("Dark", "theme-dark", FALSE));
    gtk_box_append(
        GTK_BOX(appearance_previews),
        make_theme_preview("Follow OS", "theme-follow", FALSE));
    gtk_box_append(
        GTK_BOX(appearance_previews),
        make_theme_preview("Mercedes Grey", "theme-mercedes", FALSE));
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
    const guint temporal_source_id = g_timeout_add_full(
        G_PRIORITY_DEFAULT,
        250U,
        refresh_home_temporal,
        ticker,
        home_temporal_ticker_free);
    g_source_set_name_by_id(
        temporal_source_id,
        "[system-settings] live Home temporal presentation");
    g_object_set_data_full(
        G_OBJECT(scroller),
        "system-settings-home-temporal-source",
        GUINT_TO_POINTER(temporal_source_id),
        remove_source);

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
    (void)refresh_home_status(status_ticker);
    const guint status_source_id = g_timeout_add_seconds_full(
        G_PRIORITY_DEFAULT, 5U,
        refresh_home_status, status_ticker, g_free);
    g_source_set_name_by_id(
        status_source_id,
        "[system-settings] live Home system status");
    g_object_set_data_full(
        G_OBJECT(scroller),
        "system-settings-home-status-source",
        GUINT_TO_POINTER(status_source_id),
        remove_source);

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
    install_common_theme();

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
