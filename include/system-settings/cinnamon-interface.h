// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_CINNAMON_INTERFACE_H
#define SYSTEM_SETTINGS_CINNAMON_INTERFACE_H

#include <gio/gio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a Cinnamon desktop-interface settings object only when the schema and
 * temporal keys required by System Settings are available.
 *
 * Returns: (transfer full) (nullable): a settings object owned by the caller
 */
GSettings *ss_cinnamon_interface_settings_new(void);

#ifdef __cplusplus
}
#endif
#endif
