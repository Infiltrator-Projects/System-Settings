// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file main.c
 * @brief Native Linux System Settings shell with complete Date & Time policy.
 */

#include "system-settings/date-time-model.h"
#include "system-settings/temporal-policy-store.h"
#include "system-settings/cinnamon-interface.h"
#include "system-settings/location-metadata.h"
#include "system-settings/location-search.h"
#include "system-settings/project-info.h"
#include "system-settings/regional-context.h"
#include "system-settings/system-time-service.h"

#include <gtk/gtk.h>
#include <infiltratr/core.h>
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
    GtkDropDown *clock_mode;
    GtkDropDown *calendar;
    GtkSwitch *show_seconds;
    GtkEntry *location_search;
    GtkButton *location_search_button;
    GtkListBox *location_results;
    GtkWidget *location_summary;
    GtkSpinButton *latitude;
    GtkSpinButton *longitude;
    GtkDropDown *timezone;
    GtkSwitch *network_time;
    GtkEntry *manual_date;
    GtkEntry *manual_time;
    GtkButton *manual_set_time;
    GtkSwitch *use_24h;
    GtkSwitch *show_date;
    GtkDropDown *first_day;
    GtkWidget *status_label;
    GSettings *cinnamon_interface_settings;
    SsDateTimeModel model;
    SsRegionalContext regional_context;
    SsLocationMetadata location_metadata;
    SsSystemTimeService *system_time_service;
    GPtrArray *timezone_ids;
    GCancellable *location_search_cancellable;
    GCancellable *system_time_cancellable;
    guint timer_id;
    guint location_search_generation;
    bool location_metadata_present;
    bool updating_controls;
    bool updating_system_controls;
} SettingsWindow;

typedef struct LocationSearchUiRequest {
    GtkWindow *window;
    guint generation;
} LocationSearchUiRequest;

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

static GtkStringList *calendar_strings(void)
{
    GtkStringList *list = gtk_string_list_new(NULL);
    size_t index;

    for (index = 0U; index < infiltratr_temporal_calendar_count(); ++index) {
        const InfiltratrTemporalCalendarInfo *info =
            infiltratr_temporal_calendar_at(index);
        if (info != NULL) {
            gtk_string_list_append(list, info->name);
        }
    }
    return list;
}

static bool coordinate_close(double left, double right)
{
    double difference = left - right;

    if (difference < 0.0) {
        difference = -difference;
    }
    return difference < 0.000001;
}

static bool location_metadata_matches_policy(
    const SettingsWindow *state,
    const InfiltratrTemporalPolicyV3 *policy)
{
    return state != NULL &&
           policy != NULL &&
           policy->location_configured &&
           state->location_metadata_present &&
           coordinate_close(
               policy->latitude, state->location_metadata.latitude) &&
           coordinate_close(
               policy->longitude, state->location_metadata.longitude);
}

static gchar *format_coordinate_pair(double latitude, double longitude)
{
    const double latitude_magnitude =
        latitude < 0.0 ? -latitude : latitude;
    const double longitude_magnitude =
        longitude < 0.0 ? -longitude : longitude;

    return g_strdup_printf(
        "%.4f° %c, %.4f° %c",
        latitude_magnitude, latitude < 0.0 ? 'S' : 'N',
        longitude_magnitude, longitude < 0.0 ? 'W' : 'E');
}

static GtkStringList *timezone_strings(SettingsWindow *state)
{
    GtkStringList *strings = gtk_string_list_new(NULL);
    size_t index;

    if (state == NULL) {
        return strings;
    }

    g_clear_pointer(&state->timezone_ids, g_ptr_array_unref);
    state->timezone_ids = ss_regional_context_list_timezones();

    if (state->timezone_ids == NULL) {
        state->timezone_ids =
            g_ptr_array_new_with_free_func(g_free);
    }

    if (state->timezone_ids->len == 0U &&
        state->regional_context.timezone_id[0] != '\0') {
        g_ptr_array_add(
            state->timezone_ids,
            g_strdup(state->regional_context.timezone_id));
    }

    for (index = 0U; index < state->timezone_ids->len; ++index) {
        const char *zone = g_ptr_array_index(
            state->timezone_ids, (guint)index);
        gtk_string_list_append(strings, zone);
    }
    return strings;
}

static guint timezone_index_for_id(
    const SettingsWindow *state,
    const char *timezone_id)
{
    size_t index;

    if (state == NULL || state->timezone_ids == NULL ||
        timezone_id == NULL) {
        return GTK_INVALID_LIST_POSITION;
    }

    for (index = 0U; index < state->timezone_ids->len; ++index) {
        const char *candidate = g_ptr_array_index(
            state->timezone_ids, (guint)index);
        if (g_strcmp0(candidate, timezone_id) == 0) {
            return (guint)index;
        }
    }
    return GTK_INVALID_LIST_POSITION;
}

static GtkStringList *first_day_strings(void)
{
    const char *const values[] = {
        "Use locale default",
        "Sunday",
        "Monday",
        NULL
    };
    return gtk_string_list_new(values);
}

static guint first_day_index(int value)
{
    if (value == 0) {
        return 1U;
    }
    if (value == 1) {
        return 2U;
    }
    return 0U;
}

static int first_day_value(guint index)
{
    if (index == 1U) {
        return 0;
    }
    if (index == 2U) {
        return 1;
    }
    return 7;
}

static void update_location_summary(SettingsWindow *state)
{
    const InfiltratrTemporalPolicyV3 *policy;
    g_autofree gchar *coordinates = NULL;
    g_autofree gchar *summary = NULL;

    if (state == NULL || state->location_summary == NULL) {
        return;
    }

    policy = ss_date_time_model_policy(&state->model);
    if (policy == NULL) {
        return;
    }

    if (policy->location_configured) {
        coordinates = format_coordinate_pair(
            policy->latitude, policy->longitude);
        if (location_metadata_matches_policy(state, policy) &&
            state->location_metadata.display_name[0] != '\0') {
            summary = g_strdup_printf(
                "%s • %s",
                state->location_metadata.display_name,
                coordinates);
        } else {
            summary = g_strdup_printf(
                "Selected geographic coordinates • %s",
                coordinates);
        }
    } else if (state->regional_context.has_reference_coordinates) {
        const char *city =
            state->regional_context.timezone_city[0] != '\0'
                ? state->regional_context.timezone_city
                : state->regional_context.timezone_id;
        coordinates = format_coordinate_pair(
            state->regional_context.reference_latitude,
            state->regional_context.reference_longitude);
        summary = g_strdup_printf(
            "%s is the current approximation from system time zone %s • %s. "
            "Search for your actual locality to refine it.",
            city,
            state->regional_context.timezone_id,
            coordinates);
    } else {
        summary = g_strdup(
            "Search for a locality to establish geographic coordinates.");
    }

    gtk_label_set_text(GTK_LABEL(state->location_summary), summary);
}

static void sync_native_format_controls(SettingsWindow *state)
{
    bool value;
    int first_day = 7;

    if (state == NULL || state->cinnamon_interface_settings == NULL) {
        return;
    }

    state->updating_system_controls = true;
    if (state->use_24h != NULL &&
        ss_cinnamon_interface_get_boolean(
            state->cinnamon_interface_settings,
            "clock-use-24h",
            &value)) {
        gtk_switch_set_active(state->use_24h, value);
    }
    if (state->show_date != NULL &&
        ss_cinnamon_interface_get_boolean(
            state->cinnamon_interface_settings,
            "clock-show-date",
            &value)) {
        gtk_switch_set_active(state->show_date, value);
    }
    if (state->first_day != NULL &&
        ss_cinnamon_interface_get_first_day(
            state->cinnamon_interface_settings,
            &first_day)) {
        gtk_drop_down_set_selected(
            state->first_day, first_day_index(first_day));
    }
    state->updating_system_controls = false;
}

static void fill_manual_time_entries(SettingsWindow *state)
{
    g_autoptr(GDateTime) now = NULL;
    g_autofree gchar *date = NULL;
    g_autofree gchar *time = NULL;

    if (state == NULL || state->manual_date == NULL ||
        state->manual_time == NULL) {
        return;
    }

    now = g_date_time_new_now_local();
    if (now == NULL) {
        return;
    }
    date = g_date_time_format(now, "%Y-%m-%d");
    time = g_date_time_format(now, "%H:%M:%S");
    if (date != NULL) {
        gtk_editable_set_text(
            GTK_EDITABLE(state->manual_date), date);
    }
    if (time != NULL) {
        gtk_editable_set_text(
            GTK_EDITABLE(state->manual_time), time);
    }
}

static void sync_system_time_controls(SettingsWindow *state)
{
    SsSystemTimeState system_state;
    guint zone_index;

    if (state == NULL) {
        return;
    }

    if (state->system_time_service == NULL ||
        !ss_system_time_service_read(
            state->system_time_service, &system_state)) {
        if (state->timezone != NULL) {
            gtk_widget_set_sensitive(
                GTK_WIDGET(state->timezone), FALSE);
        }
        if (state->network_time != NULL) {
            gtk_widget_set_sensitive(
                GTK_WIDGET(state->network_time), FALSE);
        }
        if (state->manual_date != NULL) {
            gtk_widget_set_sensitive(
                GTK_WIDGET(state->manual_date), FALSE);
        }
        if (state->manual_time != NULL) {
            gtk_widget_set_sensitive(
                GTK_WIDGET(state->manual_time), FALSE);
        }
        if (state->manual_set_time != NULL) {
            gtk_widget_set_sensitive(
                GTK_WIDGET(state->manual_set_time), FALSE);
        }
        return;
    }

    state->updating_system_controls = true;

    if (state->timezone != NULL) {
        zone_index = timezone_index_for_id(
            state, system_state.timezone);
        if (zone_index != GTK_INVALID_LIST_POSITION) {
            gtk_drop_down_set_selected(
                state->timezone, zone_index);
        }
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->timezone), TRUE);
    }

    if (state->network_time != NULL) {
        gtk_switch_set_active(
            state->network_time, system_state.ntp_enabled);
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->network_time),
            system_state.can_ntp);
    }

    if (state->manual_date != NULL) {
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->manual_date),
            !system_state.ntp_enabled);
    }
    if (state->manual_time != NULL) {
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->manual_time),
            !system_state.ntp_enabled);
    }
    if (state->manual_set_time != NULL) {
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->manual_set_time),
            !system_state.ntp_enabled);
    }

    state->updating_system_controls = false;
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

static guint calendar_index_for_id(const char *id)
{
    size_t index;

    for (index = 0U; index < infiltratr_temporal_calendar_count(); ++index) {
        const InfiltratrTemporalCalendarInfo *info =
            infiltratr_temporal_calendar_at(index);
        if (info != NULL && strcmp(info->id, id) == 0) {
            return (guint)index;
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
selected_calendar(GtkDropDown *dropdown)
{
    size_t index;
    if (dropdown == NULL) {
        return NULL;
    }
    index = (size_t)gtk_drop_down_get_selected(dropdown);
    return infiltratr_temporal_calendar_at(index);
}

static void update_control_capabilities(SettingsWindow *state)
{
    const InfiltratrTemporalClockModeInfo *mode;
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL) {
        return;
    }

    mode = selected_clock_mode(state);
    policy = ss_date_time_model_policy(&state->model);
    gtk_widget_set_sensitive(GTK_WIDGET(state->show_seconds),
                             mode == NULL || mode->supports_seconds);
    gtk_widget_set_sensitive(GTK_WIDGET(state->latitude), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(state->longitude), TRUE);

    update_location_summary(state);
    sync_native_format_controls(state);
    sync_system_time_controls(state);

    if (mode != NULL &&
        (mode->requires_latitude || mode->requires_longitude) &&
        (policy == NULL || !policy->location_configured) &&
        !state->regional_context.has_reference_coordinates) {
        set_status(state,
                   "This clock system needs a geographic location for a meaningful result.",
                   false);
    }
}

static bool format_preview(const SettingsWindow *state,
                           char *buffer,
                           size_t capacity)
{
    const InfiltratrTemporalPolicyV3 *policy =
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
        const gboolean use_24h =
            state->cinnamon_interface_settings != NULL
                ? g_settings_get_boolean(
                    state->cinnamon_interface_settings, "clock-use-24h")
                : TRUE;
        conventional = use_24h
            ? INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_24
            : INFILTRATR_CLOCK_PROFILE_CONVENTIONAL_12;
    } else if (strcmp(policy->clock_mode, "standard-12") == 0) {
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
    const InfiltratrTemporalPolicyV3 *policy;
    const InfiltratrTemporalCalendarInfo *calendar;
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

    calendar = infiltratr_temporal_calendar_find(policy->calendar);
    {
        g_autofree gchar *date = g_date_time_format(now, "%A, %e %B %Y");
        g_autofree gchar *summary = NULL;
        const gchar *zone = g_date_time_get_timezone_abbreviation(now);

        if (calendar != NULL) {
            if (strcmp(policy->calendar, "gregorian") == 0) {
                summary = g_strdup_printf(
                    "%s • %s", calendar->name, date != NULL ? date : "");
            } else {
                summary = g_strdup_printf(
                    "Selected calendar: %s • local Gregorian date: %s",
                    calendar->name, date != NULL ? date : "");
            }
        }
        if (summary != NULL) {
            gtk_label_set_text(GTK_LABEL(state->date_preview), summary);
        }
        (void)zone;
    }

    return G_SOURCE_CONTINUE;
}

static void sync_controls(SettingsWindow *state)
{
    const InfiltratrTemporalPolicyV3 *policy =
        ss_date_time_model_policy(&state->model);

    if (policy == NULL) {
        return;
    }

    state->updating_controls = true;
    gtk_drop_down_set_selected(
        state->clock_mode, clock_index_for_id(policy->clock_mode));
    gtk_drop_down_set_selected(
        state->calendar,
        calendar_index_for_id(policy->calendar));
    gtk_switch_set_active(state->show_seconds, policy->show_seconds);
    if (policy->location_configured) {
        gtk_spin_button_set_value(
            state->latitude, policy->latitude);
        gtk_spin_button_set_value(
            state->longitude, policy->longitude);
        if (state->location_search != NULL &&
            location_metadata_matches_policy(state, policy)) {
            gtk_editable_set_text(
                GTK_EDITABLE(state->location_search),
                state->location_metadata.display_name);
        }
    } else if (state->regional_context.has_reference_coordinates) {
        gtk_spin_button_set_value(
            state->latitude,
            state->regional_context.reference_latitude);
        gtk_spin_button_set_value(
            state->longitude,
            state->regional_context.reference_longitude);
        if (state->location_search != NULL &&
            state->regional_context.timezone_city[0] != '\0') {
            gtk_editable_set_text(
                GTK_EDITABLE(state->location_search),
                state->regional_context.timezone_city);
        }
    }
    state->updating_controls = false;
    update_control_capabilities(state);
    (void)refresh_preview(state);
}

static void on_cinnamon_interface_changed(
    GSettings *settings G_GNUC_UNUSED,
    gchar *key,
    gpointer user_data)
{
    SettingsWindow *state = user_data;

    if (state == NULL) {
        return;
    }

    sync_native_format_controls(state);

    if (!state->model.persisted_policy_present &&
        (g_strcmp0(key, "clock-use-24h") == 0 ||
         g_strcmp0(key, "clock-show-seconds") == 0)) {
        if (!ss_date_time_model_reload(&state->model)) {
            set_status(state,
                       "Mint/Cinnamon temporal preferences changed, but could not be reloaded.",
                       true);
            return;
        }
        sync_controls(state);
        set_status(
            state,
            "Using Mint/Cinnamon temporal preferences until an Infiltrator policy is saved.",
            false);
    } else {
        (void)refresh_preview(state);
    }
}

static void policy_saved(SettingsWindow *state)
{
    set_status(state,
               "System temporal policy saved. Calendar and other Common-aware applications use this setting.",
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

static void on_calendar_changed(GObject *object,
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

    calendar = selected_calendar(state->calendar);
    if (calendar == NULL ||
        !ss_date_time_model_set_calendar(&state->model, calendar->id)) {
        set_status(state, "Could not save the calendar.", true);
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

static void system_time_changed(
    SsSystemTimeService *service G_GNUC_UNUSED,
    const SsSystemTimeState *system_state,
    gpointer user_data)
{
    SettingsWindow *state = user_data;

    if (state == NULL || system_state == NULL) {
        return;
    }

    (void)ss_regional_context_detect(&state->regional_context);
    sync_system_time_controls(state);
    if (!ss_date_time_model_policy(&state->model)->location_configured) {
        state->updating_controls = true;
        if (state->regional_context.has_reference_coordinates) {
            gtk_spin_button_set_value(
                state->latitude,
                state->regional_context.reference_latitude);
            gtk_spin_button_set_value(
                state->longitude,
                state->regional_context.reference_longitude);
        }
        state->updating_controls = false;
    }
    update_location_summary(state);
    (void)refresh_preview(state);
}

static void system_time_operation_complete(
    SsSystemTimeService *service G_GNUC_UNUSED,
    bool success,
    const char *error_message,
    gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);
    SettingsWindow *state = g_object_get_data(
        G_OBJECT(window), "system-settings-state");

    if (state != NULL) {
        if (success) {
            (void)ss_regional_context_detect(&state->regional_context);
            sync_system_time_controls(state);
            update_location_summary(state);
            set_status(
                state,
                "Operating-system date/time settings updated.",
                false);
        } else {
            sync_system_time_controls(state);
            set_status(
                state,
                error_message != NULL
                    ? error_message
                    : "The operating-system date/time setting could not be changed.",
                true);
        }
    }
    g_object_unref(window);
}

static void ensure_system_cancellable(SettingsWindow *state)
{
    if (state != NULL && state->system_time_cancellable == NULL) {
        state->system_time_cancellable = g_cancellable_new();
    }
}

static void request_system_timezone(
    SettingsWindow *state,
    const char *timezone_id)
{
    if (state == NULL || state->system_time_service == NULL ||
        timezone_id == NULL || timezone_id[0] == '\0') {
        return;
    }

    ensure_system_cancellable(state);
    set_status(state, "Updating the operating-system time zone…", false);
    ss_system_time_service_set_timezone_async(
        state->system_time_service,
        timezone_id,
        state->system_time_cancellable,
        system_time_operation_complete,
        g_object_ref(state->window));
}

static void on_timezone_changed(GObject *object G_GNUC_UNUSED,
                                GParamSpec *pspec G_GNUC_UNUSED,
                                gpointer user_data)
{
    SettingsWindow *state = user_data;
    guint selected;
    const char *timezone_id;

    if (state == NULL || state->updating_system_controls ||
        state->timezone_ids == NULL) {
        return;
    }

    selected = gtk_drop_down_get_selected(state->timezone);
    if (selected >= state->timezone_ids->len) {
        return;
    }
    timezone_id = g_ptr_array_index(state->timezone_ids, selected);
    request_system_timezone(state, timezone_id);
}

static void on_network_time_changed(GObject *object,
                                    GParamSpec *pspec G_GNUC_UNUSED,
                                    gpointer user_data)
{
    SettingsWindow *state = user_data;
    bool enabled;

    if (state == NULL || state->updating_system_controls ||
        state->system_time_service == NULL) {
        return;
    }

    enabled = gtk_switch_get_active(GTK_SWITCH(object)) != FALSE;
    ensure_system_cancellable(state);
    set_status(
        state,
        enabled ? "Enabling network time…" : "Disabling network time…",
        false);
    ss_system_time_service_set_ntp_async(
        state->system_time_service,
        enabled,
        state->system_time_cancellable,
        system_time_operation_complete,
        g_object_ref(state->window));
}

static bool parse_manual_datetime(
    const char *date_text,
    const char *time_text,
    int64_t *unix_time_usec)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second = 0;
    char trailing;
    g_autoptr(GDateTime) value = NULL;

    if (date_text == NULL || time_text == NULL ||
        unix_time_usec == NULL ||
        sscanf(date_text,
               "%d-%d-%d%c",
               &year, &month, &day, &trailing) != 3) {
        return false;
    }

    if (sscanf(time_text,
               "%d:%d:%d%c",
               &hour, &minute, &second, &trailing) != 3) {
        second = 0;
        if (sscanf(time_text,
                   "%d:%d%c",
                   &hour, &minute, &trailing) != 2) {
            return false;
        }
    }

    if (year < 1970 || year > 9999 ||
        month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 ||
        second < 0 || second > 59) {
        return false;
    }

    value = g_date_time_new_local(
        year, month, day, hour, minute, (double)second);
    if (value == NULL) {
        return false;
    }
    *unix_time_usec =
        (int64_t)g_date_time_to_unix(value) * G_USEC_PER_SEC;
    return true;
}

static void on_manual_set_time_clicked(
    GtkButton *button G_GNUC_UNUSED,
    gpointer user_data)
{
    SettingsWindow *state = user_data;
    int64_t unix_time_usec;
    const char *date_text;
    const char *time_text;

    if (state == NULL || state->system_time_service == NULL) {
        return;
    }

    date_text = gtk_editable_get_text(GTK_EDITABLE(state->manual_date));
    time_text = gtk_editable_get_text(GTK_EDITABLE(state->manual_time));
    if (!parse_manual_datetime(
            date_text, time_text, &unix_time_usec)) {
        set_status(
            state,
            "Enter a valid local date as YYYY-MM-DD and time as HH:MM[:SS].",
            true);
        return;
    }

    ensure_system_cancellable(state);
    set_status(state, "Setting the operating-system date and time…", false);
    ss_system_time_service_set_time_async(
        state->system_time_service,
        unix_time_usec,
        state->system_time_cancellable,
        system_time_operation_complete,
        g_object_ref(state->window));
}

static void on_use_24h_changed(GObject *object,
                               GParamSpec *pspec G_GNUC_UNUSED,
                               gpointer user_data)
{
    SettingsWindow *state = user_data;
    bool value;

    if (state == NULL || state->updating_system_controls ||
        state->cinnamon_interface_settings == NULL) {
        return;
    }

    value = gtk_switch_get_active(GTK_SWITCH(object)) != FALSE;
    if (!ss_cinnamon_interface_set_clock_use_24h(
            state->cinnamon_interface_settings, value)) {
        set_status(state, "Could not change the desktop 12/24-hour format.", true);
        sync_native_format_controls(state);
        return;
    }
    set_status(state, "Desktop clock format updated.", false);
    (void)refresh_preview(state);
}

static void on_show_date_changed(GObject *object,
                                 GParamSpec *pspec G_GNUC_UNUSED,
                                 gpointer user_data)
{
    SettingsWindow *state = user_data;
    bool value;

    if (state == NULL || state->updating_system_controls ||
        state->cinnamon_interface_settings == NULL) {
        return;
    }
    value = gtk_switch_get_active(GTK_SWITCH(object)) != FALSE;
    if (!ss_cinnamon_interface_set_boolean(
            state->cinnamon_interface_settings,
            "clock-show-date",
            value)) {
        set_status(state, "Could not change the Cinnamon panel date setting.", true);
        sync_native_format_controls(state);
        return;
    }
    set_status(state, "Cinnamon panel date setting updated.", false);
}

static void on_first_day_changed(GObject *object G_GNUC_UNUSED,
                                 GParamSpec *pspec G_GNUC_UNUSED,
                                 gpointer user_data)
{
    SettingsWindow *state = user_data;
    int value;

    if (state == NULL || state->updating_system_controls ||
        state->cinnamon_interface_settings == NULL) {
        return;
    }

    value = first_day_value(
        gtk_drop_down_get_selected(state->first_day));
    if (!ss_cinnamon_interface_set_first_day(
            state->cinnamon_interface_settings, value)) {
        set_status(state, "Could not change the first day of week.", true);
        sync_native_format_controls(state);
        return;
    }
    set_status(state, "First day of week updated.", false);
}

static void clear_location_results(SettingsWindow *state)
{
    GtkWidget *child;

    if (state == NULL || state->location_results == NULL) {
        return;
    }

    child = gtk_widget_get_first_child(
        GTK_WIDGET(state->location_results));
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(state->location_results, child);
        child = next;
    }
    gtk_widget_set_visible(
        GTK_WIDGET(state->location_results), FALSE);
}

static void on_location_result_activated(
    GtkListBox *box G_GNUC_UNUSED,
    GtkListBoxRow *row,
    gpointer user_data)
{
    SettingsWindow *state = user_data;
    const SsLocationSearchResult *result;
    char timezone_id[SS_TIMEZONE_ID_CAPACITY] = {0};
    const InfiltratrTemporalPolicyV3 *policy;

    if (state == NULL || row == NULL) {
        return;
    }

    result = g_object_get_data(
        G_OBJECT(row), "ss-location-result");
    if (result == NULL) {
        return;
    }

    if (!ss_date_time_model_set_location(
            &state->model,
            true,
            result->latitude,
            result->longitude)) {
        set_status(state, "Could not save the selected geographic location.", true);
        return;
    }

    memset(&state->location_metadata, 0,
           sizeof(state->location_metadata));
    (void)g_strlcpy(
        state->location_metadata.display_name,
        result->display_name,
        sizeof(state->location_metadata.display_name));
    (void)g_strlcpy(
        state->location_metadata.country_code,
        result->country_code,
        sizeof(state->location_metadata.country_code));
    state->location_metadata.latitude = result->latitude;
    state->location_metadata.longitude = result->longitude;

    if (result->country_code[0] != '\0' &&
        ss_regional_context_nearest_timezone(
            result->country_code,
            result->latitude,
            result->longitude,
            timezone_id,
            sizeof(timezone_id))) {
        (void)g_strlcpy(
            state->location_metadata.timezone_id,
            timezone_id,
            sizeof(state->location_metadata.timezone_id));
    }

    state->location_metadata_present =
        ss_location_metadata_save(&state->location_metadata);

    state->updating_controls = true;
    gtk_editable_set_text(
        GTK_EDITABLE(state->location_search),
        result->display_name);
    gtk_spin_button_set_value(
        state->latitude, result->latitude);
    gtk_spin_button_set_value(
        state->longitude, result->longitude);
    state->updating_controls = false;
    clear_location_results(state);

    policy = ss_date_time_model_policy(&state->model);
    if (policy != NULL) {
        update_location_summary(state);
    }
    policy_saved(state);

    if (timezone_id[0] != '\0') {
        request_system_timezone(state, timezone_id);
    }
}

static void location_search_request_free(
    LocationSearchUiRequest *request)
{
    if (request == NULL) {
        return;
    }
    g_clear_object(&request->window);
    g_free(request);
}

static void location_search_complete(
    GPtrArray *results,
    const char *error_message,
    gpointer user_data)
{
    LocationSearchUiRequest *request = user_data;
    SettingsWindow *state;
    size_t index;

    if (request == NULL || request->window == NULL) {
        if (results != NULL) {
            g_ptr_array_unref(results);
        }
        location_search_request_free(request);
        return;
    }

    state = g_object_get_data(
        G_OBJECT(request->window), "system-settings-state");
    if (state == NULL ||
        request->generation != state->location_search_generation) {
        if (results != NULL) {
            g_ptr_array_unref(results);
        }
        location_search_request_free(request);
        return;
    }

    clear_location_results(state);
    if (results == NULL || results->len == 0U) {
        set_status(
            state,
            error_message != NULL
                ? error_message
                : "No matching locations were found.",
            true);
        if (results != NULL) {
            g_ptr_array_unref(results);
        }
        location_search_request_free(request);
        return;
    }

    for (index = 0U; index < results->len; ++index) {
        const SsLocationSearchResult *item =
            g_ptr_array_index(results, (guint)index);
        SsLocationSearchResult *copy;
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *title;
        GtkWidget *coordinates;
        g_autofree gchar *coordinate_text = NULL;

        if (item == NULL) {
            continue;
        }
        copy = g_memdup2(item, sizeof(*item));
        g_object_set_data_full(
            G_OBJECT(row),
            "ss-location-result",
            copy,
            g_free);

        title = make_label(item->display_name, "setting-label");
        coordinate_text = format_coordinate_pair(
            item->latitude, item->longitude);
        coordinates = make_label(
            coordinate_text, "setting-description");
        gtk_box_append(GTK_BOX(box), title);
        gtk_box_append(GTK_BOX(box), coordinates);
        gtk_widget_add_css_class(row, "location-result-row");
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
        gtk_list_box_append(state->location_results, row);
    }

    gtk_widget_set_visible(
        GTK_WIDGET(state->location_results), TRUE);
    set_status(
        state,
        "Select the matching locality. Its coordinates will become the system geographic location and the nearest IANA time zone will be applied to the operating system.",
        false);

    g_ptr_array_unref(results);
    location_search_request_free(request);
}

static void begin_location_search(SettingsWindow *state)
{
    const char *query;

    if (state == NULL || state->location_search == NULL) {
        return;
    }

    query = gtk_editable_get_text(
        GTK_EDITABLE(state->location_search));
    if (query == NULL || query[0] == '\0') {
        set_status(state, "Enter a locality or place name.", true);
        return;
    }

    if (state->location_search_cancellable != NULL) {
        g_cancellable_cancel(state->location_search_cancellable);
        g_clear_object(&state->location_search_cancellable);
    }
    state->location_search_cancellable = g_cancellable_new();
    state->location_search_generation++;

    clear_location_results(state);
    set_status(state, "Searching for matching localities…", false);
    {
        LocationSearchUiRequest *request =
            g_new0(LocationSearchUiRequest, 1);
        request->window = g_object_ref(state->window);
        request->generation = state->location_search_generation;
        ss_location_search_async(
            query,
            state->location_search_cancellable,
            location_search_complete,
            request);
    }
}

static void on_location_search_clicked(
    GtkButton *button G_GNUC_UNUSED,
    gpointer user_data)
{
    begin_location_search(user_data);
}

static void on_location_search_activate(
    GtkEntry *entry G_GNUC_UNUSED,
    gpointer user_data)
{
    begin_location_search(user_data);
}

static void on_location_coordinate_changed(
    GObject *object G_GNUC_UNUSED,
    GParamSpec *pspec G_GNUC_UNUSED,
    gpointer user_data)
{
    SettingsWindow *state = user_data;
    double latitude;
    double longitude;

    if (state == NULL || state->updating_controls) {
        return;
    }

    latitude = gtk_spin_button_get_value(state->latitude);
    longitude = gtk_spin_button_get_value(state->longitude);

    if (!ss_date_time_model_set_location(
            &state->model, true, latitude, longitude)) {
        set_status(state, "Could not save custom geographic coordinates.", true);
        sync_controls(state);
        return;
    }

    memset(&state->location_metadata, 0,
           sizeof(state->location_metadata));
    (void)g_strlcpy(
        state->location_metadata.display_name,
        "Custom coordinates",
        sizeof(state->location_metadata.display_name));
    state->location_metadata.latitude = latitude;
    state->location_metadata.longitude = longitude;
    state->location_metadata_present =
        ss_location_metadata_save(&state->location_metadata);
    gtk_editable_set_text(
        GTK_EDITABLE(state->location_search),
        "Custom coordinates");
    policy_saved(state);
}

static GtkWidget *build_date_time_panel(SettingsWindow *state)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkWidget *summary = make_label(
        "System Settings is the authoritative Date & Time frontend. It writes ordinary Mint/Linux settings through their native interfaces and adds richer Common-aware clock, calendar and geographic policy without maintaining a second copy of native system state.",
        "page-summary");
    GtkWidget *preview_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *system_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *location_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *clock_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *calendar_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *format_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *manual_box;
    GtkWidget *search_box;
    GtkStringList *strings;
    GtkExpression *expression;

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

    /*
     * Native system controls replace the functional scope of Mint's Date &
     * Time panel. The shell remains unprivileged; timedated performs any
     * required operation-scoped polkit authorisation.
     */
    gtk_widget_add_css_class(system_card, "settings-card");
    gtk_widget_add_css_class(system_card, "system-card");
    gtk_box_append(GTK_BOX(system_card),
                   make_label("Operating-system date & time",
                              "section-title"));

    strings = timezone_strings(state);
    state->timezone = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    expression = gtk_property_expression_new(
        GTK_TYPE_STRING_OBJECT, NULL, "string");
    gtk_drop_down_set_expression(state->timezone, expression);
    gtk_expression_unref(expression);
    gtk_drop_down_set_enable_search(state->timezone, TRUE);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->timezone), "setting-dropdown");
    gtk_widget_set_size_request(
        GTK_WIDGET(state->timezone), 360, -1);
    gtk_box_append(
        GTK_BOX(system_card),
        make_setting_row(
            "Time zone",
            "Select the real operating-system IANA time zone. This changes Linux itself through systemd-timedated, so Cinnamon and ordinary applications see the same value.",
            GTK_WIDGET(state->timezone)));

    state->network_time = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->network_time), "setting-switch");
    gtk_box_append(
        GTK_BOX(system_card),
        make_setting_row(
            "Network time",
            "Synchronise the system clock through the operating system's configured network-time service.",
            GTK_WIDGET(state->network_time)));

    manual_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->manual_date = GTK_ENTRY(gtk_entry_new());
    state->manual_time = GTK_ENTRY(gtk_entry_new());
    state->manual_set_time =
        GTK_BUTTON(gtk_button_new_with_label("Set"));
    gtk_entry_set_placeholder_text(
        state->manual_date, "YYYY-MM-DD");
    gtk_entry_set_placeholder_text(
        state->manual_time, "HH:MM:SS");
    gtk_entry_set_max_length(state->manual_date, 10);
    gtk_entry_set_max_length(state->manual_time, 8);
    gtk_widget_set_size_request(
        GTK_WIDGET(state->manual_date), 130, -1);
    gtk_widget_set_size_request(
        GTK_WIDGET(state->manual_time), 105, -1);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_date), "setting-entry");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_time), "setting-entry");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_set_time), "setting-button");
    gtk_box_append(GTK_BOX(manual_box),
                   GTK_WIDGET(state->manual_date));
    gtk_box_append(GTK_BOX(manual_box),
                   GTK_WIDGET(state->manual_time));
    gtk_box_append(GTK_BOX(manual_box),
                   GTK_WIDGET(state->manual_set_time));
    gtk_box_append(
        GTK_BOX(system_card),
        make_setting_row(
            "Manual date and time",
            "Available when network time is off. Values are interpreted in the currently selected system time zone.",
            manual_box));
    fill_manual_time_entries(state);

    state->status_label = make_label("", "status-ok");
    gtk_label_set_wrap(GTK_LABEL(state->status_label), TRUE);
    gtk_box_append(GTK_BOX(system_card), state->status_label);
    gtk_box_append(GTK_BOX(page), system_card);

    /*
     * Geographic location is a first-class named location, not an optional
     * 0,0 pair. A time-zone reference seeds the page until a locality is
     * selected, but there is deliberately no "Not selected" pseudo-choice.
     */
    gtk_widget_add_css_class(location_card, "settings-card");
    gtk_widget_add_css_class(location_card, "location-card");
    gtk_box_append(GTK_BOX(location_card),
                   make_label("Geographic location", "section-title"));

    state->location_summary = make_label("", "accent-note");
    gtk_label_set_wrap(GTK_LABEL(state->location_summary), TRUE);
    gtk_box_append(GTK_BOX(location_card), state->location_summary);

    search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->location_search = GTK_ENTRY(gtk_entry_new());
    state->location_search_button =
        GTK_BUTTON(gtk_button_new_with_label("Search"));
    gtk_entry_set_placeholder_text(
        state->location_search,
        "Type a locality, for example Mooroopna");
    gtk_widget_set_hexpand(
        GTK_WIDGET(state->location_search), TRUE);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_search), "setting-entry");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_search_button), "setting-button");
    gtk_box_append(
        GTK_BOX(search_box), GTK_WIDGET(state->location_search));
    gtk_box_append(
        GTK_BOX(search_box),
        GTK_WIDGET(state->location_search_button));
    gtk_box_append(
        GTK_BOX(location_card),
        make_setting_row(
            "Locality",
            "Search by town, suburb, city or place name. Selecting a result stores its coordinates for location-dependent clocks and applies the nearest matching IANA time zone to the operating system; the time-zone selector above remains available for correction.",
            search_box));

    state->location_results = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_results), "location-results");
    gtk_widget_set_visible(
        GTK_WIDGET(state->location_results), FALSE);
    gtk_box_append(
        GTK_BOX(location_card),
        GTK_WIDGET(state->location_results));

    state->latitude = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(-90.0, 90.0, 0.0001));
    gtk_widget_add_css_class(
        GTK_WIDGET(state->latitude), "setting-spin");
    gtk_spin_button_set_digits(state->latitude, 4U);
    gtk_box_append(
        GTK_BOX(location_card),
        make_setting_row(
            "Latitude",
            "Advanced coordinate override. Degrees north are positive; degrees south are negative.",
            GTK_WIDGET(state->latitude)));

    state->longitude = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(-180.0, 180.0, 0.0001));
    gtk_widget_add_css_class(
        GTK_WIDGET(state->longitude), "setting-spin");
    gtk_spin_button_set_digits(state->longitude, 4U);
    gtk_box_append(
        GTK_BOX(location_card),
        make_setting_row(
            "Longitude",
            "Advanced coordinate override. Degrees east of Greenwich are positive; degrees west are negative.",
            GTK_WIDGET(state->longitude)));
    gtk_box_append(GTK_BOX(page), location_card);

    gtk_widget_add_css_class(clock_card, "settings-card");
    gtk_widget_add_css_class(clock_card, "clock-card");
    gtk_box_append(GTK_BOX(clock_card),
                   make_label("Clock system", "section-title"));

    strings = clock_mode_strings();
    state->clock_mode = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->clock_mode), "setting-dropdown");
    gtk_widget_set_size_request(
        GTK_WIDGET(state->clock_mode), 360, -1);
    gtk_box_append(
        GTK_BOX(clock_card),
        make_setting_row(
            "System clock",
            "Choose the human clock representation used by Common-aware applications. Standard time follows the native desktop's 12/24-hour preference below; alternative clock systems remain an Infiltrator extension.",
            GTK_WIDGET(state->clock_mode)));

    state->show_seconds = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->show_seconds), "setting-switch");
    gtk_box_append(
        GTK_BOX(clock_card),
        make_setting_row(
            "Show seconds",
            "Show seconds or the closest finer unit supported by the selected clock system. The equivalent Cinnamon panel preference is kept aligned.",
            GTK_WIDGET(state->show_seconds)));
    gtk_box_append(GTK_BOX(page), clock_card);

    gtk_widget_add_css_class(calendar_card, "settings-card");
    gtk_widget_add_css_class(calendar_card, "calendar-card");
    gtk_box_append(GTK_BOX(calendar_card),
                   make_label("Calendar system", "section-title"));

    strings = calendar_strings();
    state->calendar = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->calendar), "setting-dropdown");
    gtk_widget_set_size_request(
        GTK_WIDGET(state->calendar), 360, -1);
    gtk_box_append(
        GTK_BOX(calendar_card),
        make_setting_row(
            "Calendar",
            "Choose the calendar system used by Common-aware applications. Gregorian remains the ordinary Mint/Linux calendar.",
            GTK_WIDGET(state->calendar)));
    gtk_box_append(GTK_BOX(page), calendar_card);

    /*
     * These are ordinary Cinnamon authorities, not copies in the Infiltrator
     * policy. Writing them here has exactly the same system effect as Mint's
     * Date & Time format controls.
     */
    gtk_widget_add_css_class(format_card, "settings-card");
    gtk_box_append(GTK_BOX(format_card),
                   make_label("Desktop format", "section-title"));

    state->use_24h = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->use_24h), "setting-switch");
    gtk_box_append(
        GTK_BOX(format_card),
        make_setting_row(
            "Use 24-hour clock",
            "Native Cinnamon clock preference used by Standard time and non-Common applications. GNOME's conventional clock-format preference is mirrored for compatibility, matching Mint's behaviour.",
            GTK_WIDGET(state->use_24h)));

    state->show_date = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->show_date), "setting-switch");
    gtk_box_append(
        GTK_BOX(format_card),
        make_setting_row(
            "Display the date",
            "Show the date in Cinnamon's panel clock.",
            GTK_WIDGET(state->show_date)));

    strings = first_day_strings();
    state->first_day = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->first_day), "setting-dropdown");
    gtk_widget_set_size_request(
        GTK_WIDGET(state->first_day), 220, -1);
    gtk_box_append(
        GTK_BOX(format_card),
        make_setting_row(
            "First day of week",
            "Use the locale default, Sunday or Monday for Cinnamon's calendar.",
            GTK_WIDGET(state->first_day)));
    gtk_box_append(GTK_BOX(page), format_card);

    g_signal_connect(
        state->clock_mode, "notify::selected",
        G_CALLBACK(on_clock_changed), state);
    g_signal_connect(
        state->calendar, "notify::selected",
        G_CALLBACK(on_calendar_changed), state);
    g_signal_connect(
        state->show_seconds, "notify::active",
        G_CALLBACK(on_seconds_changed), state);

    g_signal_connect(
        state->timezone, "notify::selected",
        G_CALLBACK(on_timezone_changed), state);
    g_signal_connect(
        state->network_time, "notify::active",
        G_CALLBACK(on_network_time_changed), state);
    g_signal_connect(
        state->manual_set_time, "clicked",
        G_CALLBACK(on_manual_set_time_clicked), state);

    g_signal_connect(
        state->location_search_button, "clicked",
        G_CALLBACK(on_location_search_clicked), state);
    g_signal_connect(
        state->location_search, "activate",
        G_CALLBACK(on_location_search_activate), state);
    g_signal_connect(
        state->location_results, "row-activated",
        G_CALLBACK(on_location_result_activated), state);
    g_signal_connect(
        state->latitude, "notify::value",
        G_CALLBACK(on_location_coordinate_changed), state);
    g_signal_connect(
        state->longitude, "notify::value",
        G_CALLBACK(on_location_coordinate_changed), state);

    g_signal_connect(
        state->use_24h, "notify::active",
        G_CALLBACK(on_use_24h_changed), state);
    g_signal_connect(
        state->show_date, "notify::active",
        G_CALLBACK(on_show_date_changed), state);
    g_signal_connect(
        state->first_day, "notify::selected",
        G_CALLBACK(on_first_day_changed), state);

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
    const InfiltratrProjectInfo *info = ss_project_info();
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *icon = gtk_image_new_from_icon_name("preferences-system-symbolic");
    GtkWidget *identity = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_widget_add_css_class(header, "titlebar-shell");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_box_append(GTK_BOX(header), icon);
    gtk_box_append(GTK_BOX(identity),
                   make_label(info->program_name, "app-title"));
    gtk_box_append(GTK_BOX(identity),
                   make_label("One place for system-wide preferences",
                              "app-subtitle"));
    gtk_box_append(GTK_BOX(header), identity);
    return header;
}

static void settings_window_free(gpointer data)
{
    SettingsWindow *state = data;

    if (state == NULL) {
        return;
    }
    if (state->timer_id != 0U) {
        g_source_remove(state->timer_id);
        state->timer_id = 0U;
    }
    if (state->location_search_cancellable != NULL) {
        g_cancellable_cancel(state->location_search_cancellable);
        g_clear_object(&state->location_search_cancellable);
    }
    if (state->system_time_cancellable != NULL) {
        g_cancellable_cancel(state->system_time_cancellable);
        g_clear_object(&state->system_time_cancellable);
    }
    g_clear_pointer(&state->timezone_ids, g_ptr_array_unref);
    ss_system_time_service_free(state->system_time_service);
    state->system_time_service = NULL;
    if (state->cinnamon_interface_settings != NULL) {
        g_object_unref(state->cinnamon_interface_settings);
        state->cinnamon_interface_settings = NULL;
    }
    g_free(state);
}

static gboolean on_close_request(GtkWindow *window, gpointer user_data)
{
    SettingsWindow *state = user_data;

    (void)window;
    if (state != NULL && state->timer_id != 0U) {
        g_source_remove(state->timer_id);
        state->timer_id = 0U;
    }
    if (state != NULL && state->location_search_cancellable != NULL) {
        g_cancellable_cancel(state->location_search_cancellable);
    }
    if (state != NULL && state->system_time_cancellable != NULL) {
        g_cancellable_cancel(state->system_time_cancellable);
    }
    return FALSE;
}

static void on_activate(GtkApplication *application, gpointer user_data)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    SettingsWindow *state = g_new0(SettingsWindow, 1);
    g_autoptr(GError) system_time_error = NULL;
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

    (void)ss_regional_context_detect(&state->regional_context);
    state->location_metadata_present =
        ss_location_metadata_load(&state->location_metadata);

    state->system_time_service =
        ss_system_time_service_new(&system_time_error);
    state->system_time_cancellable = g_cancellable_new();

    state->cinnamon_interface_settings =
        ss_cinnamon_interface_settings_new();
    if (state->cinnamon_interface_settings != NULL) {
        g_signal_connect(
            state->cinnamon_interface_settings,
            "changed",
            G_CALLBACK(on_cinnamon_interface_changed),
            state);
    }

    install_common_theme();

    state->window = GTK_WINDOW(
        gtk_application_window_new(application));
    gtk_window_set_title(state->window, info->program_name);
    gtk_window_set_default_size(state->window, 1120, 820);
    gtk_window_set_resizable(state->window, TRUE);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(state->window, root);
    gtk_box_append(GTK_BOX(root), build_header());

    body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(body, TRUE);
    gtk_box_append(GTK_BOX(root), body);
    gtk_box_append(GTK_BOX(body), build_sidebar());

    scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroller),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(scroller),
        build_date_time_panel(state));
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_box_append(GTK_BOX(body), scroller);

    if (state->system_time_service != NULL) {
        ss_system_time_service_set_changed_callback(
            state->system_time_service,
            system_time_changed,
            state);
    }

    sync_controls(state);
    if (state->system_time_service == NULL) {
        set_status(
            state,
            system_time_error != NULL
                ? system_time_error->message
                : "The operating-system date/time service is unavailable.",
            true);
    } else {
        set_status(
            state,
            state->model.persisted_policy_present
                ? "Using the saved system-wide temporal policy. Native Mint/Linux date and time controls are live."
                : "Using Mint/Cinnamon temporal preferences as the initial policy. Native Mint/Linux date and time controls are live.",
            false);
    }

    state->timer_id =
        g_timeout_add_seconds(1U, refresh_preview, state);
    g_signal_connect(
        state->window, "close-request",
        G_CALLBACK(on_close_request), state);
    g_object_set_data_full(
        G_OBJECT(state->window),
        "system-settings-state",
        state,
        settings_window_free);
    gtk_window_present(state->window);
}

int main(int argc, char **argv)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    g_autoptr(GtkApplication) application =
        gtk_application_new(info->application_id,
                            G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(application, "activate", G_CALLBACK(on_activate), NULL);
    return g_application_run(G_APPLICATION(application), argc, argv);
}
