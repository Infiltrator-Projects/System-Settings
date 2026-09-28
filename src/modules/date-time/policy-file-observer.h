// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_POLICY_FILE_OBSERVER_H
#define SYSTEM_SETTINGS_POLICY_FILE_OBSERVER_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SsPolicyFileObserver SsPolicyFileObserver;
typedef void (*SsPolicyFileObserverCallback)(gpointer user_data);

/*
 * Observe one atomically replaced policy file without creating configuration
 * directories merely by opening Settings. The observer watches the nearest
 * existing parent and rebinds as missing path components appear/disappear.
 */
SsPolicyFileObserver *ss_policy_file_observer_new(
    const char *path,
    SsPolicyFileObserverCallback callback,
    gpointer user_data);

void ss_policy_file_observer_free(SsPolicyFileObserver *observer);

#ifdef __cplusplus
}
#endif
#endif
