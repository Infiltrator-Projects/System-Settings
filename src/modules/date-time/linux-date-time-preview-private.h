// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_DATE_TIME_PREVIEW_PRIVATE_H
#define SYSTEM_SETTINGS_LINUX_DATE_TIME_PREVIEW_PRIVATE_H

#include <infiltratr/temporal_posix.h>
#include <infiltratr/temporal_clock_resource.h>
#include "linux-date-time-panel.h"

#include "system-settings/date-time-model.h"
#include "system-settings/regional-context.h"

#include <gtk/gtk.h>
#include <stdbool.h>

typedef struct SsLinuxDateTimePreviewState {
    GtkWidget *clock_label;
    GtkWidget *date_label;
    GtkWidget *overview_clock;
    GtkWidget *overview_calendar;

    InfiltratrTemporalDateProvider *calendar_provider;
    InfiltratrTemporalClockPresentation *clock_presentation;
    gchar *clock_presentation_id;
    guint timer_id;
    guint idle_id;
    gint date_year;
    gint date_month;
    gint date_day;
    gchar *date_calendar;
    bool date_cache_valid;
} SsLinuxDateTimePreviewState;

/* Narrow compiler-visible projection used by linux-date-time-preview.c. */
SsLinuxDateTimePreviewState *ss_linux_date_time_preview_state(
    SsLinuxDateTimePanel *panel);
SsDateTimeModel *ss_linux_date_time_preview_model(
    SsLinuxDateTimePanel *panel);
SsRegionalContext *ss_linux_date_time_preview_regional_context(
    SsLinuxDateTimePanel *panel);
GtkWidget *ss_linux_date_time_preview_root(
    SsLinuxDateTimePanel *panel);
bool ss_linux_date_time_preview_desktop_uses_24h(
    const SsLinuxDateTimePanel *panel);

void ss_linux_date_time_preview_init(SsLinuxDateTimePanel *panel);
void ss_linux_date_time_preview_attach(SsLinuxDateTimePanel *panel);
void ss_linux_date_time_preview_schedule_initial(SsLinuxDateTimePanel *panel);
gboolean ss_linux_date_time_preview_refresh(gpointer panel);
void ss_linux_date_time_preview_restart(SsLinuxDateTimePanel *panel);
void ss_linux_date_time_preview_dispose(SsLinuxDateTimePanel *panel);
guint ss_linux_date_time_panel_preview_interval_ms(
    SsLinuxDateTimePanel *panel);

#endif
