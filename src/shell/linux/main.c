// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main.c
 * @brief Native Linux System Settings shell with complete Date & Time policy.
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
    GtkDropDown *clock_mode;
    GtkDropDown *primary_calendar;
    GtkDropDown *secondary_calendar;
    GtkSwitch *show_seconds;
    GtkSwitch *location_configured;
    GtkSpinButton *latitude;
    GtkSpinButton *longitude;
    GtkWidget *status_label;
    SsDateTimeModel model;
    guint timer_id;
    bool updating_controls;
} SettingsWindow;

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
        ".preview-time { color: %s; font-size: 30px; font-weight: 700; }\n"
        ".preview-date { color: %s; font-size: 14px; }\n"
        ".section-title { color: %s; font-size: 16px; font-weight: 700; }\n"
        ".setting-label { color: %s; font-weight: 700; }\n"
        ".setting-description { color: %s; font-size: 12px; }\n"
        ".setting-row { padding: 8px 0; }\n"
        ".divider { background: %s; min-height: 1px; }\n"
        ".accent-note { color: %s; font-size: 12px; }\n"
        ".status-ok { color: %s; font-size: 12px; }\n"
        "dropdown, switch, spinbutton { background: %s; }\n",
        background, text, type->ui_family, type->gtk_fallback,
        titlebar, border, title, muted, panel, border, muted,
        metrics->control_radius, selected, metrics->screen_padding,
        title, muted, card, border, metrics->card_radius, title, muted,
        title, text, muted, border, accent,
        palette->success_rgb == 0U ? accent : "#63ab7c", surface);

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

static GtkWidget *make_setting_row(const char *title,
                                   const char *description,
                                   GtkWidget *control)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);

    gtk_widget_add_css_class(row, "setting-row");
    gtk_widget_set_hexpand(row, TRUE);
    gtk_box_append(GTK_BOX(row), make_setting_identity(title, description));
    gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row), control);
    return row;
}

static void set_status(SettingsWindow *state,
                       const char *message,
                       bool error)
{
    if (state == NULL || state->status_label == NULL) {
        return;
    }
    gtk_label_set_text(GTK_LABEL(state->status_label), message);
    gtk_widget_remove_css_class(state->status_label, "status-ok");
    gtk_widget_remove_css_class(state->status_label, "error");
    gtk_widget_add_css_class(state->status_label,
                             error ? "error" : "status-ok");
}

static GtkStringList *clock_mode_strings(void)
{
    GtkStringList *list = gtk_string_list_new(NULL);
    size_t index;

    for (index = 0U; index < infiltratr_temporal_clock_mode_count(); ++index) {
        const InfiltratrTemporalClockModeInfo *info =
            infiltratr_temporal_clock_mode_at(index);
        if (info != NULL) {
            gtk_string_list_append(list, info->name);
        }
    }
    return list;
}

static GtkStringList *calendar_strings(bool include_none)
{
    GtkStringList *list = gtk_string_list_new(NULL);
    size_t index = include_none ? 0U : 1U;

    for (; index < infiltratr_temporal_calendar_count(); ++index) {
        const InfiltratrTemporalCalendarInfo *info =
            infiltratr_temporal_calendar_at(index);
        if (info != NULL) {
            gtk_string_list_append(list, info->name);
        }
    }
    return list;
}

static guint clock_index_for_id(const char *id)
{
    size_t index;
    for (index = 0U; index < infiltratr_temporal_clock_mode_count(); ++index) {
        const InfiltratrTemporalClockModeInfo *info =
            infiltratr_temporal_clock_mode_at(index);
        if (info != NULL && strcmp(info->id, id) == 0) {
            return (guint)index;
        }
    }
    return 0U;
}

static guint calendar_index_for_id(const char *id, bool include_none)
{
    size_t index = include_none ? 0U : 1U;
    guint visible = 0U;

    for (; index < infiltratr_temporal_calendar_count(); ++index, ++visible) {
        const InfiltratrTemporalCalendarInfo *info =
            infiltratr_temporal_calendar_at(index);
        if (info != NULL && strcmp(info->id, id) == 0) {
            return visible;
        }
    }
    return 0U;
}

static const InfiltratrTemporalClockModeInfo *
selected_clock_mode(const SettingsWindow *state)
{
    guint selected;
    if (state == NULL || state->clock_mode == NULL) {
        return NULL;
    }
    selected = gtk_drop_down_get_selected(state->clock_mode);
    return infiltratr_temporal_clock_mode_at((size_t)selected);
}

static const InfiltratrTemporalCalendarInfo *
selected_calendar(GtkDropDown *dropdown, bool include_none)
{
    size_t index;
    if (dropdown == NULL) {
        return NULL;
    }
    index = (size_t)gtk_drop_down_get_selected(dropdown);
    if (!include_none) {
        ++index;
    }
    return infiltratr_temporal_calendar_at(index);
}

static void update_control_capabilities(SettingsWindow *state)
{
    const InfiltratrTemporalClockModeInfo *mode;
    bool location_enabled;

    if (state == NULL) {
        return;
    }

    mode = selected_clock_mode(state);
    gtk_widget_set_sensitive(GTK_WIDGET(state->show_seconds),
                             mode == NULL || mode->supports_seconds);

    location_enabled =
        gtk_switch_get_active(state->location_configured) != FALSE;
    gtk_widget_set_sensitive(GTK_WIDGET(state->latitude), location_enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(state->longitude), location_enabled);

    if (mode != NULL &&
        (mode->requires_latitude || mode->requires_longitude) &&
        !location_enabled) {
        set_status(state,
                   "This clock system needs a geographic location for a meaningful result.",
                   false);
    }
}

static bool format_preview(const SettingsWindow *state,
                           char *buffer,
                           size_t capacity)
{
    const InfiltratrTemporalPolicyV2 *policy =
        ss_date_time_model_policy(&state->model);
    const InfiltratrTemporalClockModeInfo *mode;
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    int64_t unix_us;
    gint64 offset_us;
    InfiltratrClockProfile conventional;

    if (policy == NULL || now == NULL || buffer == NULL || capacity == 0U) {
        return false;
    }

    if (strcmp(policy->clock_mode, "standard") == 0) {
        g_autofree gchar *formatted =
            g_date_time_format(now, policy->show_seconds ? "%X" : "%H:%M");
        return formatted != NULL &&
               g_strlcpy(buffer, formatted, capacity) < capacity;
    }

    if (strcmp(policy->clock_mode, "standard-12") == 0) {
        conventional = INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12;
    } else if (strcmp(policy->clock_mode, "standard-24") == 0) {
        conventional = INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24;
    } else if (strcmp(policy->clock_mode, "decimal") == 0) {
        conventional = INFILTRATR_CLOCK_PROFILE_DECIMAL_10;
    } else {
        mode = infiltratr_temporal_clock_mode_find(policy->clock_mode);
        return mode != NULL &&
               g_strlcpy(buffer, mode->name, capacity) < capacity;
    }

    unix_us = g_get_real_time();
    offset_us = g_date_time_get_utc_offset(now);
    return infiltratr_temporal_format_clock(
        conventional, unix_us,
        (int32_t)(offset_us / G_USEC_PER_SEC),
        policy->show_seconds, buffer, capacity, NULL);
}

static gboolean refresh_preview(gpointer user_data)
{
    SettingsWindow *state = user_data;
    const InfiltratrTemporalPolicyV2 *policy;
    const InfiltratrTemporalCalendarInfo *primary;
    const InfiltratrTemporalCalendarInfo *secondary;
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    char clock_text[160];

    if (state == NULL || state->clock_preview == NULL || now == NULL) {
        return G_SOURCE_CONTINUE;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL) {
        return G_SOURCE_CONTINUE;
    }

    if (format_preview(state, clock_text, sizeof(clock_text))) {
        gtk_label_set_text(GTK_LABEL(state->clock_preview), clock_text);
    }

    primary = infiltratr_temporal_calendar_find(policy->primary_calendar);
    secondary = infiltratr_temporal_calendar_find(policy->secondary_calendar);
    {
        g_autofree gchar *date = g_date_time_format(now, "%A, %e %B %Y");
        g_autofree gchar *summary = NULL;
        const gchar *zone = g_date_time_get_timezone_abbreviation(now);

        if (primary != NULL && secondary != NULL &&
            strcmp(secondary->id, "none") != 0) {
            summary = g_strdup_printf(
                "%s • secondary: %s • %s",
                primary->name, secondary->name,
                date != NULL ? date : "");
        } else if (primary != NULL) {
            summary = g_strdup_printf(
                "%s • %s", primary->name, date != NULL ? date : "");
        }
        if (summary != NULL) {
            gtk_label_set_text(GTK_LABEL(state->date_preview), summary);
        }
        gtk_label_set_text(GTK_LABEL(state->timezone_value),
                           zone != NULL ? zone : "Local time");
    }

    return G_SOURCE_CONTINUE;
}

static void sync_controls(SettingsWindow *state)
{
    const InfiltratrTemporalPolicyV2 *policy =
        ss_date_time_model_policy(&state->model);

    if (policy == NULL) {
        return;
    }

    state->updating_controls = true;
    gtk_drop_down_set_selected(
        state->clock_mode, clock_index_for_id(policy->clock_mode));
    gtk_drop_down_set_selected(
        state->primary_calendar,
        calendar_index_for_id(policy->primary_calendar, false));
    gtk_drop_down_set_selected(
        state->secondary_calendar,
        calendar_index_for_id(policy->secondary_calendar, true));
    gtk_switch_set_active(state->show_seconds, policy->show_seconds);
    gtk_switch_set_active(state->location_configured,
                          policy->location_configured);
    gtk_spin_button_set_value(state->latitude, policy->latitude);
    gtk_spin_button_set_value(state->longitude, policy->longitude);
    state->updating_controls = false;
    update_control_capabilities(state);
    (void)refresh_preview(state);
}

static void policy_saved(SettingsWindow *state)
{
    set_status(state,
               "System temporal policy saved. Calendar follows it when “Follow System Settings” is enabled.",
               false);
    update_control_capabilities(state);
    (void)refresh_preview(state);
}

static void on_clock_changed(GObject *object,
                             GParamSpec *pspec,
                             gpointer user_data)
{
    SettingsWindow *state = user_data;
    const InfiltratrTemporalClockModeInfo *mode;

    (void)object;
    (void)pspec;
    if (state == NULL || state->updating_controls) {
        return;
    }

    mode = selected_clock_mode(state);
    if (mode == NULL ||
        !ss_date_time_model_set_clock_mode(&state->model, mode->id)) {
        set_status(state, "Could not save the clock system.", true);
        sync_controls(state);
        return;
    }
    policy_saved(state);
}

static void on_primary_calendar_changed(GObject *object,
                                        GParamSpec *pspec,
                                        gpointer user_data)
{
    SettingsWindow *state = user_data;
    const InfiltratrTemporalCalendarInfo *calendar;

    (void)object;
    (void)pspec;
    if (state == NULL || state->updating_controls) {
        return;
    }

    calendar = selected_calendar(state->primary_calendar, false);
    if (calendar == NULL ||
        !ss_date_time_model_set_primary_calendar(
            &state->model, calendar->id)) {
        set_status(state, "Could not save the primary calendar.", true);
        sync_controls(state);
        return;
    }
    policy_saved(state);
}

static void on_secondary_calendar_changed(GObject *object,
                                          GParamSpec *pspec,
                                          gpointer user_data)
{
    SettingsWindow *state = user_data;
    const InfiltratrTemporalCalendarInfo *calendar;

    (void)object;
    (void)pspec;
    if (state == NULL || state->updating_controls) {
        return;
    }

    calendar = selected_calendar(state->secondary_calendar, true);
    if (calendar == NULL ||
        !ss_date_time_model_set_secondary_calendar(
            &state->model, calendar->id)) {
        set_status(state, "Could not save the secondary calendar.", true);
        sync_controls(state);
        return;
    }
    policy_saved(state);
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
    policy_saved(state);
}

static void on_location_changed(GObject *object,
                                GParamSpec *pspec,
                                gpointer user_data)
{
    SettingsWindow *state = user_data;
    bool configured;
    double latitude;
    double longitude;

    (void)object;
    (void)pspec;
    if (state == NULL || state->updating_controls) {
        return;
    }

    configured =
        gtk_switch_get_active(state->location_configured) != FALSE;
    latitude = gtk_spin_button_get_value(state->latitude);
    longitude = gtk_spin_button_get_value(state->longitude);

    if (!ss_date_time_model_set_location(
            &state->model, configured, latitude, longitude)) {
        set_status(state, "Could not save geographic location.", true);
        sync_controls(state);
        return;
    }
    policy_saved(state);
}

static GtkWidget *build_date_time_panel(SettingsWindow *state)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkWidget *summary = make_label(
        "This is the system-wide temporal authority. Common-aware applications read these clock, calendar and location choices.",
        "page-summary");
    GtkWidget *preview_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *clock_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *calendar_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *location_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *system_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkStringList *strings;

    gtk_widget_add_css_class(page, "settings-content");
    gtk_box_append(GTK_BOX(page), make_label("Date & Time", "page-title"));
    gtk_label_set_wrap(GTK_LABEL(summary), TRUE);
    gtk_box_append(GTK_BOX(page), summary);

    gtk_widget_add_css_class(preview_card, "preview-card");
    state->clock_preview = make_label("--:--", "preview-time");
    state->date_preview = make_label("", "preview-date");
    gtk_label_set_wrap(GTK_LABEL(state->clock_preview), TRUE);
    gtk_label_set_wrap(GTK_LABEL(state->date_preview), TRUE);
    gtk_box_append(GTK_BOX(preview_card), state->clock_preview);
    gtk_box_append(GTK_BOX(preview_card), state->date_preview);
    gtk_box_append(GTK_BOX(page), preview_card);

    gtk_widget_add_css_class(clock_card, "settings-card");
    gtk_box_append(GTK_BOX(clock_card),
                   make_label("Clock system", "section-title"));

    strings = clock_mode_strings();
    state->clock_mode = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    gtk_widget_set_size_request(GTK_WIDGET(state->clock_mode), 360, -1);
    gtk_box_append(GTK_BOX(clock_card),
                   make_setting_row(
                       "System clock",
                       "Choose the clock representation used by Common-aware applications. Standard time uses the operating-system locale; this is not a “Follow system” setting because System Settings is the authority.",
                       GTK_WIDGET(state->clock_mode)));

    state->show_seconds = GTK_SWITCH(gtk_switch_new());
    gtk_box_append(GTK_BOX(clock_card),
                   make_setting_row(
                       "Show seconds",
                       "Show seconds or the closest finer unit supported by the selected clock system.",
                       GTK_WIDGET(state->show_seconds)));
    gtk_box_append(GTK_BOX(page), clock_card);

    gtk_widget_add_css_class(calendar_card, "settings-card");
    gtk_box_append(GTK_BOX(calendar_card),
                   make_label("Calendar systems", "section-title"));

    strings = calendar_strings(false);
    state->primary_calendar = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    gtk_widget_set_size_request(
        GTK_WIDGET(state->primary_calendar), 360, -1);
    gtk_box_append(GTK_BOX(calendar_card),
                   make_setting_row(
                       "Primary calendar",
                       "The calendar system applications should use for their principal human-facing date representation.",
                       GTK_WIDGET(state->primary_calendar)));

    strings = calendar_strings(true);
    state->secondary_calendar = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    gtk_widget_set_size_request(
        GTK_WIDGET(state->secondary_calendar), 360, -1);
    gtk_box_append(GTK_BOX(calendar_card),
                   make_setting_row(
                       "Secondary calendar",
                       "Optionally show the same civil day using another calendar system.",
                       GTK_WIDGET(state->secondary_calendar)));
    gtk_box_append(GTK_BOX(page), calendar_card);

    gtk_widget_add_css_class(location_card, "settings-card");
    gtk_box_append(GTK_BOX(location_card),
                   make_label("Geographic location", "section-title"));

    state->location_configured = GTK_SWITCH(gtk_switch_new());
    gtk_box_append(GTK_BOX(location_card),
                   make_setting_row(
                       "Use geographic location",
                       "Required by solar, sidereal and several historical clock systems.",
                       GTK_WIDGET(state->location_configured)));

    state->latitude = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(-90.0, 90.0, 0.01));
    gtk_spin_button_set_digits(state->latitude, 2U);
    gtk_box_append(GTK_BOX(location_card),
                   make_setting_row(
                       "Latitude",
                       "Degrees north are positive; degrees south are negative.",
                       GTK_WIDGET(state->latitude)));

    state->longitude = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(-180.0, 180.0, 0.01));
    gtk_spin_button_set_digits(state->longitude, 2U);
    gtk_box_append(GTK_BOX(location_card),
                   make_setting_row(
                       "Longitude",
                       "Degrees east of Greenwich are positive; degrees west are negative.",
                       GTK_WIDGET(state->longitude)));
    gtk_box_append(GTK_BOX(page), location_card);

    gtk_widget_add_css_class(system_card, "settings-card");
    gtk_box_append(GTK_BOX(system_card),
                   make_label("Operating-system time", "section-title"));
    state->timezone_value = make_label("Local time", "accent-note");
    gtk_widget_set_halign(state->timezone_value, GTK_ALIGN_END);
    gtk_widget_set_hexpand(state->timezone_value, TRUE);
    gtk_box_append(GTK_BOX(system_card),
                   make_setting_row(
                       "Time zone",
                       "Current operating-system time zone. Protected time-zone editing will use the native system service rather than altering the presentation policy.",
                       state->timezone_value));

    state->status_label = make_label("", "status-ok");
    gtk_label_set_wrap(GTK_LABEL(state->status_label), TRUE);
    gtk_box_append(GTK_BOX(system_card), state->status_label);
    gtk_box_append(GTK_BOX(page), system_card);

    g_signal_connect(state->clock_mode, "notify::selected",
                     G_CALLBACK(on_clock_changed), state);
    g_signal_connect(state->primary_calendar, "notify::selected",
                     G_CALLBACK(on_primary_calendar_changed), state);
    g_signal_connect(state->secondary_calendar, "notify::selected",
                     G_CALLBACK(on_secondary_calendar_changed), state);
    g_signal_connect(state->show_seconds, "notify::active",
                     G_CALLBACK(on_seconds_changed), state);
    g_signal_connect(state->location_configured, "notify::active",
                     G_CALLBACK(on_location_changed), state);
    g_signal_connect(state->latitude, "notify::value",
                     G_CALLBACK(on_location_changed), state);
    g_signal_connect(state->longitude, "notify::value",
                     G_CALLBACK(on_location_changed), state);

    return page;
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
    gtk_box_append(GTK_BOX(sidebar), make_label("SYSTEM", "nav-title"));

    gtk_image_set_pixel_size(GTK_IMAGE(icon), 18);
    gtk_box_append(GTK_BOX(row_box), icon);
    gtk_box_append(GTK_BOX(row_box), make_label("Date & Time", NULL));
    gtk_widget_add_css_class(row, "nav-row");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
    gtk_list_box_append(GTK_LIST_BOX(list), row);
    gtk_list_box_select_row(GTK_LIST_BOX(list), GTK_LIST_BOX_ROW(row));
    gtk_box_append(GTK_BOX(sidebar), list);
    return sidebar;
}

static GtkWidget *build_header(void)
{
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *icon = gtk_image_new_from_icon_name("preferences-system-symbolic");
    GtkWidget *identity = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_widget_add_css_class(header, "titlebar-shell");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_box_append(GTK_BOX(header), icon);
    gtk_box_append(GTK_BOX(identity),
                   make_label("System Settings", "app-title"));
    gtk_box_append(GTK_BOX(identity),
                   make_label("One place for system-wide preferences",
                              "app-subtitle"));
    gtk_box_append(GTK_BOX(header), identity);
    return header;
}

static gboolean on_close_request(GtkWindow *window, gpointer user_data)
{
    SettingsWindow *state = user_data;

    (void)window;
    if (state != NULL && state->timer_id != 0U) {
        g_source_remove(state->timer_id);
        state->timer_id = 0U;
    }
    return FALSE;
}

static void on_activate(GtkApplication *application, gpointer user_data)
{
    SettingsWindow *state = g_new0(SettingsWindow, 1);
    GtkWidget *root;
    GtkWidget *body;
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
    gtk_window_set_default_size(state->window, 1040, 760);
    gtk_window_set_resizable(state->window, TRUE);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(state->window, root);
    gtk_box_append(GTK_BOX(root), build_header());

    body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(body, TRUE);
    gtk_box_append(GTK_BOX(root), body);
    gtk_box_append(GTK_BOX(body), build_sidebar());

    scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(scroller), build_date_time_panel(state));
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_box_append(GTK_BOX(body), scroller);

    sync_controls(state);
    set_status(
        state,
        state->model.persisted_policy_present
            ? "Using the saved system-wide temporal policy."
            : "Using the default system-wide temporal policy.",
        false);

    state->timer_id = g_timeout_add_seconds(1U, refresh_preview, state);
    g_signal_connect(state->window, "close-request",
                     G_CALLBACK(on_close_request), state);
    g_object_set_data_full(G_OBJECT(state->window),
                           "system-settings-state", state, g_free);
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
