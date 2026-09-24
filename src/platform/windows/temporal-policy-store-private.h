// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal-policy-store-private.h
 * @brief Path-injected Windows policy I/O used by the platform store and tests.
 *
 * Keeping raw file operations here lets regression tests use an isolated
 * temporary directory instead of touching the real LocalAppData policy.
 */
#ifndef SYSTEM_SETTINGS_WINDOWS_TEMPORAL_POLICY_STORE_PRIVATE_H
#define SYSTEM_SETTINGS_WINDOWS_TEMPORAL_POLICY_STORE_PRIVATE_H

#include <stdbool.h>
#include <wchar.h>

#include <infiltratr/temporal.h>

/**
 * Load one bounded UTF-8 policy file.
 * Missing, empty, oversized or malformed optional policy recovers to validated
 * Common defaults with found=false; hard I/O failures return false.
 */
bool ss_windows_temporal_policy_load_file(
    const wchar_t *path,
    InfiltratrTemporalPolicyV3 *policy,
    bool *found);

/** Atomically replace one policy file via flushed temporary-file publication. */
bool ss_windows_temporal_policy_save_file(
    const wchar_t *path,
    const InfiltratrTemporalPolicyV3 *policy);

#endif
