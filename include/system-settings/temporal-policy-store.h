// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_TEMPORAL_POLICY_STORE_H
#define SYSTEM_SETTINGS_TEMPORAL_POLICY_STORE_H

#include <stdbool.h>
#include <infiltratr/temporal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SsTemporalPolicyStore {
    bool (*load)(InfiltratrTemporalPolicy *policy, bool *found);
    bool (*save)(const InfiltratrTemporalPolicy *policy);
} SsTemporalPolicyStore;

/* Returns the current platform's user-policy persistence adapter. */
const SsTemporalPolicyStore *ss_platform_temporal_policy_store(void);

#ifdef __cplusplus
}
#endif
#endif
