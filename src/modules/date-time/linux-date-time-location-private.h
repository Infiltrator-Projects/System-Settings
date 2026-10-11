// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_DATE_TIME_LOCATION_PRIVATE_H
#define SYSTEM_SETTINGS_LINUX_DATE_TIME_LOCATION_PRIVATE_H

#include "linux-date-time-panel.h"
#include "policy-file-observer.h"

#include "system-settings/date-time-model.h"
#include "system-settings/location-metadata.h"
#include "system-settings/regional-context.h"
#include "system-settings/system-time-service.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <stdbool.h>

/*
 * Locality is a bounded Date & Time sub-domain. Its implementation may mutate
 * only this state plus the explicitly exported model/regional services below.
 * The complete panel controller intentionally remains opaque here.
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
    bool updating_controls;
} SsLinuxDateTimeLocationState;

typedef struct SsLinuxDateTimeLifetime SsLinuxDateTimeLifetime;

/* Compiler-visible locality facade. */
SsLinuxDateTimeLocationState *ss_linux_date_time_location_state(
    SsLinuxDateTimePanel *panel);
const SsLinuxDateTimeLocationState *ss_linux_date_time_location_state_const(
    const SsLinuxDateTimePanel *panel);
SsDateTimeModel *ss_linux_date_time_location_model(
    SsLinuxDateTimePanel *panel);
const SsDateTimeModel *ss_linux_date_time_location_model_const(
    const SsLinuxDateTimePanel *panel);
SsRegionalContext *ss_linux_date_time_location_regional_context(
    SsLinuxDateTimePanel *panel);
const SsRegionalContext *ss_linux_date_time_location_regional_context_const(
    const SsLinuxDateTimePanel *panel);
SsLinuxDateTimeLifetime *ss_linux_date_time_location_lifetime(
    SsLinuxDateTimePanel *panel);

/* Explicit asynchronous lifetime contract shared with locality requests. */
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

/* Narrow composition services locality is allowed to call. */
void ss_linux_date_time_panel_set_status(
    SsLinuxDateTimePanel *state,
    const char *message,
    bool error);
void ss_linux_date_time_panel_sync_controls(
    SsLinuxDateTimePanel *state);
void ss_linux_date_time_panel_policy_saved(
    SsLinuxDateTimePanel *state);
bool ss_linux_date_time_panel_read_system_time(
    const SsLinuxDateTimePanel *state,
    SsSystemTimeState *out);

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

void on_location_result_activated(
    GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
void on_location_search_clicked(GtkButton *button, gpointer user_data);
void on_location_search_activate(GtkEntry *entry, gpointer user_data);
void on_location_coordinate_changed(
    GObject *object, GParamSpec *pspec, gpointer user_data);

#endif
