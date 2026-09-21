// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file project-info.h
 * @brief Canonical System Settings identity exposed through Infiltratr Common.
 */
#ifndef SYSTEM_SETTINGS_PROJECT_INFO_H
#define SYSTEM_SETTINGS_PROJECT_INFO_H
#include <infiltratr/core.h>
#ifdef __cplusplus
extern "C" {
#endif
const InfiltratrProjectInfo *ss_project_info(void);
#ifdef __cplusplus
}
#endif
#endif
