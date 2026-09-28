// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal-policy-store.h
 * @brief Platform persistence boundary for temporal presentation policy.
 *
 * load() distinguishes a valid explicit policy (found=true) from a successful
 * platform/default fallback (found=false). save() publishes one complete
 * policy or reports failure; callers must not assume partial success.
 *
 * begin_update()/end_update() are optional interprocess serialization hooks.
 * When supplied, the model acquires them before reloading authoritative state,
 * applies exactly one requested field mutation to that fresh state, publishes
 * it, then releases the lock. This prevents unrelated concurrent edits from
 * being lost through last-writer-wins whole-document replacement.
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
    /** Optional: acquire the platform's cross-process update lock. */
    bool (*begin_update)(void);
    /** Optional mate for begin_update(); called exactly once after acquisition. */
    void (*end_update)(void);
    /**
     * Optional post-save compatibility verification. The authoritative policy
     * can commit successfully even when a desktop compatibility mirror cannot;
     * callers that report native synchronization status use this separately.
     */
    bool (*compatibility_matches)(
        const InfiltratrTemporalPolicyV3 *policy);
} SsTemporalPolicyStore;

/** Return the process-lifetime immutable store for the active platform. */
const SsTemporalPolicyStore *ss_platform_temporal_policy_store(void);

#ifdef __cplusplus
}
#endif
#endif
