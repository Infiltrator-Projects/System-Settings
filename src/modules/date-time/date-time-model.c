// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/date-time-model.h"
#include "system-settings/temporal-policy-store.h"

static bool profile_is_catalogued(InfiltratrClockProfile profile)
{
    size_t index;
    for (index = 0U; index < infiltratr_clock_profile_count(); ++index) {
        InfiltratrClockProfile candidate;
        if (infiltratr_clock_profile_at(index, &candidate) &&
            candidate == profile) {
            return true;
        }
    }
    return false;
}

bool ss_date_time_model_init(SsDateTimeModel *model,
                             const SsTemporalPolicyStore *store)
{
    if (model == NULL || store == NULL || store->load == NULL ||
        store->save == NULL ||
        !infiltratr_temporal_policy_default(&model->policy)) {
        return false;
    }
    model->store = store;
    model->persisted_policy_present = false;
    return ss_date_time_model_reload(model);
}

bool ss_date_time_model_reload(SsDateTimeModel *model)
{
    InfiltratrTemporalPolicy loaded;
    bool found = false;

    if (model == NULL ||
        !infiltratr_temporal_policy_default(&loaded) ||
        !model->store->load(&loaded, &found)) {
        return false;
    }
    model->policy = loaded;
    model->persisted_policy_present = found;
    return true;
}

const InfiltratrTemporalPolicy *
ss_date_time_model_policy(const SsDateTimeModel *model)
{
    return model != NULL ? &model->policy : NULL;
}

bool ss_date_time_model_set_clock_profile(SsDateTimeModel *model,
                                          InfiltratrClockProfile profile)
{
    InfiltratrTemporalPolicy candidate;
    if (model == NULL || !profile_is_catalogued(profile)) {
        return false;
    }
    candidate = model->policy;
    candidate.clock_profile = profile;
    if (!model->store->save(&candidate)) {
        return false;
    }
    model->policy = candidate;
    model->persisted_policy_present = true;
    return true;
}

bool ss_date_time_model_set_show_seconds(SsDateTimeModel *model,
                                         bool show_seconds)
{
    InfiltratrTemporalPolicy candidate;
    if (model == NULL) {
        return false;
    }
    candidate = model->policy;
    candidate.show_seconds = show_seconds;
    if (!model->store->save(&candidate)) {
        return false;
    }
    model->policy = candidate;
    model->persisted_policy_present = true;
    return true;
}

size_t ss_date_time_model_profile_count(void)
{
    return infiltratr_clock_profile_count();
}

bool ss_date_time_model_profile_at(size_t index,
                                   InfiltratrClockProfile *profile)
{
    return infiltratr_clock_profile_at(index, profile);
}
