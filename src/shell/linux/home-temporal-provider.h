// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_HOME_TEMPORAL_PROVIDER_H
#define SYSTEM_SETTINGS_HOME_TEMPORAL_PROVIDER_H

#include <glib.h>
#include <stdbool.h>

typedef struct SsHomeTemporalProvider SsHomeTemporalProvider;

typedef struct SsHomeTemporalSnapshot {
    gchar *clock_text;
    gchar *date_text;
    gchar *system_time_text;
} SsHomeTemporalSnapshot;

SsHomeTemporalProvider *ss_home_temporal_provider_new(void);
void ss_home_temporal_provider_free(SsHomeTemporalProvider *provider);

void ss_home_temporal_snapshot_init(SsHomeTemporalSnapshot *snapshot);
void ss_home_temporal_snapshot_clear(SsHomeTemporalSnapshot *snapshot);
bool ss_home_temporal_provider_format(
    SsHomeTemporalProvider *provider,
    SsHomeTemporalSnapshot *out);
guint ss_home_temporal_provider_refresh_interval_ms(
    SsHomeTemporalProvider *provider);

#endif
