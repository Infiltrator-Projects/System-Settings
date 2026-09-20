// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_TEMPORAL_POLICY_STORE_H
#define SYSTEM_SETTINGS_TEMPORAL_POLICY_STORE_H

#include <stdbool.h>
#include <infiltratr/temporal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SsTemporalPolicyStore {
    bool (*load)(InfiltratrTemporalPolicyV2 *policy, bool *found);
    bool (*save)(const InfiltratrTemporalPolicyV2 *policy);
} SsTemporalPolicyStore;

const SsTemporalPolicyStore *ss_platform_temporal_policy_store(void);

#ifdef __cplusplus
}
#endif
#endif
