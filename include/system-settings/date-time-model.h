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

#include <stdbool.h>
#include <infiltratr/temporal.h>
#include "system-settings/temporal-policy-store.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SsDateTimeModel {
    /** Last policy successfully loaded from, or committed to, the store. */
    InfiltratrTemporalPolicyV3 policy;
    /** Platform persistence adapter; borrowed for the model lifetime. */
    const SsTemporalPolicyStore *store;
    /** Whether the current policy came from an explicit persisted document. */
    bool persisted_policy_present;
    /** Reject synchronous re-entry from compatibility notifications during save. */
    bool saving;
} SsDateTimeModel;

/**
 * Initialise a model and immediately reconcile it with the supplied store.
 * The store pointer is borrowed and must outlive the model.
 */
bool ss_date_time_model_init(SsDateTimeModel *model,
                             const SsTemporalPolicyStore *store);
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

#ifdef __cplusplus
}
#endif
#endif
