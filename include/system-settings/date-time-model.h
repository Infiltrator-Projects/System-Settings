// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_DATE_TIME_MODEL_H
#define SYSTEM_SETTINGS_DATE_TIME_MODEL_H

#include <stdbool.h>
#include <infiltratr/temporal.h>
#include "system-settings/temporal-policy-store.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SsDateTimeModel {
    InfiltratrTemporalPolicyV3 policy;
    const SsTemporalPolicyStore *store;
    bool persisted_policy_present;
} SsDateTimeModel;

bool ss_date_time_model_init(SsDateTimeModel *model,
                             const SsTemporalPolicyStore *store);
bool ss_date_time_model_reload(SsDateTimeModel *model);
const InfiltratrTemporalPolicyV3 *
ss_date_time_model_policy(const SsDateTimeModel *model);

bool ss_date_time_model_set_clock_mode(SsDateTimeModel *model,
                                       const char *clock_mode);
bool ss_date_time_model_set_calendar(SsDateTimeModel *model,
                                     const char *calendar_id);
bool ss_date_time_model_set_show_seconds(SsDateTimeModel *model,
                                         bool show_seconds);
bool ss_date_time_model_set_location(SsDateTimeModel *model,
                                     bool configured,
                                     double latitude,
                                     double longitude);

#ifdef __cplusplus
}
#endif
#endif
