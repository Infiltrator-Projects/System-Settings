// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/project-info.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    const InfiltratrProjectInfo *info = ss_project_info();
    assert(infiltratr_project_info_is_valid(info));
    assert(strcmp(info->program_name, "System Settings") == 0);
    assert(strcmp(info->application_id, "org.infiltrator.SystemSettings") == 0);
    assert(strcmp(info->version, SYSTEM_SETTINGS_VERSION) == 0);
    assert(strcmp(infiltratr_build_profile_label(info->build_profile),
                  "CMake build") == 0);
    return 0;
}
