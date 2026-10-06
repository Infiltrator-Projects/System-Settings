// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_HOME_PAGE_PRIVATE_H
#define SYSTEM_SETTINGS_HOME_PAGE_PRIVATE_H

#include "builtin-module-host.h"

#include <gtk/gtk.h>

typedef struct HomeTemporalTicker {
    GtkWidget *owner;
    GtkLabel *clock;
    GtkLabel *date;
    GtkLabel *system_time;
    SsBuiltinModuleHost *module_host;
    guint source_id;
    guint interval_ms;
} HomeTemporalTicker;

typedef struct HomeStatusTicker {
    GtkWidget *owner;
    GtkLabel *uptime;
    GtkLabel *date_timezone;
    GtkLabel *region_value;
    GtkLabel *region_detail;
    GtkLabel *appearance_value;
    GtkLabel *appearance_detail;
    GtkLabel *network_value;
    GtkLabel *network_detail;
    guint source_id;
    GtkSettings *settings;
    GNetworkMonitor *network;
    gulong theme_name_id;
    gulong theme_dark_id;
    gulong network_available_id;
    gulong network_metered_id;
    gulong network_connectivity_id;
} HomeStatusTicker;

typedef struct HomeAdaptiveLayout {
    GtkWidget *hero;
    GtkFlowBox *features;
    GtkFlowBox *primary_grid;
    GtkFlowBox *status_grid;
    GtkFlowBox *appearance_previews;
    guint applied_columns;
} HomeAdaptiveLayout;

guint home_layout_columns_for_width(int width);
void home_layout_apply_width(HomeAdaptiveLayout *layout, int width);
GtkWidget *make_visual_panel(
    const char *filename,
    int width,
    int height,
    const char *css_class);

#endif
