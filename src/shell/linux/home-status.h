// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_HOME_STATUS_H
#define SYSTEM_SETTINGS_HOME_STATUS_H

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
