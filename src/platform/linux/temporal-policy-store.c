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

bool ss_temporal_policy_store_load(InfiltratrTemporalPolicy *policy, bool *found)
{
    char directory[SS_PATH_CAPACITY];
    char path[SS_PATH_CAPACITY];
    char text[SS_POLICY_CAPACITY];
    InfiltratrIoResult result;

    if (policy == NULL || found == NULL ||
        !infiltratr_temporal_policy_default(policy) ||
        !policy_paths(directory, sizeof(directory), path, sizeof(path)))
        return false;

    *found = false;
    result = infiltratr_read_text_file_ex(path, text, sizeof(text), NULL);
    if (result == INFILTRATR_IO_NOT_FOUND)
        return true;
    if (result != INFILTRATR_IO_OK ||
        !infiltratr_temporal_policy_parse(text, policy))
        return false;
    *found = true;
    return true;
}

bool ss_temporal_policy_store_save(const InfiltratrTemporalPolicy *policy)
{
    char directory[SS_PATH_CAPACITY];
    char path[SS_PATH_CAPACITY];
    char text[SS_POLICY_CAPACITY];
    size_t length = 0U;

    if (policy == NULL ||
        !policy_paths(directory, sizeof(directory), path, sizeof(path)) ||
        infiltratr_mkdir_parents(directory, 0700U) != 0 ||
        !infiltratr_temporal_policy_serialize(policy, text, sizeof(text), &length))
        return false;

    return infiltratr_atomic_file_write_bytes(
        path, INFILTRATR_ATOMIC_FILE_PRIVATE, text, length) == 0;
}
