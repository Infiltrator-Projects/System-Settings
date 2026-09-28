// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_THEME_H
#define SYSTEM_SETTINGS_LINUX_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

void ss_linux_theme_install(void);
void ss_linux_theme_watch(void);
unsigned int ss_linux_theme_generation(void);

#ifdef __cplusplus
}
#endif
#endif
