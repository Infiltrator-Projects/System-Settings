// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_DATE_TIME_PANEL_PRIVATE_H
#define SYSTEM_SETTINGS_LINUX_DATE_TIME_PANEL_PRIVATE_H

#ifdef SYSTEM_SETTINGS_DATE_TIME_PREVIEW_BOUNDARY
#error "Preview must use linux-date-time-preview-private.h, not the complete Date & Time panel controller."
#endif

#ifdef SYSTEM_SETTINGS_DATE_TIME_LOCATION_BOUNDARY
#error "Locality must use linux-date-time-location-private.h, not the complete Date & Time panel controller."
#endif

#ifdef SYSTEM_SETTINGS_DATE_TIME_SYSTEM_TIME_BOUNDARY
#error "System time must use linux-date-time-system-time-private.h, not the complete Date & Time panel controller."
#endif

#include "linux-date-time-panel.h"
#include "linux-date-time-location-private.h"
#include "linux-date-time-preview-private.h"
#include "linux-date-time-system-time-private.h"
#include "policy-file-observer.h"

#include "system-settings/date-time-model.h"
#include "system-settings/regional-context.h"
#include "system-settings/system-time-service.h"

#include <gio/gio.h>
#include <gtk/gtk.h>

/* Controller-only lifetime operations are intentionally not exposed to locality. */
SsLinuxDateTimeLifetime *ss_linux_date_time_lifetime_new(
    SsLinuxDateTimePanel *panel);
void ss_linux_date_time_lifetime_invalidate(
    SsLinuxDateTimeLifetime *lifetime);

/*
 * The complete controller is visible only to composition/controller units.
 * Locality has its own compiler-visible private boundary and cannot include
 * this header when built as linux-date-time-location.c.
 */

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
    bool native_format_updating;
};

GtkWidget *ss_linux_date_time_panel_build_ui(
    SsLinuxDateTimePanel *state);

/* Internal controller services shared by non-locality Date & Time units. */
void ss_linux_date_time_panel_set_status(
    SsLinuxDateTimePanel *state,
    const char *message,
    bool error);
void ss_linux_date_time_panel_refresh_location_authority_state(
    SsLinuxDateTimePanel *state);
void ss_linux_date_time_system_time_sync(
    SsLinuxDateTimePanel *state);
void ss_linux_date_time_system_time_start(
    SsLinuxDateTimePanel *state);
void ss_linux_date_time_system_time_dispose(
    SsLinuxDateTimePanel *state);
void ss_linux_date_time_system_time_invalidate_manual_draft(
    SsLinuxDateTimePanel *state);
void ss_linux_date_time_panel_sync_controls(
    SsLinuxDateTimePanel *state);
void ss_linux_date_time_panel_policy_saved(
    SsLinuxDateTimePanel *state);
bool ss_linux_date_time_panel_read_system_time(
    const SsLinuxDateTimePanel *state,
    SsSystemTimeState *out);
bool ss_linux_date_time_panel_desktop_uses_24h(
    const SsLinuxDateTimePanel *state);

void on_clock_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_calendar_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_seconds_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_show_date_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_first_day_changed(GObject *object, GParamSpec *pspec, gpointer user_data);

#endif
