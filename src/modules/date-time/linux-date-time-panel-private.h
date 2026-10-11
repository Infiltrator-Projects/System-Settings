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

typedef struct SsLinuxDateTimeLifetime SsLinuxDateTimeLifetime;

SsLinuxDateTimeLifetime *ss_linux_date_time_lifetime_new(
    SsLinuxDateTimePanel *panel);
SsLinuxDateTimeLifetime *ss_linux_date_time_lifetime_ref(
    SsLinuxDateTimeLifetime *lifetime);
void ss_linux_date_time_lifetime_invalidate(
    SsLinuxDateTimeLifetime *lifetime);
void ss_linux_date_time_lifetime_unref(
    SsLinuxDateTimeLifetime *lifetime);
SsLinuxDateTimePanel *ss_linux_date_time_lifetime_get_panel(
    SsLinuxDateTimeLifetime *lifetime);

/*
 * Mutable state is grouped by the domain that owns it. The panel remains
 * the composition root, but locality code no longer shares flat widget or
 * protected-system-time fields with unrelated presentation concerns.
 */
typedef struct SsLinuxDateTimeLocationState {
    GtkEntry *search_entry;
    GtkButton *search_button;
    GtkListBox *results;
    GtkWidget *summary;
    GtkSpinButton *latitude;
    GtkSpinButton *longitude;

    SsLocationMetadata metadata;
    SsPolicyFileObserver *metadata_observer;
    GCancellable *search_cancellable;
    guint coordinate_commit_id;
    guint recovery_retry_id;
    guint recovery_retry_seconds;
    guint search_generation;
    bool metadata_present;
    bool metadata_uncertain;
    bool follows_timezone_reference;
    bool recovery_failed;
} SsLinuxDateTimeLocationState;

typedef struct SsLinuxDateTimeSystemTimeState {
    GtkWidget *overview_timezone;
    GtkWidget *overview_sync;
    GtkDropDown *timezone;
    GtkSwitch *network_time;
    GtkEntry *manual_date;
    GtkEntry *manual_time;
    GtkButton *manual_set_time;
    GtkWidget *manual_row;
    GtkWidget *manual_unavailable;

    SsSystemTimeService *service;
    GPtrArray *timezone_ids;
    GCancellable *service_cancellable;
    GCancellable *timezone_cancellable;
    GCancellable *ntp_cancellable;
    GCancellable *manual_time_cancellable;
    guint timezone_generation;
    guint ntp_generation;
    guint manual_time_generation;
    bool updating_controls;
    bool manual_dirty;
} SsLinuxDateTimeSystemTimeState;

typedef struct SsLinuxDateTimePreviewState {
    GtkWidget *clock_label;
    GtkWidget *date_label;
    GtkWidget *overview_clock;
    GtkWidget *overview_calendar;

    SsCalendarPreviewProvider *calendar_provider;
    guint timer_id;
    guint idle_id;
    gint date_year;
    gint date_month;
    gint date_day;
    gchar *date_calendar;
    bool date_cache_valid;
} SsLinuxDateTimePreviewState;

struct SsLinuxDateTimePanel {
    GtkWindow *window;
    GtkWidget *root;
    GtkDropDown *clock_mode;
    GtkDropDown *calendar;
    GtkSwitch *show_seconds;
    GtkSwitch *show_date;
    GtkDropDown *first_day;
    GtkWidget *status_label;

    GSettings *cinnamon_interface_settings;
    SsDateTimeModel model;
    SsRegionalContext regional_context;
    SsPolicyFileObserver *policy_observer;
    GPtrArray *clock_mode_ids;
    SsLinuxDateTimeLifetime *lifetime;

    SsLinuxDateTimeLocationState location;
    SsLinuxDateTimeSystemTimeState system_time;
    SsLinuxDateTimePreviewState preview;

    bool updating_controls;
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
