// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_WINDOWS_TEMPORAL_POLICY_STORE_PRIVATE_H
#define SYSTEM_SETTINGS_WINDOWS_TEMPORAL_POLICY_STORE_PRIVATE_H

#include <stdbool.h>
#include <wchar.h>

#include <infiltratr/temporal.h>

bool ss_windows_temporal_policy_load_file(
    const wchar_t *path,
    InfiltratrTemporalPolicyV3 *policy,
    bool *found);

bool ss_windows_temporal_policy_save_file(
    const wchar_t *path,
    const InfiltratrTemporalPolicyV3 *policy);

#endif
