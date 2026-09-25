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

static GtkWidget *make_section_heading(const char *icon_name,
                                       const char *title,
                                       const char *summary)
{
    GtkWidget *heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *subtitle = ss_linux_ui_make_label(summary, "section-summary");

    gtk_widget_add_css_class(heading, "section-heading");
    gtk_widget_add_css_class(icon_wrap, "section-icon-wrap");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 22);
    gtk_box_append(GTK_BOX(icon_wrap), icon);
    gtk_box_append(GTK_BOX(heading), icon_wrap);
    gtk_box_append(
        GTK_BOX(copy),
        ss_linux_ui_make_label(title, "section-title"));
    gtk_label_set_wrap(GTK_LABEL(subtitle), TRUE);
    gtk_box_append(GTK_BOX(copy), subtitle);
    gtk_box_append(GTK_BOX(heading), copy);
    return heading;
}

static GtkWidget *make_coordinate_field(const char *caption,
                                        GtkSpinButton *spin)
{
    GtkWidget *field = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    gtk_box_append(
        GTK_BOX(field),
        ss_linux_ui_make_label(caption, "field-caption"));
    gtk_box_append(GTK_BOX(field), GTK_WIDGET(spin));
    return field;
}

static GtkWidget *make_manual_setting_block(
    SsLinuxDateTimePanel *state,
    GtkWidget *controls)
{
    GtkWidget *block = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
    GtkWidget *description = ss_linux_ui_make_label(
        "Available when Network time is off.",
        "setting-description");

    gtk_widget_add_css_class(block, "setting-row");
    gtk_box_append(
        GTK_BOX(block),
        ss_linux_ui_make_label("Manual date and time", "setting-label"));
    gtk_label_set_wrap(GTK_LABEL(description), TRUE);
    gtk_box_append(GTK_BOX(block), description);
    gtk_box_append(GTK_BOX(block), controls);
    state->manual_row = block;
    return block;
}

GtkWidget *ss_linux_date_time_panel_build_ui(SsLinuxDateTimePanel *state)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
    GtkWidget *page_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GtkWidget *page_icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *page_icon = gtk_image_new_from_icon_name(
        "preferences-system-time-symbolic");
    GtkWidget *page_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *summary = ss_linux_ui_make_label(
        "Clock, calendar, location and system time — live, visual and immediate.",
        "page-summary");
    GtkWidget *hero_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *hero_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *hero_copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *hero_badge = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
    GtkWidget *hero_badge_icon = gtk_image_new_from_icon_name(
        "media-playback-start-symbolic");
    GtkWidget *location_strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *location_strip_icon = gtk_image_new_from_icon_name(
        "mark-location-symbolic");
    GtkWidget *location_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *presentation_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *system_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *lower = gtk_flow_box_new();
    GtkWidget *manual_box;
    GtkWidget *search_box;
    GtkWidget *coordinate_box;
    GtkStringList *strings;
    GtkExpression *expression;

    gtk_widget_add_css_class(page, "settings-content");
    gtk_widget_set_hexpand(page, TRUE);

    gtk_widget_add_css_class(page_header, "page-header");
    gtk_widget_add_css_class(page_icon_wrap, "page-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(page_icon), 28);
    gtk_box_append(GTK_BOX(page_icon_wrap), page_icon);
    gtk_box_append(GTK_BOX(page_header), page_icon_wrap);
    gtk_widget_set_hexpand(page_copy, TRUE);
    gtk_box_append(
        GTK_BOX(page_copy),
        ss_linux_ui_make_label("SYSTEM / DATE & TIME", "page-eyebrow"));
    gtk_box_append(
        GTK_BOX(page_copy),
        ss_linux_ui_make_label("Date & Time", "page-title"));
    gtk_label_set_wrap(GTK_LABEL(summary), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(summary), 92);
    gtk_box_append(GTK_BOX(page_copy), summary);
    gtk_box_append(GTK_BOX(page_header), page_copy);
    gtk_box_append(GTK_BOX(page), page_header);

    /*
     * One dominant preview gives the page a visual anchor. Everything below
     * edits the values represented here, avoiding the former stack of equally
     * prominent coloured cards.
     */
    gtk_widget_add_css_class(hero_card, "hero-card");
    gtk_widget_add_css_class(hero_top, "hero-top");
    gtk_widget_set_hexpand(hero_copy, TRUE);
    gtk_box_append(
        GTK_BOX(hero_copy),
        ss_linux_ui_make_label("LIVE PRESENTATION", "hero-kicker"));
    state->clock_preview = ss_linux_ui_make_label("--:--", "preview-time");
    state->date_preview = ss_linux_ui_make_label("", "preview-date");
    gtk_label_set_wrap(GTK_LABEL(state->clock_preview), TRUE);
    gtk_label_set_wrap(GTK_LABEL(state->date_preview), TRUE);
    gtk_box_append(GTK_BOX(hero_copy), state->clock_preview);
    gtk_box_append(GTK_BOX(hero_copy), state->date_preview);
    gtk_box_append(GTK_BOX(hero_top), hero_copy);

    gtk_widget_add_css_class(hero_badge, "hero-badge");
    gtk_image_set_pixel_size(GTK_IMAGE(hero_badge_icon), 13);
    gtk_box_append(GTK_BOX(hero_badge), hero_badge_icon);
    gtk_box_append(
        GTK_BOX(hero_badge),
        ss_linux_ui_make_label("LIVE", "hero-badge-label"));
    gtk_widget_set_valign(hero_badge, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(hero_top), hero_badge);
    gtk_box_append(GTK_BOX(hero_card), hero_top);

    gtk_box_append(
        GTK_BOX(hero_card),
        ss_linux_ui_make_label(
            "Presentation changes appear here immediately.",
            "hero-note"));
    gtk_box_append(GTK_BOX(page), hero_card);

    /*
     * Location needs the full content width because search results, IANA zone
     * selection and coordinate overrides form one correction workflow.
     */
    gtk_widget_add_css_class(location_card, "settings-card");
    gtk_box_append(
        GTK_BOX(location_card),
        make_section_heading(
            "mark-location-symbolic",
            "Location & time zone",
            "Locality, time zone and coordinates."));

    gtk_widget_add_css_class(location_strip, "info-strip");
    gtk_image_set_pixel_size(GTK_IMAGE(location_strip_icon), 14);
    gtk_box_append(GTK_BOX(location_strip), location_strip_icon);
    state->location_summary = ss_linux_ui_make_label("", "accent-note");
    gtk_label_set_wrap(GTK_LABEL(state->location_summary), TRUE);
    gtk_widget_set_hexpand(state->location_summary, TRUE);
    gtk_box_append(GTK_BOX(location_strip), state->location_summary);
    gtk_box_append(GTK_BOX(location_card), location_strip);

    search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->location_search = GTK_ENTRY(gtk_entry_new());
    state->location_search_button =
        GTK_BUTTON(gtk_button_new_with_label("Search"));
    gtk_entry_set_placeholder_text(
        state->location_search,
        "Town, suburb, city or place");
    gtk_widget_set_hexpand(GTK_WIDGET(state->location_search), TRUE);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_search), "setting-entry");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_search_button), "setting-button");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_search_button), "primary-button");
    gtk_box_append(GTK_BOX(search_box), GTK_WIDGET(state->location_search));
    gtk_box_append(
        GTK_BOX(search_box), GTK_WIDGET(state->location_search_button));
    gtk_box_append(
        GTK_BOX(location_card),
        ss_linux_ui_make_setting_row(
            "Locality",
            "Search by town, suburb, city or place.",
            search_box));

    state->location_results = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->location_results), "location-results");
    gtk_widget_set_visible(GTK_WIDGET(state->location_results), FALSE);
    gtk_box_append(GTK_BOX(location_card), GTK_WIDGET(state->location_results));

    strings = timezone_strings(state);
    state->timezone = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    /* gtk_drop_down_new() consumes the model reference (transfer full). */
    expression = gtk_property_expression_new(
        GTK_TYPE_STRING_OBJECT, NULL, "string");
    gtk_drop_down_set_expression(state->timezone, expression);
    gtk_expression_unref(expression);
    gtk_drop_down_set_enable_search(state->timezone, TRUE);
    gtk_widget_add_css_class(
        GTK_WIDGET(state->timezone), "setting-dropdown");
    gtk_widget_set_size_request(GTK_WIDGET(state->timezone), 250, -1);
    gtk_box_append(
        GTK_BOX(location_card),
        ss_linux_ui_make_setting_row(
            "Time zone",
            "Operating-system IANA time zone.",
            GTK_WIDGET(state->timezone)));

    state->latitude = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(-90.0, 90.0, 0.0001));
    state->longitude = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(-180.0, 180.0, 0.0001));
    gtk_widget_add_css_class(GTK_WIDGET(state->latitude), "setting-spin");
    gtk_widget_add_css_class(GTK_WIDGET(state->longitude), "setting-spin");
    gtk_spin_button_set_digits(state->latitude, 4U);
    gtk_spin_button_set_digits(state->longitude, 4U);
    gtk_widget_set_size_request(GTK_WIDGET(state->latitude), 130, -1);
    gtk_widget_set_size_request(GTK_WIDGET(state->longitude), 130, -1);

    coordinate_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(
        GTK_BOX(coordinate_box),
        make_coordinate_field("LATITUDE", state->latitude));
    gtk_box_append(
        GTK_BOX(coordinate_box),
        make_coordinate_field("LONGITUDE", state->longitude));
    gtk_box_append(
        GTK_BOX(location_card),
        ss_linux_ui_make_setting_row(
            "Coordinates",
            "Advanced decimal-degree override.",
            coordinate_box));
    gtk_box_append(GTK_BOX(page), location_card);

    /*
     * Presentation and system-clock control are peers. GtkFlowBox keeps them
     * side-by-side on an ordinary laptop and naturally wraps them to one
     * column when the window becomes narrow.
     */
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(lower), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(lower), 1U);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(lower), 2U);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(lower), 18U);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(lower), 18U);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(lower), TRUE);
    gtk_widget_set_hexpand(lower, TRUE);

    gtk_widget_add_css_class(presentation_card, "settings-card");
    gtk_widget_set_size_request(presentation_card, 390, -1);
    gtk_box_append(
        GTK_BOX(presentation_card),
        make_section_heading(
            "preferences-desktop-display-symbolic",
            "Presentation",
            "Choose how time and dates look."));

    strings = clock_mode_strings(state);
    state->clock_mode = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    /* gtk_drop_down_new() consumes the model reference (transfer full). */
    gtk_widget_add_css_class(
        GTK_WIDGET(state->clock_mode), "setting-dropdown");
    gtk_widget_set_size_request(GTK_WIDGET(state->clock_mode), 190, -1);
    gtk_box_append(
        GTK_BOX(presentation_card),
        ss_linux_ui_make_setting_row(
            "Clock system",
            "Choose the clock style.",
            GTK_WIDGET(state->clock_mode)));

    strings = calendar_strings();
    state->calendar = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    /* gtk_drop_down_new() consumes the model reference (transfer full). */
    gtk_widget_add_css_class(
        GTK_WIDGET(state->calendar), "setting-dropdown");
    gtk_widget_set_size_request(GTK_WIDGET(state->calendar), 190, -1);
    gtk_box_append(
        GTK_BOX(presentation_card),
        ss_linux_ui_make_setting_row(
            "Calendar",
            "Choose the calendar system.",
            GTK_WIDGET(state->calendar)));

    state->show_seconds = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->show_seconds), "setting-switch");
    gtk_box_append(
        GTK_BOX(presentation_card),
        ss_linux_ui_make_setting_row(
            "Show seconds",
            "Show seconds or the closest finer unit.",
            GTK_WIDGET(state->show_seconds)));

    state->show_date = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->show_date), "setting-switch");
    gtk_box_append(
        GTK_BOX(presentation_card),
        ss_linux_ui_make_setting_row(
            "Panel date",
            "Show the date in the Cinnamon panel.",
            GTK_WIDGET(state->show_date)));

    strings = first_day_strings();
    state->first_day = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(strings), NULL));
    /* gtk_drop_down_new() consumes the model reference (transfer full). */
    gtk_widget_add_css_class(
        GTK_WIDGET(state->first_day), "setting-dropdown");
    gtk_widget_set_size_request(GTK_WIDGET(state->first_day), 180, -1);
    gtk_box_append(
        GTK_BOX(presentation_card),
        ss_linux_ui_make_setting_row(
            "First day of week",
            "Locale, Sunday or Monday.",
            GTK_WIDGET(state->first_day)));

    gtk_widget_add_css_class(system_card, "settings-card");
    gtk_widget_set_size_request(system_card, 390, -1);
    gtk_box_append(
        GTK_BOX(system_card),
        make_section_heading(
            "preferences-system-time-symbolic",
            "System clock",
            "Network sync and manual time."));

    state->network_time = GTK_SWITCH(gtk_switch_new());
    gtk_widget_add_css_class(
        GTK_WIDGET(state->network_time), "setting-switch");
    gtk_box_append(
        GTK_BOX(system_card),
        ss_linux_ui_make_setting_row(
            "Network time",
            "Synchronise automatically with the system service.",
            GTK_WIDGET(state->network_time)));

    manual_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
    state->manual_date = GTK_ENTRY(gtk_entry_new());
    state->manual_time = GTK_ENTRY(gtk_entry_new());
    state->manual_set_time =
        GTK_BUTTON(gtk_button_new_with_label("Set"));
    gtk_entry_set_placeholder_text(state->manual_date, "YYYY-MM-DD");
    gtk_entry_set_placeholder_text(state->manual_time, "Clock time");
    gtk_entry_set_max_length(state->manual_date, 10);
    gtk_entry_set_max_length(state->manual_time, 24);
    gtk_widget_set_size_request(GTK_WIDGET(state->manual_date), 108, -1);
    gtk_widget_set_size_request(GTK_WIDGET(state->manual_time), 128, -1);
    gtk_widget_add_css_class(GTK_WIDGET(state->manual_date), "setting-entry");
    gtk_widget_add_css_class(GTK_WIDGET(state->manual_time), "setting-entry");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_set_time), "setting-button");
    gtk_widget_add_css_class(
        GTK_WIDGET(state->manual_set_time), "primary-button");
    gtk_box_append(GTK_BOX(manual_box), GTK_WIDGET(state->manual_date));
    gtk_box_append(GTK_BOX(manual_box), GTK_WIDGET(state->manual_time));
    gtk_box_append(GTK_BOX(manual_box), GTK_WIDGET(state->manual_set_time));
    gtk_box_append(
        GTK_BOX(system_card),
        make_manual_setting_block(state, manual_box));

    state->manual_unavailable = ss_linux_ui_make_label(
        "Manual setting is unavailable for this clock/calendar combination.",
        "accent-note");
    gtk_label_set_wrap(GTK_LABEL(state->manual_unavailable), TRUE);
    gtk_widget_set_visible(state->manual_unavailable, FALSE);
    gtk_box_append(GTK_BOX(system_card), state->manual_unavailable);

    state->status_label = ss_linux_ui_make_label("", "status-ok");
    gtk_label_set_wrap(GTK_LABEL(state->status_label), TRUE);
    gtk_box_append(GTK_BOX(system_card), state->status_label);

    gtk_flow_box_insert(GTK_FLOW_BOX(lower), presentation_card, -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(lower), system_card, -1);
    gtk_box_append(GTK_BOX(page), lower);

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
