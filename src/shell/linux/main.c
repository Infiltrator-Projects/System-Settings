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
#include "system-settings/location-metadata.h"

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
        ".home-hero { padding: 18px 20px; overflow: hidden; }\n"
        ".home-hero-mark { min-width: 390px; min-height: 182px; padding: 0; overflow: hidden; border-radius: 18px; }\n"
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
        ".theme-light { background-image: linear-gradient(to bottom, #f4f6f8 0 22%%, #dce2e8 22%% 100%%); }\n"
        ".theme-dark { background-image: linear-gradient(to bottom, #24282d 0 22%%, #0f1216 22%% 100%%); }\n"
        ".theme-follow { background-image: linear-gradient(135deg, #e9eef3 0 50%%, #171b20 50%% 100%%); }\n"
        ".theme-mercedes { background-image: linear-gradient(to bottom, #53575b 0 22%%, #202326 22%% 100%%); }\n"
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
        ".shell-header { background-image: linear-gradient(to right, #06131f, #08263a); min-height: 58px; }\n"
        ".header-brand-icon { box-shadow: 0 0 18px rgba(0,183,255,0.18); }\n"
        ".settings-sidebar { padding: 14px 10px 12px 10px; }\n"
        ".nav-row { min-height: 58px; padding: 7px 9px; margin: 2px 4px; }\n"
        ".nav-icon-well { min-width: 43px; min-height: 43px; border-radius: 12px; padding: 5px; background: %s; border: 1px solid %s; }\n"
        ".nav-gold .nav-icon-well { box-shadow: 0 0 16px rgba(218,164,58,0.10); }\n"
        ".nav-cyan .nav-icon-well { box-shadow: 0 0 16px rgba(0,183,255,0.12); }\n"
        ".nav-row:selected { background-image: linear-gradient(to right, rgba(0,174,255,0.88), rgba(0,112,194,0.72)); box-shadow: inset 0 0 0 1px #47d4ff, 0 0 18px rgba(0,183,255,0.18); }\n"
        ".nav-row:selected .nav-primary, .nav-row:selected .nav-secondary { color: #ffffff; }\n"
        ".nav-row:selected .nav-icon-well { background: rgba(3,18,28,0.46); border-color: rgba(255,255,255,0.28); }\n"
        ".home-hero { min-height: 238px; padding: 0; border-color: #0b8fc4; }\n"
        ".hero-scene { min-height: 238px; }\n"
        ".hero-copy-overlay { min-width: 560px; padding: 16px 24px; margin: 14px; border-radius: 17px; background: rgba(3,11,18,0.66); }\n"
        ".hero-brand-overlay { margin: 18px; padding: 14px 18px; background: rgba(3,11,18,0.68); box-shadow: 0 0 28px rgba(0,183,255,0.15); }\n",
        surface, border);

    g_string_append_printf(
        css,
        ".home-hero-title, .home-hero-accent { font-size: 38px; }\n"
        ".home-feature { background: rgba(4,15,24,0.64); border-color: rgba(86,176,219,0.36); }\n"
        ".quick-action { min-height: 76px; padding: 9px 11px; }\n"
        ".quick-action-icon { min-width: 48px; min-height: 48px; border-radius: 13px; padding: 7px; background: rgba(4,17,27,0.74); border: 1px solid rgba(85,189,235,0.30); }\n"
        ".quick-action-arrow { opacity: 0.70; }\n"
        ".quick-action-cyan { background-image: linear-gradient(135deg, %s, %s); border-color: %s; }\n"
        ".quick-action-gold { background-image: linear-gradient(135deg, %s, #3a2507); border-color: %s; }\n"
        ".quick-action:hover { box-shadow: 0 0 18px rgba(0,183,255,0.14); }\n"
        ".home-clock-value { color: %s; font-size: 32px; font-weight: %u; }\n"
        ".home-clock-date { color: %s; font-size: 13px; }\n"
        ".location-pin-well { background: %s; border: 1px solid %s; border-radius: 12px; padding: 8px; }\n"
        ".location-pin-well image { color: %s; }\n"
        ".location-primary { color: %s; font-size: 18px; font-weight: %u; }\n"
        ".location-coordinates { color: %s; font-size: 11px; }\n"
        ".date-scene { min-width: 190px; min-height: 96px; border-radius: 13px; }\n"
        ".card-arrow { background: transparent; border: 0; min-width: 32px; min-height: 32px; }\n"
        ".card-arrow image { color: %s; }\n",
        surface, panel, accent,
        panel, warm,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        surface, border,
        warm,
        title, (unsigned int)type->ui_bold_weight,
        muted,
        accent);

    g_string_append_printf(
        css,
        ".home-hero { box-shadow: 0 0 28px rgba(0,160,220,0.13); }\n"
        ".hero-copy-overlay { background: linear-gradient(to right, rgba(2,8,14,0.78), rgba(2,8,14,0.34), rgba(2,8,14,0.06)); border: 0; min-width: 610px; }\n"
        ".hero-brand-overlay { background: rgba(2,9,15,0.42); border-color: rgba(70,210,255,0.38); }\n"
        ".home-card, .status-card { background-image: linear-gradient(145deg, %s, %s); box-shadow: 0 6px 18px rgba(0,0,0,0.18); }\n"
        ".home-card:hover, .status-card:hover { border-color: %s; }\n"
        ".overview-scene, .date-scene, .region-scene, .network-visual { border: 1px solid %s; border-radius: 13px; }\n"
        ".overview-link-button { background: transparent; border: 0; color: %s; font-size: 11px; padding: 4px 8px; }\n"
        ".overview-link-button:hover { color: %s; text-decoration-line: underline; }\n"
        ".quick-action-icon { box-shadow: 0 0 18px rgba(0,183,255,0.12); }\n"
        ".region-status-card { background-image: linear-gradient(135deg, %s, #071725); }\n"
        ".network-status-card { background-image: linear-gradient(135deg, %s, #04131f); }\n"
        ".appearance-status-card { background-image: linear-gradient(135deg, %s, #141719); }\n"
        ".date-status-card { background-image: linear-gradient(135deg, %s, #11161b); }\n",
        card, panel,
        accent,
        border,
        accent,
        warm,
        card, card, card, card);

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
    gtk_widget_set_focusable(button, FALSE);
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
    gtk_widget_set_size_request(search, 320, -1);
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
    GtkLabel *clock;
    GtkLabel *date;
    GtkLabel *system_time;
} HomeTemporalTicker;

static gboolean refresh_home_temporal(gpointer user_data)
{
    HomeTemporalTicker *ticker = user_data;
    SsHomeTemporalPresentation temporal = {0};

    if (ticker == NULL ||
        ticker->clock == NULL ||
        ticker->date == NULL ||
        ticker->system_time == NULL) {
        return G_SOURCE_REMOVE;
    }

    if (ss_home_temporal_presentation_now(&temporal)) {
        gtk_label_set_text(ticker->clock, temporal.clock_text);
        gtk_label_set_text(ticker->date, temporal.date_text);
        gtk_label_set_text(ticker->system_time, temporal.system_time_text);
    }
    ss_home_temporal_presentation_clear(&temporal);
    return G_SOURCE_CONTINUE;
}

static void remove_home_temporal_source(gpointer data)
{
    const guint source_id = GPOINTER_TO_UINT(data);

    if (source_id != 0U) {
        g_source_remove(source_id);
    }
}

static void open_date_time(GtkButton *button, gpointer user_data)
{
    (void)button;
    gtk_stack_set_visible_child_name(GTK_STACK(user_data), "date-time");
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

#define SYSTEM_SETTINGS_UI_ASSET_DIR "/usr/share/infiltrator/system-settings/ui"

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
    path = g_build_filename(
        SYSTEM_SETTINGS_UI_ASSET_DIR, filename, NULL);
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

static void draw_scenic_panel(GtkDrawingArea *area,
                              cairo_t *cr,
                              int width,
                              int height,
                              gpointer user_data)
{
    (void)area;
    (void)user_data;
    const double w = (double)width;
    const double h = (double)height;

    cairo_pattern_t *sky = cairo_pattern_create_linear(0.0, 0.0, 0.0, h);
    cairo_pattern_add_color_stop_rgb(sky, 0.0, 0.01, 0.07, 0.13);
    cairo_pattern_add_color_stop_rgb(sky, 0.45, 0.02, 0.18, 0.28);
    cairo_pattern_add_color_stop_rgb(sky, 0.72, 0.78, 0.30, 0.08);
    cairo_pattern_add_color_stop_rgb(sky, 1.0, 0.03, 0.08, 0.12);
    cairo_set_source(cr, sky);
    cairo_rectangle(cr, 0.0, 0.0, w, h);
    cairo_fill(cr);
    cairo_pattern_destroy(sky);

    cairo_set_source_rgba(cr, 1.0, 0.63, 0.14, 0.92);
    cairo_arc(cr, w * 0.72, h * 0.44, h * 0.075, 0.0, 6.283185307179586);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.03, 0.07, 0.10);
    cairo_move_to(cr, 0.0, h * 0.60);
    cairo_line_to(cr, w * 0.14, h * 0.42);
    cairo_line_to(cr, w * 0.25, h * 0.56);
    cairo_line_to(cr, w * 0.39, h * 0.34);
    cairo_line_to(cr, w * 0.51, h * 0.57);
    cairo_line_to(cr, w * 0.64, h * 0.40);
    cairo_line_to(cr, w * 0.78, h * 0.58);
    cairo_line_to(cr, w, h * 0.48);
    cairo_line_to(cr, w, h);
    cairo_line_to(cr, 0.0, h);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_pattern_t *water = cairo_pattern_create_linear(0.0, h * 0.60, 0.0, h);
    cairo_pattern_add_color_stop_rgb(water, 0.0, 0.02, 0.18, 0.25);
    cairo_pattern_add_color_stop_rgb(water, 1.0, 0.01, 0.05, 0.09);
    cairo_set_source(cr, water);
    cairo_rectangle(cr, 0.0, h * 0.60, w, h * 0.40);
    cairo_fill(cr);
    cairo_pattern_destroy(water);

    cairo_set_source_rgba(cr, 1.0, 0.55, 0.08, 0.48);
    cairo_set_line_width(cr, 2.0);
    for (int i = 0; i < 6; ++i) {
        const double y = h * (0.68 + 0.045 * (double)i);
        const double spread = w * (0.025 + 0.018 * (double)i);
        cairo_move_to(cr, w * 0.72 - spread, y);
        cairo_line_to(cr, w * 0.72 + spread, y);
        cairo_stroke(cr);
    }

    cairo_set_source_rgb(cr, 0.01, 0.03, 0.04);
    cairo_set_line_width(cr, 7.0);
    cairo_move_to(cr, w * 0.90, h);
    cairo_curve_to(cr, w * 0.88, h * 0.78, w * 0.93, h * 0.58, w * 0.91, h * 0.30);
    cairo_stroke(cr);
    cairo_set_line_width(cr, 4.0);
    cairo_move_to(cr, w * 0.91, h * 0.46);
    cairo_line_to(cr, w * 0.83, h * 0.28);
    cairo_move_to(cr, w * 0.91, h * 0.40);
    cairo_line_to(cr, w * 0.97, h * 0.22);
    cairo_stroke(cr);
}

static GtkWidget *make_scenic_panel(int width, int height, const char *css_class)
{
    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), width);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), height);
    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(area), draw_scenic_panel, NULL, NULL);
    if (css_class != NULL) {
        gtk_widget_add_css_class(area, css_class);
    }
    return area;
}

static GtkWidget *make_visual_panel(const char *filename,
                                    int width,
                                    int height,
                                    const char *css_class)
{
    GtkWidget *picture = make_ui_asset_picture(
        filename, width, height, css_class);
    return picture != NULL
        ? picture
        : make_scenic_panel(width, height, css_class);
}

static void draw_australia_flag(GtkDrawingArea *area,
                                cairo_t *cr,
                                int width,
                                int height,
                                gpointer user_data)
{
    (void)area;
    (void)user_data;
    const double w = (double)width;
    const double h = (double)height;

    cairo_set_source_rgb(cr, 0.02, 0.14, 0.38);
    cairo_paint(cr);

    cairo_set_source_rgb(cr, 0.95, 0.95, 0.96);
    cairo_rectangle(cr, 0.0, 0.0, w * 0.46, h * 0.52);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.03, 0.16, 0.42);
    cairo_rectangle(cr, 0.0, 0.0, w * 0.46, h * 0.52);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.96, 0.96, 0.96);
    cairo_set_line_width(cr, h * 0.08);
    cairo_move_to(cr, 0.0, 0.0);
    cairo_line_to(cr, w * 0.46, h * 0.52);
    cairo_move_to(cr, w * 0.46, 0.0);
    cairo_line_to(cr, 0.0, h * 0.52);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.85, 0.05, 0.12);
    cairo_set_line_width(cr, h * 0.035);
    cairo_move_to(cr, 0.0, 0.0);
    cairo_line_to(cr, w * 0.46, h * 0.52);
    cairo_move_to(cr, w * 0.46, 0.0);
    cairo_line_to(cr, 0.0, h * 0.52);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.96, 0.96, 0.96);
    cairo_rectangle(cr, w * 0.19, 0.0, w * 0.08, h * 0.52);
    cairo_rectangle(cr, 0.0, h * 0.20, w * 0.46, h * 0.12);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.85, 0.05, 0.12);
    cairo_rectangle(cr, w * 0.215, 0.0, w * 0.03, h * 0.52);
    cairo_rectangle(cr, 0.0, h * 0.23, w * 0.46, h * 0.06);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    const double stars[][2] = {
        {0.68, 0.25}, {0.80, 0.48}, {0.64, 0.68},
        {0.86, 0.76}, {0.75, 0.87}, {0.35, 0.72}
    };
    for (guint i = 0; i < G_N_ELEMENTS(stars); ++i) {
        cairo_arc(cr, w * stars[i][0], h * stars[i][1], h * 0.035,
                  0.0, 6.283185307179586);
        cairo_fill(cr);
    }
}

static GtkWidget *make_australia_flag(void)
{
    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), 104);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), 66);
    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(area), draw_australia_flag, NULL, NULL);
    gtk_widget_add_css_class(area, "region-flag");
    return area;
}

static void draw_network_visual(GtkDrawingArea *area,
                                cairo_t *cr,
                                int width,
                                int height,
                                gpointer user_data)
{
    (void)area;
    const gboolean online = GPOINTER_TO_INT(user_data) != 0;
    const double w = (double)width;
    const double h = (double)height;
    const double nodes[][2] = {
        {0.14,0.62},{0.30,0.36},{0.46,0.58},{0.62,0.30},{0.78,0.52},{0.90,0.28}
    };

    cairo_set_source_rgb(cr, 0.01, 0.05, 0.09);
    cairo_paint(cr);
    cairo_set_line_width(cr, 1.5);
    cairo_set_source_rgba(cr, 0.0, 0.68, 0.95, online ? 0.50 : 0.18);
    for (guint i = 1; i < G_N_ELEMENTS(nodes); ++i) {
        cairo_move_to(cr, w * nodes[i-1][0], h * nodes[i-1][1]);
        cairo_line_to(cr, w * nodes[i][0], h * nodes[i][1]);
        cairo_stroke(cr);
    }
    for (guint i = 0; i < G_N_ELEMENTS(nodes); ++i) {
        cairo_set_source_rgb(
            cr,
            online ? 0.0 : 0.45,
            online ? 0.75 : 0.45,
            online ? 1.0 : 0.45);
        cairo_arc(cr, w * nodes[i][0], h * nodes[i][1], 4.0,
                  0.0, 6.283185307179586);
        cairo_fill(cr);
    }
}

static GtkWidget *make_network_visual(gboolean online)
{
    GtkWidget *area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), 280);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(area), 84);
    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(area),
        draw_network_visual,
        GINT_TO_POINTER(online ? 1 : 0),
        NULL);
    gtk_widget_add_css_class(area, "network-visual");
    return area;
}

static GtkWidget *make_theme_preview(const char *label,
                                     const char *class_name,
                                     gboolean selected)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    g_autofree gchar *asset_name =
        g_strdup_printf("%s.png", class_name);
    GtkWidget *preview = make_ui_asset_picture(
        asset_name, 112, 58, "theme-preview-window");

    if (preview == NULL) {
        preview = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(preview, "theme-preview-window");
        gtk_widget_add_css_class(preview, class_name);
        gtk_widget_set_size_request(preview, 112, 58);
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

static const char *locale_country_label(const char *locale_name)
{
    if (locale_name != NULL && strstr(locale_name, "_AU") != NULL) {
        return "Australia";
    }
    return locale_name != NULL ? locale_name : "Unknown";
}

static const char *locale_language_label(const char *locale_name)
{
    if (locale_name != NULL && g_str_has_prefix(locale_name, "en_AU")) {
        return "English (Australia)";
    }
    return locale_name != NULL ? locale_name : "Unknown";
}

static GtkWidget *build_home_page(GtkStack *stack)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *hero = gtk_overlay_new();
    GtkWidget *hero_scene = make_visual_panel(
        "hero-asset.png", 1060, 242, "hero-scene");
    GtkWidget *hero_foreground = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    GtkWidget *hero_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *hero_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *hero_brand = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *hero_icon = gtk_image_new_from_icon_name(
        "video-display-symbolic");
    GtkWidget *features = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *grid = gtk_grid_new();
    GtkWidget *overview = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *overview_heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *overview_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *overview_icon = gtk_image_new_from_icon_name(
        "video-display-symbolic");
    GtkWidget *overview_data = gtk_grid_new();
    GtkWidget *overview_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    GtkWidget *system_time_label;
    GtkWidget *overview_scene = make_visual_panel(
        "overview-asset.png", 190, 126, "overview-scene");
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
    SsHomeTemporalPresentation temporal = {0};
    g_autofree gchar *region_detail = NULL;
    g_autofree gchar *appearance_detail = NULL;
    g_autofree gchar *network_detail = NULL;
    g_autofree gchar *uptime_text = format_uptime();
    g_autoptr(GTimeZone) local_zone = g_time_zone_new_local();
    const char *timezone_name =
        local_zone != NULL ? g_time_zone_get_identifier(local_zone) : "Unknown";
    SsLocationMetadata location = {0};
    const gboolean have_location =
        ss_location_metadata_load(&location) ? TRUE : FALSE;
    g_autofree gchar *coordinate_text = have_location
        ? g_strdup_printf("%.4f° %c, %.4f° %c",
                          fabs(location.latitude),
                          location.latitude < 0.0 ? 'S' : 'N',
                          fabs(location.longitude),
                          location.longitude < 0.0 ? 'W' : 'E')
        : NULL;
    g_autofree gchar *os_display = g_strdup_printf(
        "Infiltrator OS (%s)",
        os_name != NULL ? os_name : "Linux");
    if (!ss_home_temporal_presentation_now(&temporal)) {
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
        "%s  •  Locale %s%s",
        locale_language_label(locale_name),
        locale_name,
        strstr(locale_name, "_AU") != NULL
            ? "  •  Currency AUD  •  Units Metric"
            : "");
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
    g_signal_connect(
        updates_button, "clicked",
        G_CALLBACK(launch_external_program), NULL);
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
    append_overview_row(
        GTK_GRID(overview_data), 5, "Uptime", uptime_text);
    system_time_label = append_overview_row(
        GTK_GRID(overview_data), 6, "System time", temporal.system_time_text);
    gtk_box_append(GTK_BOX(overview_body), overview_scene);
    gtk_widget_set_hexpand(overview_data, TRUE);
    gtk_box_append(GTK_BOX(overview_body), overview_data);
    gtk_box_append(GTK_BOX(overview), overview_body);
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
    gtk_widget_add_css_class(date_action, "quick-action-cyan");
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
    gtk_widget_add_css_class(region_action, "quick-action-cyan");
    gtk_grid_attach(GTK_GRID(quick_grid), region_action, 1, 0, 1, 1);

    GtkWidget *display_action = make_program_action(
        "video-display-symbolic",
        "Configure Display",
        "Scaling, layout & monitors",
        "cinnamon-settings",
        "display");
    gtk_widget_add_css_class(display_action, "quick-action-cyan");
    gtk_grid_attach(GTK_GRID(quick_grid), display_action, 0, 1, 1, 1);

    GtkWidget *software_action = make_program_action(
        "system-software-install-symbolic",
        "Software & Updates",
        "Apps, packages & updates",
        "infiltrator-software",
        NULL);
    gtk_widget_add_css_class(software_action, "quick-action-gold");
    gtk_grid_attach(GTK_GRID(quick_grid), software_action, 1, 1, 1, 1);

    gtk_box_append(GTK_BOX(quick), quick_grid);
    gtk_grid_attach(GTK_GRID(grid), quick, 1, 0, 1, 1);
    gtk_box_append(GTK_BOX(page), grid);

    gtk_widget_add_css_class(status_grid, "status-grid");
    gtk_grid_set_column_spacing(GTK_GRID(status_grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(status_grid), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(status_grid), TRUE);

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
    gtk_widget_add_css_class(date_open, "card-arrow");
    gtk_widget_set_halign(date_open, GTK_ALIGN_END);
    gtk_widget_set_hexpand(date_open, TRUE);
    g_signal_connect(
        date_open, "clicked",
        G_CALLBACK(open_date_time), stack);
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
    gtk_box_append(
        GTK_BOX(date_location),
        ss_linux_ui_make_label(
            have_location ? location.display_name : timezone_name,
            "location-primary"));
    gtk_box_append(
        GTK_BOX(date_location),
        ss_linux_ui_make_label(
            have_location && location.country_code[0] != '\0'
                ? (g_strcmp0(location.country_code, "AU") == 0 ? "Australia" : location.country_code)
                : timezone_name,
            "status-card-detail"));
    if (coordinate_text != NULL) {
        gtk_box_append(
            GTK_BOX(date_location),
            ss_linux_ui_make_label(coordinate_text, "location-coordinates"));
    }
    gtk_box_append(GTK_BOX(date_meta), date_location);
    gtk_box_append(GTK_BOX(date_body), date_meta);

    GtkWidget *date_scene = make_visual_panel(
        "date-asset.png", 190, 96, "date-scene");
    gtk_widget_set_hexpand(date_scene, TRUE);
    gtk_widget_set_halign(date_scene, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(date_body), date_scene);
    gtk_box_append(GTK_BOX(date_card), date_body);
    gtk_grid_attach(GTK_GRID(status_grid), date_card, 0, 0, 1, 1);

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
    gtk_box_append(GTK_BOX(region_body), make_australia_flag());
    region_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(
        GTK_BOX(region_copy),
        ss_linux_ui_make_label(
            locale_country_label(locale_name), "region-country"));
    gtk_box_append(
        GTK_BOX(region_copy),
        ss_linux_ui_make_label(region_detail, "status-card-detail"));
    gtk_box_append(GTK_BOX(region_body), region_copy);
    GtkWidget *region_scene = make_ui_asset_picture(
        "region-asset.png", 220, 96, "region-scene");
    if (region_scene != NULL) {
        gtk_widget_set_hexpand(region_scene, TRUE);
        gtk_widget_set_halign(region_scene, GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(region_body), region_scene);
    }
    gtk_box_append(GTK_BOX(region_card), region_body);
    gtk_grid_attach(GTK_GRID(status_grid), region_card, 1, 0, 1, 1);

    appearance_card = make_status_card(
        "appearance-status-card",
        "preferences-desktop-theme-symbolic",
        "Display & Appearance",
        theme_name != NULL ? theme_name : "System theme",
        appearance_detail);
    appearance_previews = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    gtk_box_set_homogeneous(GTK_BOX(appearance_previews), TRUE);
    gtk_box_append(
        GTK_BOX(appearance_previews),
        make_theme_preview("Light", "theme-light", !prefer_dark));
    gtk_box_append(
        GTK_BOX(appearance_previews),
        make_theme_preview("Dark", "theme-dark", prefer_dark));
    gtk_box_append(
        GTK_BOX(appearance_previews),
        make_theme_preview("Follow OS", "theme-follow", FALSE));
    gtk_box_append(
        GTK_BOX(appearance_previews),
        make_theme_preview("Mercedes Grey", "theme-mercedes", FALSE));
    gtk_box_append(GTK_BOX(appearance_card), appearance_previews);
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
    network_body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *network_visual = make_ui_asset_picture(
        "network-asset.png", 300, 140, "network-visual");
    if (network_visual == NULL) {
        network_visual = make_network_visual(online);
    }
    gtk_widget_set_hexpand(network_visual, TRUE);
    gtk_widget_set_halign(network_visual, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(network_body), network_visual);
    gtk_box_append(GTK_BOX(network_card), network_body);
    gtk_grid_attach(GTK_GRID(status_grid), network_card, 1, 1, 1, 1);

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
    ticker->clock = GTK_LABEL(date_clock_label);
    ticker->date = GTK_LABEL(date_calendar_label);
    ticker->system_time = GTK_LABEL(system_time_label);
    const guint temporal_source_id = g_timeout_add_full(
        G_PRIORITY_DEFAULT,
        250U,
        refresh_home_temporal,
        ticker,
        g_free);
    g_source_set_name_by_id(
        temporal_source_id,
        "[system-settings] live Home temporal presentation");
    g_object_set_data_full(
        G_OBJECT(scroller),
        "system-settings-home-temporal-source",
        GUINT_TO_POINTER(temporal_source_id),
        remove_home_temporal_source);

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
        build_home_page(stack),
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
