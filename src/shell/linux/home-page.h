// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_HOME_PAGE_H
#define SYSTEM_SETTINGS_HOME_PAGE_H

#include "builtin-module-host.h"

#include <gtk/gtk.h>

typedef void (*SsHomeOpenDateTimeFunc)(gpointer user_data);

typedef struct SsHomePageContext {
    SsBuiltinModuleHost *module_host;
    SsHomeOpenDateTimeFunc open_date_time;
    gpointer open_date_time_data;
} SsHomePageContext;

GtkWidget *ss_linux_home_page_new(const SsHomePageContext *context);

#endif
