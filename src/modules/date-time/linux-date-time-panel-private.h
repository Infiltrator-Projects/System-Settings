// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_DATE_TIME_PANEL_PRIVATE_H
#define SYSTEM_SETTINGS_LINUX_DATE_TIME_PANEL_PRIVATE_H

#include "linux-date-time-panel.h"
#include "calendar-preview-provider.h"
#include "policy-file-observer.h"

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
    GtkWidget *overview_clock;
    GtkWidget *overview_calendar;
    GtkWidget *overview_timezone;
    GtkWidget *overview_sync;
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
    SsPolicyFileObserver *policy_observer;
    SsPolicyFileObserver *location_metadata_observer;
    GPtrArray *clock_mode_ids;
    GPtrArray *timezone_ids;
    GCancellable *location_search_cancellable;
    GCancellable *service_cancellable;
    GCancellable *timezone_cancellable;
    GCancellable *ntp_cancellable;
    GCancellable *manual_time_cancellable;
    guint timer_id;
    guint preview_idle_id;
    guint coordinate_commit_id;
    guint locality_recovery_retry_id;
    guint locality_recovery_retry_seconds;
    guint location_search_generation;
    guint timezone_generation;
    guint ntp_generation;
    guint manual_time_generation;
    gint preview_date_year;
    gint preview_date_month;
    gint preview_date_day;
    gchar *preview_date_calendar;
    bool preview_date_cache_valid;
    bool location_metadata_present;
    bool location_metadata_uncertain;
    bool location_follows_timezone_reference;
    bool updating_controls;
    bool updating_system_controls;
    bool manual_dirty;
    bool locality_recovery_failed;
};

GtkWidget *ss_linux_date_time_panel_build_ui(
    SsLinuxDateTimePanel *state);

/* Internal controller services shared by split Date & Time translation units. */
void ss_linux_date_time_panel_set_status(
    SsLinuxDateTimePanel *state,
    const char *message,
    bool error);
void ss_linux_date_time_panel_sync_controls(
    SsLinuxDateTimePanel *state);
void ss_linux_date_time_panel_policy_saved(
    SsLinuxDateTimePanel *state);
guint ss_linux_date_time_panel_preview_interval_ms(
    const SsLinuxDateTimePanel *state);

bool ss_linux_date_time_location_coordinate_close(
    double left,
    double right);
bool ss_linux_date_time_location_metadata_matches_policy(
    const SsLinuxDateTimePanel *state,
    const InfiltratrTemporalPolicyV3 *policy);
bool ss_linux_date_time_location_policy_matches_reference(
    const SsLinuxDateTimePanel *state);
void ss_linux_date_time_location_update_summary(
    SsLinuxDateTimePanel *state);
void ss_linux_date_time_location_cancel_coordinate_commit(
    SsLinuxDateTimePanel *state);

void on_clock_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_calendar_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_seconds_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_timezone_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_network_time_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_manual_set_time_clicked(GtkButton *button, gpointer user_data);
void on_manual_entry_changed(GtkEditable *editable, gpointer user_data);
void on_show_date_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_first_day_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_location_result_activated(
    GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
void on_location_search_clicked(GtkButton *button, gpointer user_data);
void on_location_search_activate(GtkEntry *entry, gpointer user_data);
void on_location_coordinate_changed(
    GObject *object, GParamSpec *pspec, gpointer user_data);

#endif
