// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal-policy-store.h
 * @brief Platform persistence boundary for temporal presentation policy.
 *
 * load() distinguishes a valid explicit policy (found=true) from a successful
 * platform/default fallback (found=false). save() publishes one complete
 * policy or reports failure; callers must not assume partial success.
 */
#ifndef SYSTEM_SETTINGS_TEMPORAL_POLICY_STORE_H
#define SYSTEM_SETTINGS_TEMPORAL_POLICY_STORE_H

#include <stdbool.h>
#include <infiltratr/temporal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SsTemporalPolicyStore {
    /** Populate policy; found reports whether explicit persisted state existed. */
    bool (*load)(InfiltratrTemporalPolicyV3 *policy, bool *found);
    /** Persist one complete validated version-3 policy. */
    bool (*save)(const InfiltratrTemporalPolicyV3 *policy);
} SsTemporalPolicyStore;

/** Return the process-lifetime immutable store for the active platform. */
const SsTemporalPolicyStore *ss_platform_temporal_policy_store(void);

#ifdef __cplusplus
}
#endif
#endif
