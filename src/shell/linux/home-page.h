// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_HOME_PAGE_H
#define SYSTEM_SETTINGS_HOME_PAGE_H

#include "home-temporal-provider.h"

#include <gtk/gtk.h>

typedef void (*SsHomeNavigateFunc)(const char *page_id, gpointer user_data);

typedef struct SsHomePageContext {
    SsHomeTemporalProvider *temporal_provider;
    SsHomeNavigateFunc navigate;
    gpointer navigate_data;
} SsHomePageContext;

GtkWidget *ss_linux_home_page_new(const SsHomePageContext *context);

#endif
