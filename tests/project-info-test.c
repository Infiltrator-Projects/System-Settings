// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/project-info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "Project identity test failed: %s (%s:%d)\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

int main(void)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    const char *expected_label;

    CHECK(infiltratr_project_info_is_valid(info));
    CHECK(strcmp(info->program_name, "System Settings") == 0);
    CHECK(strcmp(info->application_id, "org.infiltrator.SystemSettings") == 0);
    CHECK(strcmp(info->version, SYSTEM_SETTINGS_VERSION) == 0);
    CHECK(strcmp(info->build_profile, SYSTEM_SETTINGS_BUILD_PROFILE) == 0);

    if (strcmp(SYSTEM_SETTINGS_BUILD_PROFILE, "generic") == 0)
        expected_label = "Generic / APT package";
    else if (strcmp(SYSTEM_SETTINGS_BUILD_PROFILE, "native") == 0)
        expected_label = "Native / local machine compile";
    else if (strcmp(SYSTEM_SETTINGS_BUILD_PROFILE, "cmake") == 0)
        expected_label = "Source / CMake build";
    else
        expected_label = "Source / development build";

    CHECK(strcmp(infiltratr_build_profile_label(info->build_profile),
                 expected_label) == 0);
    return EXIT_SUCCESS;
}
