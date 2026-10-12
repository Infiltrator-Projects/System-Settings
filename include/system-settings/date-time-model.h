// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file date-time-model.h
 * @brief Transactional owner of the persisted temporal presentation policy.
 *
 * The model deliberately separates candidate edits from authoritative state:
 * a setter validates and persists a complete candidate policy before replacing
 * the in-memory policy. Failed persistence therefore leaves the model exactly
 * as it was before the attempted edit.
 */
#ifndef SYSTEM_SETTINGS_DATE_TIME_MODEL_H
#define SYSTEM_SETTINGS_DATE_TIME_MODEL_H

#if defined(SYSTEM_SETTINGS_GENERIC_SHELL)
#error "Generic Linux shell must not include Date & Time domain contracts."
#endif

#include <stdbool.h>
#include <infiltratr/temporal.h>
#include "system-settings/temporal-policy-store.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SsDateTimeModelPrivate SsDateTimeModelPrivate;

typedef struct SsDateTimeModel {
    SsDateTimeModelPrivate *_private;
} SsDateTimeModel;

#define SS_DATE_TIME_MODEL_INIT { NULL }

typedef struct SsDateTimePolicyDraft SsDateTimePolicyDraft;

/**
 * Initialise a zero-initialised model and immediately reconcile it with the
 * supplied store. The store pointer is borrowed and must outlive the model.
 * Call ss_date_time_model_clear() before reinitialising or leaving scope.
 */
bool ss_date_time_model_init(SsDateTimeModel *model,
                             const SsTemporalPolicyStore *store);
/** Release the private model state and return the handle to its empty state. */
void ss_date_time_model_clear(SsDateTimeModel *model);
/**
 * Reload authoritative state from the store.
 *
 * On failure the caller must treat the operation as unsuccessful; the model is
 * not a staging area for partially parsed policy documents.
 */
bool ss_date_time_model_reload(SsDateTimeModel *model);

/** Return a borrowed pointer to the model's current committed policy. */
const InfiltratrTemporalPolicyV3 *
ss_date_time_model_policy(const SsDateTimeModel *model);
/** Whether the current committed policy came from explicit persisted storage. */
bool ss_date_time_model_has_persisted_policy(const SsDateTimeModel *model);
/** Whether the model is currently inside its persist-before-publish boundary. */
bool ss_date_time_model_is_saving(const SsDateTimeModel *model);

/**
 * Normalize a legacy, unpersisted bootstrap clock choice in memory.
 *
 * This is intentionally narrower than a general mutation escape hatch: it is
 * accepted only before an explicit policy document exists and still validates
 * the requested clock identifier. Persisted models must use the transactional
 * setter below.
 */
bool ss_date_time_model_normalize_unpersisted_clock_mode(
    SsDateTimeModel *model,
    const char *clock_mode);

/**
 * Persist one clock-mode change transactionally.
 * Invalid catalogue identifiers or persistence failure leave the model intact.
 */
bool ss_date_time_model_set_clock_mode(SsDateTimeModel *model,
                                       const char *clock_mode);
/** Persist one validated calendar change transactionally. */
bool ss_date_time_model_set_calendar(SsDateTimeModel *model,
                                     const char *calendar_id);
/** Persist the seconds-presentation choice transactionally. */
bool ss_date_time_model_set_show_seconds(SsDateTimeModel *model,
                                         bool show_seconds);
/**
 * Persist geographic presentation context transactionally.
 * Latitude/longitude are validated even when configured is false so malformed
 * coordinates never enter the versioned policy. NaN and infinities fail.
 */
bool ss_date_time_model_set_location(SsDateTimeModel *model,
                                     bool configured,
                                     double latitude,
                                     double longitude);

/**
 * Create an isolated, non-authoritative policy draft from a committed policy.
 * Draft setters perform the same semantic validation as model setters but do
 * not perform persistence. This is the supported boundary for a caller that
 * must validate several related edits before one external transaction commits.
 */
SsDateTimePolicyDraft *ss_date_time_policy_draft_new(
    const InfiltratrTemporalPolicyV3 *base_policy);
void ss_date_time_policy_draft_free(SsDateTimePolicyDraft *draft);
const InfiltratrTemporalPolicyV3 *ss_date_time_policy_draft_policy(
    const SsDateTimePolicyDraft *draft);
bool ss_date_time_policy_draft_set_clock_mode(
    SsDateTimePolicyDraft *draft,
    const char *clock_mode);
bool ss_date_time_policy_draft_set_calendar(
    SsDateTimePolicyDraft *draft,
    const char *calendar_id);
bool ss_date_time_policy_draft_set_show_seconds(
    SsDateTimePolicyDraft *draft,
    bool show_seconds);
bool ss_date_time_policy_draft_set_location(
    SsDateTimePolicyDraft *draft,
    bool configured,
    double latitude,
    double longitude);

#ifdef __cplusplus
}
#endif
#endif
