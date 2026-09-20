// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main.c
 * @brief Native Linux System Settings shell with the Date & Time panel.
 */

#include "system-settings/date-time-model.h"
#include "system-settings/temporal-policy-store.h"

#include <gtk/gtk.h>
#include <infiltratr/design.h>
#include <infiltratr/temporal.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct SettingsWindow {
    GtkWindow *window;
    GtkWidget *clock_preview;
    GtkWidget *date_preview;
    GtkWidget *timezone_value;
    GtkDropDown *clock_profile;
    GtkSwitch *show_seconds;
    GtkWidget *status_label;
    SsDateTimeModel model;
    guint timer_id;
    bool updating_controls;
} SettingsWindow;

static const char *const profile_labels[] = {
    "Follow system",
    "12-hour time",
    "24-hour time",
    "Decimal time (10-hour day)",
    NULL
};

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
        gchar *folded = g_ascii_strdown(theme_name, -1);
        dark = folded != NULL && strstr(folded, "dark") != NULL;
        g_free(folded);
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
    gchar *border;
    gchar *text;
    gchar *title;
    gchar *muted;
    gchar *accent;
    gchar *selected;
    gchar *titlebar;

    if (palette == NULL || metrics == NULL || type == NULL) {
        return;
    }

    background = rgb_css(palette->background_rgb);
    panel = rgb_css(palette->panel_rgb);
    card = rgb_css(palette->card_rgb);
    surface = rgb_css(palette->surface_rgb);
    border = rgb_css(palette->border_rgb);
    text = rgb_css(palette->text_rgb);
    title = rgb_css(palette->title_rgb);
    muted = rgb_css(palette->muted_rgb);
    accent = rgb_css(palette->neutral_accent_rgb);
    selected = rgb_css(palette->selection_background_rgb);
    titlebar = rgb_css(palette->titlebar_rgb);

    css = g_string_new(NULL);
    g_string_append_printf(
        css,
        "window { background: %s; color: %s; font-family: '%s', %s; }\n"
        ".titlebar-shell { background: %s; border-bottom: 1px solid %s; padding: 10px 16px; }\n"
        ".app-title { color: %s; font-size: 18px; font-weight: 700; }\n"
        ".app-subtitle { color: %s; font-size: 12px; }\n"
        ".settings-sidebar { background: %s; border-right: 1px solid %s; padding: 14px 10px; }\n"
        ".nav-title { color: %s; font-size: 11px; font-weight: 700; letter-spacing: 0.08em; margin: 4px 8px 8px 8px; }\n"
        ".nav-row { border-radius: %upx; padding: 10px 12px; }\n"
        ".nav-row:selected { background: %s; }\n"
        ".settings-content { padding: %upx; }\n"
        ".page-title { color: %s; font-size: 26px; font-weight: 700; }\n"
        ".page-summary { color: %s; font-size: 13px; margin-bottom: 6px; }\n"
        ".preview-card, .settings-card { background: %s; border: 1px solid %s; border-radius: %upx; padding: 18px; }\n"
        ".preview-time { color: %s; font-size: 36px; font-weight: 700; }\n"
        ".preview-date { color: %s; font-size: 14px; }\n"
        ".section-title { color: %s; font-size: 16px; font-weight: 700; }\n"
        ".setting-label { color: %s; font-weight: 700; }\n"
        ".setting-description { color: %s; font-size: 12px; }\n"
        ".setting-row { padding: 8px 0; }\n"
        ".divider { background: %s; min-height: 1px; }\n"
        ".accent-note { color: %s; font-size: 12px; }\n"
        ".status-ok { color: %s; font-size: 12px; }\n"
        "dropdown, switch { background: %s; }\n",
        background, text, type->ui_family, type->gtk_fallback,
        titlebar, border,
        title, muted,
        panel, border,
        muted,
        metrics->control_radius, selected,
        metrics->screen_padding,
        title, muted,
        card, border, metrics->card_radius,
        title, muted,
        title,
        text, muted,
        border,
        accent,
        palette->success_rgb == 0U ? accent : "#63ab7c",
        surface);

    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css->str, -1);
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
    g_free(border);
    g_free(text);
    g_free(title);
    g_free(muted);
    g_free(accent);
    g_free(selected);
    g_free(titlebar);
}

static GtkWidget *make_label(const char *text, const char *css_class)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    if (css_class != NULL) {
        gtk_widget_add_css_class(label, css_class);
    }
    return label;
}

static GtkWidget *make_setting_identity(const char *title,
                                        const char *description)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *heading = make_label(title, "setting-label");
    GtkWidget *copy = make_label(description, "setting-description");

    gtk_label_set_wrap(GTK_LABEL(copy), TRUE);
    gtk_widget_set_hexpand(copy, TRUE);
    gtk_box_append(GTK_BOX(box), heading);
    gtk_box_append(GTK_BOX(box), copy);
    return box;
}

static void set_status(SettingsWindow *state,
                       const char *message,
                       bool error)
{
    gtk_label_set_text(GTK_LABEL(state->status_label), message);
    gtk_widget_remove_css_class(state->status_label, "status-ok");
    gtk_widget_remove_css_class(state->status_label, "error");
    gtk_widget_add_css_class(state->status_label,
                             error ? "error" : "status-ok");
}

static bool format_preview(const SettingsWindow *state,
                           char *buffer,
                           size_t capacity)
{
    const InfiltratrTemporalPolicy *policy =
        ss_date_time_model_policy(&state->model);
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    int64_t unix_us;
    gint64 offset_us;

    if (policy == NULL || now == NULL || buffer == NULL || capacity == 0U) {
        return false;
    }

    if (policy->clock_profile == INFILTRATR_CLOCK_PROFILE_SYSTEM) {
        g_autofree gchar *formatted =
            g_date_time_format(now, policy->show_seconds ? "%X" : "%H:%M");
        if (formatted == NULL ||
            g_strlcpy(buffer, formatted, capacity) >= capacity) {
            return false;
        }
        return true;
    }

    unix_us = g_get_real_time();
    offset_us = g_date_time_get_utc_offset(now);
    return infiltratr_temporal_format_clock(
        policy->clock_profile,
        unix_us,
        (int32_t)(offset_us / G_USEC_PER_SEC),
        policy->show_seconds,
        buffer,
        capacity,
        NULL);
}

static gboolean refresh_preview(gpointer user_data)
{
    SettingsWindow *state = user_data;
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    char clock_text[64];

    if (state == NULL || state->clock_preview == NULL || now == NULL) {
        return G_SOURCE_CONTINUE;
    }

    if (format_preview(state, clock_text, sizeof(clock_text))) {
        gtk_label_set_text(GTK_LABEL(state->clock_preview), clock_text);
    }

    {
        g_autofree gchar *date = g_date_time_format(now, "%A, %e %B %Y");
        const gchar *zone = g_date_time_get_timezone_abbreviation(now);
        if (date != NULL) {
            gtk_label_set_text(GTK_LABEL(state->date_preview), date);
        }
        gtk_label_set_text(GTK_LABEL(state->timezone_value),
                           zone != NULL ? zone : "Local time");
    }

    return G_SOURCE_CONTINUE;
}

static guint profile_index(InfiltratrClockProfile profile)
{
    switch (profile) {
    case INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12:
        return 1U;
    case INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24:
        return 2U;
    case INFILTRATR_CLOCK_PROFILE_DECIMAL_10:
        return 3U;
    case INFILTRATR_CLOCK_PROFILE_SYSTEM:
    default:
        return 0U;
    }
}

static InfiltratrClockProfile profile_from_index(guint index)
{
    switch (index) {
    case 1U:
        return INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12;
    case 2U:
        return INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24;
    case 3U:
        return INFILTRATR_CLOCK_PROFILE_DECIMAL_10;
    case 0U:
    default:
        return INFILTRATR_CLOCK_PROFILE_SYSTEM;
    }
}

static void sync_controls(SettingsWindow *state)
{
    const InfiltratrTemporalPolicy *policy =
        ss_date_time_model_policy(&state->model);

    if (policy == NULL) {
        return;
    }

    state->updating_controls = true;
    gtk_drop_down_set_selected(state->clock_profile,
                               profile_index(policy->clock_profile));
    gtk_switch_set_active(state->show_seconds, policy->show_seconds);
    state->updating_controls = false;
    (void)refresh_preview(state);
}

static void on_profile_changed(GObject *object,
                               GParamSpec *pspec,
                               gpointer user_data)
{
    SettingsWindow *state = user_data;
    guint selected;
    InfiltratrClockProfile profile;

    (void)pspec;
    if (state == NULL || state->updating_controls) {
        return;
    }

    selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    profile = profile_from_index(selected);
    if (!ss_date_time_model_set_clock_profile(&state->model, profile)) {
        set_status(state, "Could not save the clock-system setting.", true);
        sync_controls(state);
        return;
    }

    set_status(state, "Clock system saved. Calendar will follow this setting.",
               false);
    (void)refresh_preview(state);
}

static void on_seconds_changed(GObject *object,
                               GParamSpec *pspec,
                               gpointer user_data)
{
    SettingsWindow *state = user_data;
    gboolean active;

    (void)pspec;
    if (state == NULL || state->updating_controls) {
        return;
    }

    active = gtk_switch_get_active(GTK_SWITCH(object));
    if (!ss_date_time_model_set_show_seconds(&state->model,
                                             active != FALSE)) {
        set_status(state, "Could not save the seconds setting.", true);
        sync_controls(state);
        return;
    }

    set_status(state, "Seconds preference saved.", false);
    (void)refresh_preview(state);
}

static GtkWidget *build_date_time_panel(SettingsWindow *state)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkWidget *title = make_label("Date & Time", "page-title");
    GtkWidget *summary = make_label(
        "Choose how time is presented across Common-aware applications.",
        "page-summary");
    GtkWidget *preview_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *settings_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *clock_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    GtkWidget *seconds_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    GtkWidget *timezone_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    GtkStringList *profiles;

    gtk_widget_add_css_class(page, "settings-content");
    gtk_box_append(GTK_BOX(page), title);
    gtk_label_set_wrap(GTK_LABEL(summary), TRUE);
    gtk_box_append(GTK_BOX(page), summary);

    gtk_widget_add_css_class(preview_card, "preview-card");
    state->clock_preview = make_label("--:--", "preview-time");
    state->date_preview = make_label("", "preview-date");
    gtk_box_append(GTK_BOX(preview_card), state->clock_preview);
    gtk_box_append(GTK_BOX(preview_card), state->date_preview);
    gtk_box_append(GTK_BOX(page), preview_card);

    gtk_widget_add_css_class(settings_card, "settings-card");
    gtk_box_append(GTK_BOX(settings_card),
                   make_label("Clock presentation", "section-title"));

    gtk_widget_add_css_class(clock_row, "setting-row");
    gtk_widget_set_hexpand(clock_row, TRUE);
    gtk_box_append(GTK_BOX(clock_row),
                   make_setting_identity(
                       "Clock system",
                       "Follow the operating system, force conventional 12/24-hour time, or use a 10-hour decimal day."));
    profiles = gtk_string_list_new(profile_labels);
    state->clock_profile = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(profiles), NULL));
    g_object_unref(profiles);
    gtk_widget_set_valign(GTK_WIDGET(state->clock_profile), GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(GTK_WIDGET(state->clock_profile), 230, -1);
    gtk_box_append(GTK_BOX(clock_row), GTK_WIDGET(state->clock_profile));
    gtk_box_append(GTK_BOX(settings_card), clock_row);

    gtk_box_append(GTK_BOX(settings_card),
                   make_label("", "divider"));

    gtk_widget_add_css_class(seconds_row, "setting-row");
    gtk_widget_set_hexpand(seconds_row, TRUE);
    gtk_box_append(GTK_BOX(seconds_row),
                   make_setting_identity(
                       "Show seconds",
                       "Display seconds, or the nearest finer unit supported by the selected clock system."));
    state->show_seconds = GTK_SWITCH(gtk_switch_new());
    gtk_widget_set_valign(GTK_WIDGET(state->show_seconds), GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(seconds_row), GTK_WIDGET(state->show_seconds));
    gtk_box_append(GTK_BOX(settings_card), seconds_row);

    gtk_box_append(GTK_BOX(settings_card),
                   make_label("", "divider"));

    gtk_widget_add_css_class(timezone_row, "setting-row");
    gtk_box_append(GTK_BOX(timezone_row),
                   make_setting_identity(
                       "Time zone",
                       "The current operating-system time zone. Time-zone editing will use the native system service in the protected system settings phase."));
    state->timezone_value = make_label("Local time", "accent-note");
    gtk_widget_set_valign(state->timezone_value, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(state->timezone_value, GTK_ALIGN_END);
    gtk_widget_set_hexpand(state->timezone_value, TRUE);
    gtk_box_append(GTK_BOX(timezone_row), state->timezone_value);
    gtk_box_append(GTK_BOX(settings_card), timezone_row);

    state->status_label = make_label("", "status-ok");
    gtk_label_set_wrap(GTK_LABEL(state->status_label), TRUE);
    gtk_box_append(GTK_BOX(settings_card), state->status_label);

    gtk_box_append(GTK_BOX(page), settings_card);

    g_signal_connect(state->clock_profile, "notify::selected",
                     G_CALLBACK(on_profile_changed), state);
    g_signal_connect(state->show_seconds, "notify::active",
                     G_CALLBACK(on_seconds_changed), state);

    return page;
}

static GtkWidget *build_sidebar(void)
{
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *label = make_label("SYSTEM", "nav-title");
    GtkWidget *list = gtk_list_box_new();
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name("preferences-system-time-symbolic");
    GtkWidget *row_label = make_label("Date & Time", NULL);

    gtk_widget_set_size_request(sidebar, 210, -1);
    gtk_widget_add_css_class(sidebar, "settings-sidebar");
    gtk_box_append(GTK_BOX(sidebar), label);

    gtk_image_set_pixel_size(GTK_IMAGE(icon), 18);
    gtk_box_append(GTK_BOX(row_box), icon);
    gtk_box_append(GTK_BOX(row_box), row_label);
    gtk_widget_add_css_class(row, "nav-row");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
    gtk_list_box_append(GTK_LIST_BOX(list), row);
    gtk_list_box_select_row(GTK_LIST_BOX(list), GTK_LIST_BOX_ROW(row));
    gtk_selection_model_selection_changed(
        GTK_SELECTION_MODEL(gtk_single_selection_new(NULL)), 0U, 0U);
    gtk_box_append(GTK_BOX(sidebar), list);

    return sidebar;
}

static GtkWidget *build_header(void)
{
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *icon = gtk_image_new_from_icon_name(
        "preferences-system-symbolic");
    GtkWidget *identity = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *title = make_label("System Settings", "app-title");
    GtkWidget *subtitle = make_label(
        "One place for system-wide preferences", "app-subtitle");

    gtk_widget_add_css_class(header, "titlebar-shell");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_box_append(GTK_BOX(header), icon);
    gtk_box_append(GTK_BOX(identity), title);
    gtk_box_append(GTK_BOX(identity), subtitle);
    gtk_box_append(GTK_BOX(header), identity);
    return header;
}

static void on_activate(GtkApplication *application, gpointer user_data)
{
    SettingsWindow *state = g_new0(SettingsWindow, 1);
    GtkWidget *root;
    GtkWidget *body;
    GtkWidget *panel;
    GtkWidget *scroller;

    (void)user_data;

    if (!ss_date_time_model_init(&state->model,
                                 ss_platform_temporal_policy_store())) {
        g_printerr("Unable to initialise Date & Time settings.\n");
        g_free(state);
        return;
    }

    install_common_theme();

    state->window = GTK_WINDOW(gtk_application_window_new(application));
    gtk_window_set_title(state->window, "System Settings");
    gtk_window_set_default_size(state->window, 900, 620);
    gtk_window_set_resizable(state->window, TRUE);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(state->window, root);
    gtk_box_append(GTK_BOX(root), build_header());

    body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(body, TRUE);
    gtk_box_append(GTK_BOX(root), body);
    gtk_box_append(GTK_BOX(body), build_sidebar());

    panel = build_date_time_panel(state);
    scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), panel);
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_box_append(GTK_BOX(body), scroller);

    sync_controls(state);
    set_status(state,
               state->model.persisted_policy_present
                   ? "Using the saved system-wide temporal policy."
                   : "No custom temporal policy is saved yet; following the operating system.",
               false);

    state->timer_id = g_timeout_add_seconds(1U, refresh_preview, state);
    g_object_set_data_full(G_OBJECT(state->window),
                           "system-settings-state",
                           state,
                           g_free);
    gtk_window_present(state->window);
}

int main(int argc, char **argv)
{
    g_autoptr(GtkApplication) application =
        gtk_application_new("org.infiltrator.SystemSettings",
                            G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(application, "activate", G_CALLBACK(on_activate), NULL);
    return g_application_run(G_APPLICATION(application), argc, argv);
}
