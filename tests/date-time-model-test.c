// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/date-time-model.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "Date & Time test failed: %s (%s:%d)\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static InfiltratrTemporalPolicyV3 persisted;
static bool persisted_found;
static bool fail_save;
static bool fail_load;
static SsDateTimeModel *reenter_model;

static bool fake_load(InfiltratrTemporalPolicyV3 *policy, bool *found)
{
    if (policy == NULL || found == NULL || fail_load)
        return false;
    if (persisted_found)
        *policy = persisted;
    else
        CHECK(infiltratr_temporal_policy_v3_default(policy));
    *found = persisted_found;
    return true;
}

static bool fake_save(const InfiltratrTemporalPolicyV3 *policy)
{
    if (policy == NULL || fail_save)
        return false;
    if (reenter_model != NULL)
        CHECK(!ss_date_time_model_set_show_seconds(reenter_model, false));
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

    persisted_found = false;
    CHECK(ss_date_time_model_init(&model, &store));
    CHECK(strcmp(model.policy.clock_mode, "standard") == 0);
    CHECK(strcmp(model.policy.calendar, "gregorian") == 0);
    CHECK(!model.persisted_policy_present);

    CHECK(ss_date_time_model_set_clock_mode(&model, "roman-temporal"));
    CHECK(strcmp(model.policy.clock_mode, "roman-temporal") == 0);
    CHECK(strcmp(persisted.clock_mode, "roman-temporal") == 0);

    CHECK(ss_date_time_model_set_calendar(
        &model, "egyptian-nabonassar"));
    CHECK(strcmp(model.policy.calendar, "egyptian-nabonassar") == 0);

    CHECK(ss_date_time_model_set_show_seconds(&model, true));
    CHECK(model.policy.show_seconds);

    CHECK(ss_date_time_model_set_location(&model, true, -36.39, 145.36));
    CHECK(model.policy.location_configured);
    CHECK(model.policy.latitude == -36.39);
    CHECK(model.policy.longitude == 145.36);

    strcpy(model.policy.clock_mode, "standard");
    CHECK(ss_date_time_model_reload(&model));
    CHECK(strcmp(model.policy.clock_mode, "roman-temporal") == 0);
    CHECK(strcmp(model.policy.calendar, "egyptian-nabonassar") == 0);
    CHECK(model.policy.show_seconds);
    CHECK(model.policy.location_configured);

    CHECK(!ss_date_time_model_set_clock_mode(&model, "system"));
    CHECK(!ss_date_time_model_set_calendar(&model, "none"));
    SsDateTimeModel empty = {0};
    CHECK(!ss_date_time_model_reload(&empty));
    CHECK(!ss_date_time_model_reload(NULL));
    const InfiltratrTemporalPolicyV3 before = model.policy;
    CHECK(!ss_date_time_model_set_location(&model, true, NAN, 0.0));
    CHECK(!ss_date_time_model_set_location(&model, false, 0.0, NAN));
    CHECK(!ss_date_time_model_set_location(&model, true, INFINITY, 0.0));
    fail_save = true;
    CHECK(!ss_date_time_model_set_clock_mode(&model, "decimal"));
    CHECK(memcmp(&model.policy, &before, sizeof(before)) == 0);
    fail_save = false;
    fail_load = true;
    CHECK(!ss_date_time_model_reload(&model));
    CHECK(memcmp(&model.policy, &before, sizeof(before)) == 0);
    fail_load = false;
    reenter_model = &model;
    CHECK(ss_date_time_model_set_clock_mode(&model, "decimal"));
    CHECK(model.policy.show_seconds);
    CHECK(!model.saving);
    return 0;
}
