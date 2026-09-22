// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file linux-date-time-panel-ui.c
 * @brief GTK construction for the built-in Linux Date & Time module.
 *
 * Widget construction is kept separate from the module's policy,
 * reconciliation and asynchronous backend lifecycle so the behavioural unit
 * remains reviewable as the Date & Time surface grows.
 */

#include "linux-date-time-panel-private.h"
#include "linux-ui-helpers.h"

#include "system-settings/native-clock-policy.h"
#include "system-settings/regional-context.h"

#include <infiltratr/temporal.h>

static GtkStringList *clock_mode_strings(
    SsLinuxDateTimePanel *state)
{
    GtkStringList *list = gtk_string_list_new(NULL);
    size_t index;

    if (state == NULL) {
        return list;
    }

    g_clear_pointer(&state->clock_mode_ids, g_ptr_array_unref);
    state->clock_mode_ids = g_ptr_array_new_with_free_func(g_free);

    for (index = 0U; index < infiltratr_temporal_clock_mode_count(); ++index) {
        const InfiltratrTemporalClockModeInfo *info =
            infiltratr_temporal_clock_mode_at(index);

        /*
         * "standard" remains an internal Common/bootstrap identifier so
         * consumers can run without System Settings. Once System Settings is
         * present it is not a user-facing third conventional clock choice:
         * the explicit 12-hour and 24-hour systems are the settings authority.
         */
        if (info != NULL &&
            !ss_native_clock_mode_is_legacy_default(info->id)) {
            g_ptr_array_add(state->clock_mode_ids, g_strdup(info->id));
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

static GtkStringList *timezone_strings(SsLinuxDateTimePanel *state)
{
    GtkStringList *strings = gtk_string_list_new(NULL);
    size_t index;

    if (state == NULL) {
        return strings;
    }

    g_clear_pointer(&state->timezone_ids, g_ptr_array_unref);
    state->timezone_ids = ss_regional_context_list_timezones(
        state->regional_context.timezone_id);

    if (state->timezone_ids == NULL) {
        state->timezone_ids =
            g_ptr_array_new_with_free_func(g_free);
    }

    for (index = 0U; index < state->timezone_ids->len; ++index) {
        const char *zone = g_ptr_array_index(
            state->timezone_ids, (guint)index);
        gtk_string_list_append(strings, zone);
    }
    return strings;
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

GtkWidget *ss_linux_date_time_panel_build_ui(SsLinuxDateTimePanel *state)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    GtkWidget *summary = ss_linux_ui_make_label(
        "System Settings is the authoritative Date & Time frontend. It writes ordinary Mint/Linux settings through their native interfaces and adds richer Common-aware clock, calendar and geographic policy without maintaining a second copy of native system state.",
        "page-summary");
    GtkWidget *preview_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *location_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *clock_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *calendar_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *system_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *format_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *manual_box;
    GtkWidget *search_box;
    GtkStringList *strings;
    GtkExpression *expression;

    gtk_widget_add_css_class(page, "settings-content");
    gtk_box_append(GTK_BOX(page), ss_linux_ui_make_label("Date & Time", "page-title"));
    gtk_label_set_wrap(GTK_LABEL(summary), TRUE);
    gtk_box_append(GTK_BOX(page), summary);

    gtk_widget_add_css_class(preview_card, "preview-card");
    state->clock_preview = ss_linux_ui_make_label("--:--", "preview-time");
    state->date_preview = ss_linux_ui_make_label("", "preview-date");
    gtk_label_set_wrap(GTK_LABEL(state->clock_preview), TRUE);
    gtk_label_set_wrap(GTK_LABEL(state->date_preview), TRUE);
    gtk_box_append(GTK_BOX(preview_card), state->clock_preview);
    gtk_box_append(GTK_BOX(preview_card), state->date_preview);
    gtk_box_append(GTK_BOX(page), preview_card);

    /*
     * Locality, coordinates and time zone are one coherent settings family.
     * A named locality may propose the matching IANA zone, while the explicit
     * zone selector remains visible for correction. If no precise locality has
     * been selected, the system zone's tzdata reference follows zone changes.
     */
    gtk_widget_add_css_class(location_card, "settings-card");
    gtk_widget_add_css_class(location_card, "location-card");
    gtk_box_append(
        GTK_BOX(location_card),
        ss_linux_ui_make_label("Location & time zone", "section-title"));

    state->location_summary = ss_linux_ui_make_label("", "accent-note");
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
        ss_linux_ui_make_setting_row(
            "Locality",
            "Search by town, suburb, city or place name. Selecting a result stores its coordinates and applies the nearest matching IANA time zone to Linux.",
            search_box));

    state->location_results = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_results), "location-results");
    gtk_widget_set_visible(
        GTK_WIDGET(state->location_results), FALSE);
    gtk_box_append(
        GTK_BOX(location_card),
        GTK_WIDGET(state->location_results));

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
        GTK_BOX(location_card),
        ss_linux_ui_make_setting_row(
            "Time zone",
            "The real operating-system IANA zone. Locality selection normally chooses it automatically; it remains editable for correction or deliberate overrides.",
            GTK_WIDGET(state->timezone)));

    state->latitude = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(-90.0, 90.0, 0.0001));
    gtk_widget_add_css_class(
        GTK_WIDGET(state->latitude), "setting-spin");
    gtk_spin_button_set_digits(state->latitude, 4U);
    gtk_box_append(
        GTK_BOX(location_card),
        ss_linux_ui_make_setting_row(
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
        ss_linux_ui_make_setting_row(
            "Longitude",
            "Advanced coordinate override. Degrees east of Greenwich are positive; degrees west are negative.",
            GTK_WIDGET(state->longitude)));
    gtk_box_append(GTK_BOX(page), location_card);

    gtk_widget_add_css_class(clock_card, "settings-card");
    gtk_widget_add_css_class(clock_card, "clock-card");
    gtk_box_append(
        GTK_BOX(clock_card),
        ss_linux_ui_make_label("Clock system", "section-title"));

    strings = clock_mode_strings(state);
    state->clock_mode = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    g_object_unref(strings);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->clock_mode), "setting-dropdown");
    gtk_widget_set_size_request(
        GTK_WIDGET(state->clock_mode), 360, -1);
    gtk_box_append(
        GTK_BOX(clock_card),
        ss_linux_ui_make_setting_row(
            "System clock",
            "Choose the human clock representation used by Common-aware applications. Standard time follows the native desktop's 12/24-hour preference below.",
            GTK_WIDGET(state->clock_mode)));

    state->show_seconds = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->show_seconds), "setting-switch");
    gtk_box_append(
        GTK_BOX(clock_card),
        ss_linux_ui_make_setting_row(
            "Show seconds",
            "Show seconds or the closest finer unit supported by the selected clock system. The equivalent Cinnamon panel preference is kept aligned.",
            GTK_WIDGET(state->show_seconds)));
    gtk_box_append(GTK_BOX(page), clock_card);

    gtk_widget_add_css_class(calendar_card, "settings-card");
    gtk_widget_add_css_class(calendar_card, "calendar-card");
    gtk_box_append(
        GTK_BOX(calendar_card),
        ss_linux_ui_make_label("Calendar system", "section-title"));

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
        ss_linux_ui_make_setting_row(
            "Calendar",
            "Choose the calendar system used by Common-aware applications. Gregorian remains the ordinary Mint/Linux calendar.",
            GTK_WIDGET(state->calendar)));
    gtk_box_append(GTK_BOX(page), calendar_card);

    /*
     * Network/manual clock source belongs after the user's presentation
     * choices. When NTP is enabled the manual controls do not merely become
     * insensitive: they disappear because they are not an active source.
     */
    gtk_widget_add_css_class(system_card, "settings-card");
    gtk_widget_add_css_class(system_card, "system-card");
    gtk_box_append(
        GTK_BOX(system_card),
        ss_linux_ui_make_label("System time", "section-title"));

    state->network_time = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->network_time), "setting-switch");
    gtk_box_append(
        GTK_BOX(system_card),
        ss_linux_ui_make_setting_row(
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
        state->manual_time, "Selected clock time");
    gtk_entry_set_max_length(state->manual_date, 10);
    gtk_entry_set_max_length(state->manual_time, 24);
    gtk_widget_set_size_request(
        GTK_WIDGET(state->manual_date), 130, -1);
    gtk_widget_set_size_request(
        GTK_WIDGET(state->manual_time), 150, -1);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_date), "setting-entry");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_time), "setting-entry");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_set_time), "setting-button");
    gtk_box_append(
        GTK_BOX(manual_box), GTK_WIDGET(state->manual_date));
    gtk_box_append(
        GTK_BOX(manual_box), GTK_WIDGET(state->manual_time));
    gtk_box_append(
        GTK_BOX(manual_box), GTK_WIDGET(state->manual_set_time));

    state->manual_row = ss_linux_ui_make_setting_row(
        "Manual date and time",
        "Shown only when Network time is off. The time entry follows the selected clock system above; the current reversible editor supports Gregorian dates with Standard, 12-hour, 24-hour and French Republican decimal time.",
        manual_box);
    gtk_box_append(GTK_BOX(system_card), state->manual_row);

    state->manual_unavailable = ss_linux_ui_make_label(
        "Manual setting is hidden for this clock/calendar combination because System Settings will not reinterpret a presentation it cannot safely convert back to one canonical system instant.",
        "accent-note");
    gtk_label_set_wrap(
        GTK_LABEL(state->manual_unavailable), TRUE);
    gtk_widget_set_visible(state->manual_unavailable, FALSE);
    gtk_box_append(
        GTK_BOX(system_card), state->manual_unavailable);

    state->status_label = ss_linux_ui_make_label("", "status-ok");
    gtk_label_set_wrap(GTK_LABEL(state->status_label), TRUE);
    gtk_box_append(GTK_BOX(system_card), state->status_label);
    gtk_box_append(GTK_BOX(page), system_card);

    /*
     * These are ordinary Cinnamon authorities, not copies in the Infiltrator
     * policy. Writing them here has exactly the same system effect as Mint's
     * Date & Time format controls.
     */
    gtk_widget_add_css_class(format_card, "settings-card");
    gtk_box_append(
        GTK_BOX(format_card),
        ss_linux_ui_make_label("Desktop format", "section-title"));

    state->show_date = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->show_date), "setting-switch");
    gtk_box_append(
        GTK_BOX(format_card),
        ss_linux_ui_make_setting_row(
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
        ss_linux_ui_make_setting_row(
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
        state->show_date, "notify::active",
        G_CALLBACK(on_show_date_changed), state);
    g_signal_connect(
        state->first_day, "notify::selected",
        G_CALLBACK(on_first_day_changed), state);

    return page;
}


