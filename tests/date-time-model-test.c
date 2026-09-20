// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/date-time-model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "Date & Time test failed: %s (%s:%d)\\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static InfiltratrTemporalPolicy persisted;
static bool persisted_found;

static bool fake_load(InfiltratrTemporalPolicy *policy, bool *found)
{
    if (policy == NULL || found == NULL)
        return false;
    if (persisted_found)
        *policy = persisted;
    else
        CHECK(infiltratr_temporal_policy_default(policy));
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
    CHECK(ss_date_time_model_init(&model, &store));
    CHECK(model.policy.clock_profile == INFILTRATR_CLOCK_PROFILE_SYSTEM);
    CHECK(!model.persisted_policy_present);

    CHECK(ss_date_time_model_profile_count() == 4U);
    for (i = 0U; i < ss_date_time_model_profile_count(); ++i) {
        CHECK(ss_date_time_model_profile_at(i, &profile));
        CHECK(infiltratr_clock_profile_id(profile) != NULL);
        CHECK(infiltratr_clock_profile_name(profile) != NULL);
        if (profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10) {
            saw_decimal = true;
            CHECK(strcmp(infiltratr_clock_profile_id(profile),
                          "decimal-10") == 0);
        }
    }
    CHECK(saw_decimal);

    CHECK(ss_date_time_model_set_clock_profile(
        &model, INFILTRATR_CLOCK_PROFILE_DECIMAL_10));
    CHECK(model.policy.clock_profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10);
    CHECK(persisted.clock_profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10);

    CHECK(ss_date_time_model_set_show_seconds(&model, true));
    CHECK(model.policy.show_seconds);
    CHECK(persisted.show_seconds);

    model.policy.clock_profile = INFILTRATR_CLOCK_PROFILE_SYSTEM;
    CHECK(ss_date_time_model_reload(&model));
    CHECK(model.policy.clock_profile == INFILTRATR_CLOCK_PROFILE_DECIMAL_10);
    CHECK(model.policy.show_seconds);
    return 0;
}
