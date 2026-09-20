// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/temporal-policy-store.h"
#include <infiltratr/posix.h>
#include <stddef.h>

#define SS_PATH_CAPACITY 4096U
#define SS_POLICY_CAPACITY 1024U

static bool policy_paths(char *directory, size_t directory_size,
                         char *path, size_t path_size)
{
    char config_home[SS_PATH_CAPACITY];
    return infiltratr_xdg_config_home(config_home, sizeof(config_home)) &&
           infiltratr_path_join(directory, directory_size, config_home, "infiltrator") &&
           infiltratr_path_join(path, path_size, directory, "presentation.conf");
}

static bool platform_load(InfiltratrTemporalPolicyV2 *policy, bool *found)
{
    char directory[SS_PATH_CAPACITY];
    char path[SS_PATH_CAPACITY];
    char text[SS_POLICY_CAPACITY];
    InfiltratrIoResult result;

    if (policy == NULL || found == NULL ||
        !infiltratr_temporal_policy_v2_default(policy) ||
        !policy_paths(directory, sizeof(directory), path, sizeof(path)))
        return false;

    *found = false;
    result = infiltratr_read_text_file_ex(path, text, sizeof(text), NULL);
    if (result == INFILTRATR_IO_NOT_FOUND)
        return true;
    if (result != INFILTRATR_IO_OK ||
        !infiltratr_temporal_policy_v2_parse(text, policy))
        return false;
    *found = true;
    return true;
}

static bool platform_save(const InfiltratrTemporalPolicyV2 *policy)
{
    char directory[SS_PATH_CAPACITY];
    char path[SS_PATH_CAPACITY];
    char text[SS_POLICY_CAPACITY];
    size_t length = 0U;

    if (policy == NULL ||
        !policy_paths(directory, sizeof(directory), path, sizeof(path)) ||
        infiltratr_mkdir_parents(directory, 0700U) != 0 ||
        !infiltratr_temporal_policy_v2_serialize(policy, text, sizeof(text), &length))
        return false;

    return infiltratr_atomic_file_write_bytes(
        path, INFILTRATR_ATOMIC_FILE_PRIVATE, text, length) == 0;
}


const SsTemporalPolicyStore *ss_platform_temporal_policy_store(void)
{
    static const SsTemporalPolicyStore store = {
        .load = platform_load,
        .save = platform_save
    };
    return &store;
}
