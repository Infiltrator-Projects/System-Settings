// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_HOME_PAGE_H
#define SYSTEM_SETTINGS_HOME_PAGE_H

#include "builtin-module-host.h"

#include <gtk/gtk.h>

/*
 * Home asks the shell to navigate; it does not own a Date & Time-specific
 * callback contract. The current implementation still consumes the compatibility
 * aliases below until its event helper is renamed, but callers use navigate.
 */
typedef void (*SsHomeNavigateFunc)(gpointer user_data);

typedef struct SsHomePageContext {
    SsBuiltinModuleHost *module_host;
    union {
        SsHomeNavigateFunc navigate;
        SsHomeNavigateFunc open_date_time;
    };
    union {
        gpointer navigate_data;
        gpointer open_date_time_data;
    };
} SsHomePageContext;

GtkWidget *ss_linux_home_page_new(const SsHomePageContext *context);

#endif
