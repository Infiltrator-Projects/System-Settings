// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_DATE_TIME_PANEL_PRIVATE_H
#define SYSTEM_SETTINGS_LINUX_DATE_TIME_PANEL_PRIVATE_H

#include "linux-date-time-panel.h"
#include "calendar-preview-provider.h"

#include "system-settings/date-time-model.h"
#include "system-settings/location-metadata.h"
#include "system-settings/regional-context.h"
#include "system-settings/system-time-service.h"

#include <gio/gio.h>
#include <gtk/gtk.h>

struct SsLinuxDateTimePanel {
    GtkWindow *window;
    GtkWidget *root;
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
    GtkWidget *manual_row;
    GtkWidget *manual_unavailable;
    GtkSwitch *show_date;
    GtkDropDown *first_day;
    GtkWidget *status_label;
    GSettings *cinnamon_interface_settings;
    SsDateTimeModel model;
    SsRegionalContext regional_context;
    SsLocationMetadata location_metadata;
    SsSystemTimeService *system_time_service;
    SsCalendarPreviewProvider *calendar_preview_provider;
    GPtrArray *clock_mode_ids;
    GPtrArray *timezone_ids;
    GCancellable *location_search_cancellable;
    GCancellable *system_time_cancellable;
    guint timer_id;
    guint preview_idle_id;
    guint location_search_generation;
    guint system_time_generation;
    bool location_metadata_present;
    bool location_follows_timezone_reference;
    bool updating_controls;
    bool updating_system_controls;
};

GtkWidget *ss_linux_date_time_panel_build_ui(
    SsLinuxDateTimePanel *state);

void on_clock_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_calendar_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_seconds_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_timezone_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_network_time_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_manual_set_time_clicked(GtkButton *button, gpointer user_data);
void on_show_date_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_first_day_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_location_result_activated(
    GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
void on_location_search_clicked(GtkButton *button, gpointer user_data);
void on_location_search_activate(GtkEntry *entry, gpointer user_data);
void on_location_coordinate_changed(
    GObject *object, GParamSpec *pspec, gpointer user_data);

#endif
