// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file project-info.c
 * @brief Single source of truth for System Settings release identity.
 */
#include "system-settings/project-info.h"
#ifndef SYSTEM_SETTINGS_VERSION
#define SYSTEM_SETTINGS_VERSION "development"
#endif
#ifndef SYSTEM_SETTINGS_BUILD_PROFILE
#define SYSTEM_SETTINGS_BUILD_PROFILE "development"
#endif
static const InfiltratrProjectInfo project_info = {
    .struct_size = sizeof(InfiltratrProjectInfo),
    .abi_version = INFILTRATR_PROJECT_INFO_ABI,
    .program_name = "System Settings",
    .executable_name = "system-settings",
    .application_id = "org.infiltrator.SystemSettings",
    .version = SYSTEM_SETTINGS_VERSION,
    .source_id = "system-settings-" SYSTEM_SETTINGS_VERSION,
    .build_profile = SYSTEM_SETTINGS_BUILD_PROFILE,
    .author = "Shannon Smith",
    .website = "https://github.com/Infiltrator-Projects/System-Settings",
    .license_id = "GPL-3.0-or-later",
    .comments = "Native system-wide settings shell for Infiltrator applications.",
    .icon_name = "org.infiltrator.SystemSettings",
    .copyright_text = "Copyright © 2000-2026 Shannon Smith"
};
const InfiltratrProjectInfo *ss_project_info(void)
{
    return &project_info;
}
