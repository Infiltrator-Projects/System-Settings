// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/date-time-model.h"

#include <infiltratr/core.h>

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
static bool fail_clear;
static bool update_locked;
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
    if (reenter_model != NULL) {
        CHECK(ss_date_time_model_is_saving(reenter_model));
        CHECK(!ss_date_time_model_set_show_seconds(reenter_model, false));
    }
    persisted = *policy;
    persisted_found = true;
    return true;
}

static bool fake_clear(void)
{
    if (fail_clear)
        return false;
    persisted_found = false;
    return true;
}

static bool fake_begin_update(void)
{
    if (update_locked)
        return false;
    update_locked = true;
    return true;
}

static void fake_end_update(void)
{
    CHECK(update_locked);
    update_locked = false;
}

static const InfiltratrTemporalPolicyV3 *policy_of(
    const SsDateTimeModel *model)
{
    const InfiltratrTemporalPolicyV3 *policy =
        ss_date_time_model_policy(model);
    CHECK(policy != NULL);
    return policy;
}

int main(void)
{
    static const SsTemporalPolicyStore store = {
        .load = fake_load,
        .save = fake_save,
        .clear = fake_clear,
        .begin_update = fake_begin_update,
        .end_update = fake_end_update
    };
    SsDateTimeModel model = SS_DATE_TIME_MODEL_INIT;
    SsDateTimeModel bootstrap_model = SS_DATE_TIME_MODEL_INIT;
    SsDateTimePolicyDraft *draft;
    SsDateTimeLocationUpdate location_update =
        SS_DATE_TIME_LOCATION_UPDATE_INIT;
    InfiltratrTemporalPolicyV3 before;

    persisted_found = false;
    fail_save = false;
    fail_load = false;
    fail_clear = false;
    update_locked = false;
    reenter_model = NULL;

    CHECK(ss_date_time_model_init(&model, &store));
    CHECK(strcmp(policy_of(&model)->clock_mode, "standard") == 0);
    CHECK(strcmp(policy_of(&model)->calendar, "gregorian") == 0);
    CHECK(!ss_date_time_model_has_persisted_policy(&model));
    CHECK(!ss_date_time_model_is_saving(&model));

    CHECK(ss_date_time_model_init(&bootstrap_model, &store));
    CHECK(ss_date_time_model_normalize_unpersisted_clock_mode(
        &bootstrap_model, "standard-24"));
    CHECK(strcmp(
        policy_of(&bootstrap_model)->clock_mode,
        "standard-24") == 0);
    CHECK(!ss_date_time_model_has_persisted_policy(&bootstrap_model));
    CHECK(!ss_date_time_model_normalize_unpersisted_clock_mode(
        &bootstrap_model, "not-a-clock"));

    CHECK(ss_date_time_model_set_clock_mode(&model, "roman-temporal"));
    CHECK(strcmp(policy_of(&model)->clock_mode, "roman-temporal") == 0);
    CHECK(strcmp(persisted.clock_mode, "roman-temporal") == 0);
    CHECK(ss_date_time_model_has_persisted_policy(&model));
    CHECK(!ss_date_time_model_normalize_unpersisted_clock_mode(
        &model, "standard-24"));

    CHECK(ss_date_time_model_set_calendar(
        &model, "egyptian-nabonassar"));
    CHECK(strcmp(
        policy_of(&model)->calendar,
        "egyptian-nabonassar") == 0);

    CHECK(ss_date_time_model_set_show_seconds(&model, true));
    CHECK(policy_of(&model)->show_seconds);

    CHECK(ss_date_time_model_set_location(&model, true, -36.39, 145.36));
    CHECK(policy_of(&model)->location_configured);
    CHECK(policy_of(&model)->latitude == -36.39);
    CHECK(policy_of(&model)->longitude == 145.36);

    /* External authority changes are visible only after an explicit reload. */
    infiltratr_copy_string(
        persisted.clock_mode,
        sizeof(persisted.clock_mode),
        "decimal");
    CHECK(strcmp(policy_of(&model)->clock_mode, "roman-temporal") == 0);
    CHECK(ss_date_time_model_reload(&model));
    CHECK(strcmp(policy_of(&model)->clock_mode, "decimal") == 0);
    CHECK(strcmp(
        policy_of(&model)->calendar,
        "egyptian-nabonassar") == 0);
    CHECK(policy_of(&model)->show_seconds);
    CHECK(policy_of(&model)->location_configured);

    CHECK(!ss_date_time_model_set_clock_mode(&model, "system"));
    CHECK(!ss_date_time_model_set_calendar(&model, "none"));
    CHECK(!ss_date_time_model_reload(NULL));
    before = *policy_of(&model);
    CHECK(!ss_date_time_model_set_location(&model, true, NAN, 0.0));
    CHECK(!ss_date_time_model_set_location(&model, false, 0.0, NAN));
    CHECK(!ss_date_time_model_set_location(&model, true, INFINITY, 0.0));

    fail_save = true;
    CHECK(!ss_date_time_model_set_clock_mode(&model, "standard-24"));
    CHECK(memcmp(policy_of(&model), &before, sizeof(before)) == 0);
    fail_save = false;

    fail_load = true;
    CHECK(!ss_date_time_model_reload(&model));
    CHECK(memcmp(policy_of(&model), &before, sizeof(before)) == 0);
    fail_load = false;

    reenter_model = &model;
    CHECK(ss_date_time_model_set_clock_mode(&model, "standard-24"));
    CHECK(policy_of(&model)->show_seconds);
    CHECK(!ss_date_time_model_is_saving(&model));
    reenter_model = NULL;

    /* Batch staging must be isolated from committed model authority. */
    before = *policy_of(&model);
    draft = ss_date_time_policy_draft_new(&before);
    CHECK(draft != NULL);
    CHECK(!ss_date_time_policy_draft_set_clock_mode(draft, "system"));
    CHECK(!ss_date_time_policy_draft_set_calendar(draft, "none"));
    CHECK(ss_date_time_policy_draft_set_clock_mode(draft, "decimal"));
    CHECK(ss_date_time_policy_draft_set_calendar(draft, "gregorian"));
    CHECK(ss_date_time_policy_draft_set_show_seconds(draft, false));
    CHECK(ss_date_time_policy_draft_set_location(
        draft, false, before.latitude, before.longitude));
    CHECK(strcmp(
        ss_date_time_policy_draft_policy(draft)->clock_mode,
        "decimal") == 0);
    CHECK(strcmp(policy_of(&model)->clock_mode, before.clock_mode) == 0);
    CHECK(policy_of(&model)->show_seconds == before.show_seconds);
    ss_date_time_policy_draft_free(draft);

    /*
     * A reversible locality mutation retains the update lock until its external
     * metadata half is resolved. Rolling back a platform-default starting state
     * must restore absence, not create an explicit file containing defaults.
     */
    persisted_found = false;
    CHECK(ss_date_time_model_reload(&bootstrap_model));
    CHECK(ss_date_time_model_begin_location_update(
        &bootstrap_model, true, -36.39, 145.36, &location_update));
    CHECK(location_update.active);
    CHECK(update_locked);
    CHECK(ss_date_time_model_is_saving(&bootstrap_model));
    CHECK(persisted_found);
    CHECK(!ss_date_time_model_reload(&bootstrap_model));
    CHECK(ss_date_time_model_rollback_location_update(
        &bootstrap_model, &location_update));
    CHECK(!location_update.active);
    CHECK(!update_locked);
    CHECK(!persisted_found);
    CHECK(!ss_date_time_model_has_persisted_policy(&bootstrap_model));
    CHECK(!policy_of(&bootstrap_model)->location_configured);

    /* Successful completion keeps the new explicit policy and releases lock. */
    CHECK(ss_date_time_model_begin_location_update(
        &bootstrap_model, true, -37.8136, 144.9631, &location_update));
    CHECK(update_locked);
    CHECK(ss_date_time_model_finish_location_update(
        &bootstrap_model, &location_update));
    CHECK(!update_locked);
    CHECK(persisted_found);
    CHECK(policy_of(&bootstrap_model)->location_configured);

    /* Rollback failure is explicit and still releases transaction ownership. */
    CHECK(ss_date_time_model_reload(&bootstrap_model));
    CHECK(ss_date_time_model_begin_location_update(
        &bootstrap_model, true, -33.8688, 151.2093, &location_update));
    fail_save = true;
    CHECK(!ss_date_time_model_rollback_location_update(
        &bootstrap_model, &location_update));
    fail_save = false;
    CHECK(!update_locked);
    CHECK(!location_update.active);
    CHECK(ss_date_time_model_reload(&bootstrap_model));
    CHECK(policy_of(&bootstrap_model)->latitude == -33.8688);

    ss_date_time_model_clear(&bootstrap_model);
    ss_date_time_model_clear(&model);
    CHECK(ss_date_time_model_policy(&model) == NULL);
    CHECK(!ss_date_time_model_reload(&model));
    return 0;
}
