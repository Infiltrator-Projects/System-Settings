// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_MANUAL_TIME_H
#define SYSTEM_SETTINGS_MANUAL_TIME_H

#if defined(SYSTEM_SETTINGS_GENERIC_SHELL)
#error "Generic Linux shell must not include Date & Time domain contracts."
#endif

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
 * Deprecated portable formatter: deliberately returns false now that
 * clock faces are owned by installed native resources. The Linux manual-time
 * UI formats through Common's clock presentation ABI. The separate legacy
 * parse operation below remains available while resource parsing is designed.
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
