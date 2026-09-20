// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_TEMPORAL_POLICY_STORE_H
#define SYSTEM_SETTINGS_TEMPORAL_POLICY_STORE_H

#include <stdbool.h>
#include <infiltratr/temporal.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-owned persistence for the Infiltrator extension policy.
 * A missing file/value is success with *found == false and default policy.
 */
bool ss_temporal_policy_store_load(InfiltratrTemporalPolicy *policy,
                                   bool *found);
bool ss_temporal_policy_store_save(const InfiltratrTemporalPolicy *policy);

#ifdef __cplusplus
}
#endif
#endif
