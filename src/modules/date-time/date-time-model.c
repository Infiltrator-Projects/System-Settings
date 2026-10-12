// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file date-time-model.c
 * @brief Validation and transactional publication for Date & Time policy.
 *
 * This layer contains no GTK or operating-system policy. It is the semantic
 * commit boundary between UI requests and the platform persistence adapter.
 */
#include "system-settings/date-time-model.h"

#include <infiltratr/core.h>
#include <math.h>
#include <stdlib.h>

struct SsDateTimeModelPrivate {
    InfiltratrTemporalPolicyV3 policy;
    const SsTemporalPolicyStore *store;
    bool persisted_policy_present;
    bool saving;
};

struct SsDateTimePolicyDraft {
    InfiltratrTemporalPolicyV3 policy;
};

static SsDateTimeModelPrivate *model_private(SsDateTimeModel *model)
{
    return model != NULL ? model->_private : NULL;
}

static const SsDateTimeModelPrivate *model_private_const(
    const SsDateTimeModel *model)
{
    return model != NULL ? model->_private : NULL;
}

static const InfiltratrTemporalClockModeInfo *valid_clock_mode(
    const char *clock_mode)
{
    return clock_mode != NULL
        ? infiltratr_temporal_clock_mode_find(clock_mode)
        : NULL;
}

static const InfiltratrTemporalCalendarInfo *valid_calendar(
    const char *calendar_id)
{
    return calendar_id != NULL
        ? infiltratr_temporal_calendar_find(calendar_id)
        : NULL;
}

static bool valid_location(double latitude, double longitude)
{
    return isfinite(latitude) && isfinite(longitude) &&
        latitude >= -90.0 && latitude <= 90.0 &&
        longitude >= -180.0 && longitude <= 180.0;
}

static void assign_clock_mode(
    InfiltratrTemporalPolicyV3 *policy,
    const InfiltratrTemporalClockModeInfo *info)
{
    infiltratr_copy_string(
        policy->clock_mode,
        sizeof(policy->clock_mode),
        info->id);
}

static void assign_calendar(
    InfiltratrTemporalPolicyV3 *policy,
    const InfiltratrTemporalCalendarInfo *info)
{
    infiltratr_copy_string(
        policy->calendar,
        sizeof(policy->calendar),
        info->id);
}

static void assign_location(
    InfiltratrTemporalPolicyV3 *policy,
    bool configured,
    double latitude,
    double longitude)
{
    policy->location_configured = configured;
    policy->latitude = latitude;
    policy->longitude = longitude;
}

/*
 * Persist-before-publish is the central model invariant. A failed write must
 * never make an uncommitted value visible through ss_date_time_model_policy().
 *
 * Stores that provide update locking get a stronger invariant: each setter
 * acquires the cross-process lock, reloads the latest complete policy, changes
 * only its own field, then publishes. Two processes editing unrelated fields
 * therefore merge instead of silently replacing one another's fresh changes.
 */
static bool begin_candidate(
    SsDateTimeModel *model,
    InfiltratrTemporalPolicyV3 *candidate)
{
    SsDateTimeModelPrivate *private_state = model_private(model);
    bool found = false;

    if (private_state == NULL || candidate == NULL ||
        private_state->store == NULL || private_state->store->save == NULL ||
        private_state->saving) {
        return false;
    }

    private_state->saving = true;
    if (private_state->store->begin_update != NULL) {
        if (!private_state->store->begin_update()) {
            private_state->saving = false;
            return false;
        }
        if (private_state->store->load == NULL ||
            !private_state->store->load(candidate, &found)) {
            private_state->store->end_update();
            private_state->saving = false;
            return false;
        }
    } else {
        *candidate = private_state->policy;
    }
    return true;
}

static bool save_candidate(
    SsDateTimeModel *model,
    const InfiltratrTemporalPolicyV3 *candidate)
{
    SsDateTimeModelPrivate *private_state = model_private(model);
    bool saved;

    if (private_state == NULL || candidate == NULL ||
        !private_state->saving || private_state->store == NULL ||
        private_state->store->save == NULL) {
        return false;
    }

    saved = private_state->store->save(candidate);
    if (private_state->store->begin_update != NULL) {
        private_state->store->end_update();
    }
    private_state->saving = false;
    if (!saved) {
        return false;
    }
    private_state->policy = *candidate;
    private_state->persisted_policy_present = true;
    return true;
}

bool ss_date_time_model_init(
    SsDateTimeModel *model,
    const SsTemporalPolicyStore *store)
{
    SsDateTimeModelPrivate *private_state;

    if (model == NULL || model->_private != NULL ||
        store == NULL || store->load == NULL || store->save == NULL ||
        ((store->begin_update == NULL) != (store->end_update == NULL))) {
        return false;
    }

    private_state = calloc(1U, sizeof(*private_state));
    if (private_state == NULL ||
        !infiltratr_temporal_policy_v3_default(&private_state->policy)) {
        free(private_state);
        return false;
    }
    private_state->store = store;
    model->_private = private_state;
    if (!ss_date_time_model_reload(model)) {
        ss_date_time_model_clear(model);
        return false;
    }
    return true;
}

void ss_date_time_model_clear(SsDateTimeModel *model)
{
    if (model == NULL) {
        return;
    }
    free(model->_private);
    model->_private = NULL;
}

bool ss_date_time_model_reload(SsDateTimeModel *model)
{
    SsDateTimeModelPrivate *private_state = model_private(model);
    InfiltratrTemporalPolicyV3 loaded;
    bool found = false;

    if (private_state == NULL || private_state->store == NULL ||
        private_state->store->load == NULL || private_state->saving ||
        !infiltratr_temporal_policy_v3_default(&loaded) ||
        !private_state->store->load(&loaded, &found)) {
        return false;
    }
    private_state->policy = loaded;
    private_state->persisted_policy_present = found;
    return true;
}

const InfiltratrTemporalPolicyV3 *
ss_date_time_model_policy(const SsDateTimeModel *model)
{
    const SsDateTimeModelPrivate *private_state = model_private_const(model);
    return private_state != NULL ? &private_state->policy : NULL;
}

bool ss_date_time_model_has_persisted_policy(const SsDateTimeModel *model)
{
    const SsDateTimeModelPrivate *private_state = model_private_const(model);
    return private_state != NULL && private_state->persisted_policy_present;
}

bool ss_date_time_model_is_saving(const SsDateTimeModel *model)
{
    const SsDateTimeModelPrivate *private_state = model_private_const(model);
    return private_state != NULL && private_state->saving;
}

bool ss_date_time_model_normalize_unpersisted_clock_mode(
    SsDateTimeModel *model,
    const char *clock_mode)
{
    SsDateTimeModelPrivate *private_state = model_private(model);
    const InfiltratrTemporalClockModeInfo *info = valid_clock_mode(clock_mode);

    if (private_state == NULL || info == NULL || private_state->saving ||
        private_state->persisted_policy_present) {
        return false;
    }
    assign_clock_mode(&private_state->policy, info);
    return true;
}

bool ss_date_time_model_set_clock_mode(
    SsDateTimeModel *model,
    const char *clock_mode)
{
    const InfiltratrTemporalClockModeInfo *info = valid_clock_mode(clock_mode);
    InfiltratrTemporalPolicyV3 candidate;

    if (model == NULL || info == NULL || !begin_candidate(model, &candidate)) {
        return false;
    }
    assign_clock_mode(&candidate, info);
    return save_candidate(model, &candidate);
}

bool ss_date_time_model_set_calendar(
    SsDateTimeModel *model,
    const char *calendar_id)
{
    const InfiltratrTemporalCalendarInfo *info = valid_calendar(calendar_id);
    InfiltratrTemporalPolicyV3 candidate;

    if (model == NULL || info == NULL || !begin_candidate(model, &candidate)) {
        return false;
    }
    assign_calendar(&candidate, info);
    return save_candidate(model, &candidate);
}

bool ss_date_time_model_set_show_seconds(
    SsDateTimeModel *model,
    bool show_seconds)
{
    InfiltratrTemporalPolicyV3 candidate;

    if (model == NULL || !begin_candidate(model, &candidate)) {
        return false;
    }
    candidate.show_seconds = show_seconds;
    return save_candidate(model, &candidate);
}

bool ss_date_time_model_set_location(
    SsDateTimeModel *model,
    bool configured,
    double latitude,
    double longitude)
{
    InfiltratrTemporalPolicyV3 candidate;

    if (model == NULL || !valid_location(latitude, longitude) ||
        !begin_candidate(model, &candidate)) {
        return false;
    }
    assign_location(&candidate, configured, latitude, longitude);
    return save_candidate(model, &candidate);
}

SsDateTimePolicyDraft *ss_date_time_policy_draft_new(
    const InfiltratrTemporalPolicyV3 *base_policy)
{
    SsDateTimePolicyDraft *draft;

    if (base_policy == NULL) {
        return NULL;
    }
    draft = calloc(1U, sizeof(*draft));
    if (draft != NULL) {
        draft->policy = *base_policy;
    }
    return draft;
}

void ss_date_time_policy_draft_free(SsDateTimePolicyDraft *draft)
{
    free(draft);
}

const InfiltratrTemporalPolicyV3 *ss_date_time_policy_draft_policy(
    const SsDateTimePolicyDraft *draft)
{
    return draft != NULL ? &draft->policy : NULL;
}

bool ss_date_time_policy_draft_set_clock_mode(
    SsDateTimePolicyDraft *draft,
    const char *clock_mode)
{
    const InfiltratrTemporalClockModeInfo *info = valid_clock_mode(clock_mode);

    if (draft == NULL || info == NULL) {
        return false;
    }
    assign_clock_mode(&draft->policy, info);
    return true;
}

bool ss_date_time_policy_draft_set_calendar(
    SsDateTimePolicyDraft *draft,
    const char *calendar_id)
{
    const InfiltratrTemporalCalendarInfo *info = valid_calendar(calendar_id);

    if (draft == NULL || info == NULL) {
        return false;
    }
    assign_calendar(&draft->policy, info);
    return true;
}

bool ss_date_time_policy_draft_set_show_seconds(
    SsDateTimePolicyDraft *draft,
    bool show_seconds)
{
    if (draft == NULL) {
        return false;
    }
    draft->policy.show_seconds = show_seconds;
    return true;
}

bool ss_date_time_policy_draft_set_location(
    SsDateTimePolicyDraft *draft,
    bool configured,
    double latitude,
    double longitude)
{
    if (draft == NULL || !valid_location(latitude, longitude)) {
        return false;
    }
    assign_location(&draft->policy, configured, latitude, longitude);
    return true;
}
