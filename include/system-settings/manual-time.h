// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_MANUAL_TIME_H
#define SYSTEM_SETTINGS_MANUAL_TIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return whether System Settings can round-trip manual wall-clock input for
 * the selected presentation mode without guessing.
 */
bool ss_manual_time_mode_supported(const char *clock_mode);

/**
 * Format one canonical instant using the selected reversible clock mode.
 *
 * "standard" follows @desktop_use_24h. Explicit 12-hour, 24-hour and decimal
 * modes ignore that desktop preference. Other clock systems deliberately fail
 * until a mathematically sound inverse is available.
 */
bool ss_manual_time_format(const char *clock_mode,
                           bool desktop_use_24h,
                           int64_t unix_microseconds,
                           int32_t utc_offset_seconds,
                           bool show_seconds,
                           char *buffer,
                           size_t capacity);

/**
 * Parse one selected-clock representation into microseconds after local
 * midnight. The result is in [0, 86400000000), unchanged on failure.
 * Accept H:M[:S] with one or two ASCII digits per field, plus AM/PM
 * (case-insensitive, optional separating spaces/tabs) in 12-hour mode.
 * Signs, leading/trailing whitespace, oversized fields and garbage fail.
 */
bool ss_manual_time_parse(const char *clock_mode,
                          bool desktop_use_24h,
                          const char *text,
                          int64_t *microseconds_of_day);

#ifdef __cplusplus
}
#endif

#endif
