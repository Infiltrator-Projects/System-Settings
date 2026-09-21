// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_CINNAMON_INTERFACE_H
#define SYSTEM_SETTINGS_CINNAMON_INTERFACE_H

#include <gio/gio.h>
#include <stdbool.h>

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

/** Check whether one Cinnamon interface key is present on this release. */
bool ss_cinnamon_interface_has_key(const char *key);

/** Read/write optional Cinnamon compatibility keys without guessing types. */
bool ss_cinnamon_interface_get_boolean(
    GSettings *settings,
    const char *key,
    bool *value);
bool ss_cinnamon_interface_set_boolean(
    GSettings *settings,
    const char *key,
    bool value);
bool ss_cinnamon_interface_get_first_day(
    GSettings *settings,
    int *value);
bool ss_cinnamon_interface_set_first_day(
    GSettings *settings,
    int value);

/**
 * Update Cinnamon's conventional 12/24-hour preference and mirror GNOME's
 * documented clock-format key, matching Mint's own Date & Time behaviour.
 */
bool ss_cinnamon_interface_set_clock_use_24h(
    GSettings *settings,
    bool use_24h);

#ifdef __cplusplus
}
#endif
#endif
