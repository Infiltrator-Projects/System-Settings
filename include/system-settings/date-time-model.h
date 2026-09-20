// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_DATE_TIME_MODEL_H
#define SYSTEM_SETTINGS_DATE_TIME_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <infiltratr/temporal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SsDateTimeModel {
    InfiltratrTemporalPolicy policy;
    bool persisted_policy_present;
} SsDateTimeModel;

bool ss_date_time_model_init(SsDateTimeModel *model);
bool ss_date_time_model_reload(SsDateTimeModel *model);
const InfiltratrTemporalPolicy *
ss_date_time_model_policy(const SsDateTimeModel *model);
bool ss_date_time_model_set_clock_profile(SsDateTimeModel *model,
                                          InfiltratrClockProfile profile);
bool ss_date_time_model_set_show_seconds(SsDateTimeModel *model,
                                         bool show_seconds);
size_t ss_date_time_model_profile_count(void);
bool ss_date_time_model_profile_at(size_t index,
                                   InfiltratrClockProfile *profile);

#ifdef __cplusplus
}
#endif
#endif
