// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/date-time-model.h"

#include <assert.h>
#include <string.h>

static InfiltratrTemporalPolicy persisted;
static bool persisted_found;

static bool fake_load(InfiltratrTemporalPolicy *policy, bool *found)
{
    if (policy == NULL || found == NULL)
        return false;
    if (persisted_found)
        *policy = persisted;
    else
        assert(infiltratr_temporal_policy_default(policy));
    *found = persisted_found;
    return true;
}

static bool fake_save(const InfiltratrTemporalPolicy *policy)
{
    if (policy == NULL)
        return false;
    persisted = *policy;
    persisted_found = true;
    return true;
}

int main(void)
{
    static const SsTemporalPolicyStore store = {
        .load = fake_load,
        .save = fake_save
    };
    SsDateTimeModel model;
    InfiltratrClockProfile profile = INFILTRATR_CLOCK_PROFILE_SYSTEM;
    size_t i;
    bool saw_decimal = false;

    persisted_found = false;
    assert(ss_date_time_model_init(&model, &store));
    assert(model.policy.clock_profile == INFILTRATR_CLOCK_PROFILE_SYSTEM);
    assert(!model.persisted_policy_present);

    assert(ss_date_time_model_profile_count() == 4U);
    for (i = 0U; i < ss_date_time_model_profile_count(); ++i) {
        assert(ss_date_time_model_profile_at(i, &profile));
        assert(infiltratr_clock_profile_id(profile) != NULL);
        assert(infiltratr_clock_profile_name(profile) != NULL);
        if (profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10) {
            saw_decimal = true;
            assert(strcmp(infiltratr_clock_profile_id(profile),
                          "decimal-10") == 0);
        }
    }
    assert(saw_decimal);

    assert(ss_date_time_model_set_clock_profile(
        &model, INFILTRATR_CLOCK_PROFILE_DECIMAL_10));
    assert(model.policy.clock_profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10);
    assert(persisted.clock_profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10);

    assert(ss_date_time_model_set_show_seconds(&model, true));
    assert(model.policy.show_seconds);
    assert(persisted.show_seconds);

    model.policy.clock_profile = INFILTRATR_CLOCK_PROFILE_SYSTEM;
    assert(ss_date_time_model_reload(&model));
    assert(model.policy.clock_profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10);
    assert(model.policy.show_seconds);
    return 0;
}
