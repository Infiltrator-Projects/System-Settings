// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/date-time-model.h"

#include <infiltratr/core.h>

static bool save_candidate(SsDateTimeModel *model,
                           const InfiltratrTemporalPolicyV3 *candidate)
{
    if (model == NULL || candidate == NULL ||
        model->store == NULL || model->store->save == NULL ||
        !model->store->save(candidate)) {
        return false;
    }
    model->policy = *candidate;
    model->persisted_policy_present = true;
    return true;
}

bool ss_date_time_model_init(SsDateTimeModel *model,
                             const SsTemporalPolicyStore *store)
{
    if (model == NULL || store == NULL || store->load == NULL ||
        store->save == NULL ||
        !infiltratr_temporal_policy_v3_default(&model->policy)) {
        return false;
    }
    model->store = store;
    model->persisted_policy_present = false;
    return ss_date_time_model_reload(model);
}

bool ss_date_time_model_reload(SsDateTimeModel *model)
{
    InfiltratrTemporalPolicyV3 loaded;
    bool found = false;

    if (model == NULL ||
        !infiltratr_temporal_policy_v3_default(&loaded) ||
        !model->store->load(&loaded, &found)) {
        return false;
    }
    model->policy = loaded;
    model->persisted_policy_present = found;
    return true;
}

const InfiltratrTemporalPolicyV3 *
ss_date_time_model_policy(const SsDateTimeModel *model)
{
    return model != NULL ? &model->policy : NULL;
}

bool ss_date_time_model_set_clock_mode(SsDateTimeModel *model,
                                       const char *clock_mode)
{
    const InfiltratrTemporalClockModeInfo *info;
    InfiltratrTemporalPolicyV3 candidate;

    if (model == NULL) {
        return false;
    }
    info = infiltratr_temporal_clock_mode_find(clock_mode);
    if (info == NULL) {
        return false;
    }

    candidate = model->policy;
    infiltratr_copy_string(candidate.clock_mode,
                           sizeof(candidate.clock_mode),
                           info->id);
    return save_candidate(model, &candidate);
}

bool ss_date_time_model_set_calendar(SsDateTimeModel *model,
                                     const char *calendar_id)
{
    const InfiltratrTemporalCalendarInfo *info;
    InfiltratrTemporalPolicyV3 candidate;

    if (model == NULL) {
        return false;
    }
    info = infiltratr_temporal_calendar_find(calendar_id);
    if (info == NULL) {
        return false;
    }

    candidate = model->policy;
    infiltratr_copy_string(candidate.calendar,
                           sizeof(candidate.calendar),
                           info->id);
    return save_candidate(model, &candidate);
}

bool ss_date_time_model_set_show_seconds(SsDateTimeModel *model,
                                         bool show_seconds)
{
    InfiltratrTemporalPolicyV3 candidate;

    if (model == NULL) {
        return false;
    }
    candidate = model->policy;
    candidate.show_seconds = show_seconds;
    return save_candidate(model, &candidate);
}

bool ss_date_time_model_set_location(SsDateTimeModel *model,
                                     bool configured,
                                     double latitude,
                                     double longitude)
{
    InfiltratrTemporalPolicyV3 candidate;

    if (model == NULL ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        return false;
    }
    candidate = model->policy;
    candidate.location_configured = configured;
    candidate.latitude = latitude;
    candidate.longitude = longitude;
    return save_candidate(model, &candidate);
}
