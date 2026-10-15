// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_HOME_STATUS_H
#define SYSTEM_SETTINGS_HOME_STATUS_H

#include <stdbool.h>

#include <gtk/gtk.h>

typedef struct SsHomeStatusSnapshot {
    gchar *uptime;
    gchar *timezone;
    gchar *region_value;
    gchar *region_detail;
    gchar *appearance_value;
    gchar *appearance_detail;
    gchar *network_value;
    gchar *network_detail;
} SsHomeStatusSnapshot;

/*
 * Parse the first kernel uptime field without consulting the process locale.
 * Exposed only inside the Linux shell so the machine-text boundary can carry a
 * focused regression without making Home's presentation state public API.
 */
bool ss_home_status_parse_uptime_seconds(
    const char *contents,
    guint64 *seconds_out);

void ss_home_status_snapshot_init(SsHomeStatusSnapshot *snapshot);
void ss_home_status_snapshot_clear(SsHomeStatusSnapshot *snapshot);
void ss_home_status_snapshot_read(SsHomeStatusSnapshot *snapshot);

void ss_home_status_attach(
    GtkWidget *owner,
    GtkLabel *uptime,
    GtkLabel *date_timezone,
    GtkLabel *region_value,
    GtkLabel *region_detail,
    GtkLabel *appearance_value,
    GtkLabel *appearance_detail,
    GtkLabel *network_value,
    GtkLabel *network_detail);

#endif
