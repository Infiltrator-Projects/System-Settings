// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_DATE_TIME_SYSTEM_TIME_PRIVATE_H
#define SYSTEM_SETTINGS_LINUX_DATE_TIME_SYSTEM_TIME_PRIVATE_H

#include "linux-date-time-panel.h"

#include "system-settings/date-time-model.h"
#include "system-settings/system-time-service.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <stdbool.h>

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

typedef struct SsLinuxDateTimeLifetime SsLinuxDateTimeLifetime;

SsLinuxDateTimeSystemTimeState *ss_linux_date_time_system_time_state(
    SsLinuxDateTimePanel *panel);
const SsLinuxDateTimeSystemTimeState *ss_linux_date_time_system_time_state_const(
    const SsLinuxDateTimePanel *panel);
const InfiltratrTemporalPolicyV3 *ss_linux_date_time_system_time_policy(
    const SsLinuxDateTimePanel *panel);
SsLinuxDateTimeLifetime *ss_linux_date_time_system_time_lifetime(
    SsLinuxDateTimePanel *panel);

SsLinuxDateTimeLifetime *ss_linux_date_time_lifetime_ref(
    SsLinuxDateTimeLifetime *lifetime);
void ss_linux_date_time_lifetime_unref(
    SsLinuxDateTimeLifetime *lifetime);
SsLinuxDateTimePanel *ss_linux_date_time_lifetime_get_panel(
    SsLinuxDateTimeLifetime *lifetime);

void ss_linux_date_time_panel_set_status(
    SsLinuxDateTimePanel *state, const char *message, bool error);
bool ss_linux_date_time_panel_desktop_uses_24h(
    const SsLinuxDateTimePanel *state);
void ss_linux_date_time_panel_reconcile_system_time_change(
    SsLinuxDateTimePanel *state, const SsSystemTimeState *system_state);
bool ss_linux_date_time_panel_system_time_operation_applied(
    SsLinuxDateTimePanel *state, bool timezone_operation,
    const char *observed_timezone);
void ss_linux_date_time_panel_system_time_service_ready(
    SsLinuxDateTimePanel *state);

bool ss_linux_date_time_panel_read_system_time(
    const SsLinuxDateTimePanel *state, SsSystemTimeState *out);
void ss_linux_date_time_system_time_sync(SsLinuxDateTimePanel *state);
void ss_linux_date_time_system_time_start(SsLinuxDateTimePanel *state);
void ss_linux_date_time_system_time_dispose(SsLinuxDateTimePanel *state);
void ss_linux_date_time_system_time_invalidate_manual_draft(
    SsLinuxDateTimePanel *state);

void on_timezone_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_network_time_changed(GObject *object, GParamSpec *pspec, gpointer user_data);
void on_manual_set_time_clicked(GtkButton *button, gpointer user_data);
void on_manual_entry_changed(GtkEditable *editable, gpointer user_data);

#endif
