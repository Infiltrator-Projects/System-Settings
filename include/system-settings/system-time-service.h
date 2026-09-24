// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system-time-service.h
 * @brief Asynchronous unprivileged adapter for systemd timedated.
 *
 * The adapter never owns credentials or elevates the Settings process.
 * Protected operations are submitted to timedated with interactive
 * authorisation enabled so the platform policy agent remains responsible for
 * authentication.
 */
#ifndef SYSTEM_SETTINGS_SYSTEM_TIME_SERVICE_H
#define SYSTEM_SETTINGS_SYSTEM_TIME_SERVICE_H

#include <gio/gio.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SS_SYSTEM_TIMEZONE_CAPACITY 128U

typedef struct SsSystemTimeService SsSystemTimeService;

typedef struct SsSystemTimeState {
    char timezone[SS_SYSTEM_TIMEZONE_CAPACITY];
    bool available;
    bool can_ntp;
    bool ntp_enabled;
} SsSystemTimeState;

/**
 * State-change callback.
 *
 * service and state are borrowed for the duration of the callback. user_data
 * remains caller-owned.
 */
typedef void (*SsSystemTimeChangedCallback)(
    SsSystemTimeService *service,
    const SsSystemTimeState *state,
    gpointer user_data);

/**
 * Completion for one mutation request.
 *
 * error_message is borrowed and valid only during the callback. Cancellation
 * is reported as an unsuccessful completion; callers that supersede requests
 * should generation-gate their own UI state.
 */
typedef void (*SsSystemTimeCompletionCallback)(
    SsSystemTimeService *service,
    bool success,
    const char *error_message,
    gpointer user_data);

/**
 * Completion for asynchronous service construction.
 *
 * A non-NULL service is transferred to the callback and must eventually be
 * released with ss_system_time_service_free().
 */
typedef void (*SsSystemTimeReadyCallback)(
    SsSystemTimeService *service,
    const char *error_message,
    gpointer user_data);

/**
 * Open the systemd timedated service used by current Mint releases.
 *
 * The returned service is unprivileged. Protected mutations are authorised by
 * timedated/polkit per operation; System Settings never asks for or stores an
 * administrator password itself.
 */
void ss_system_time_service_new_async(
    GCancellable *cancellable,
    SsSystemTimeReadyCallback callback,
    gpointer user_data);
void ss_system_time_service_free(SsSystemTimeService *service);

/**
 * Snapshot cached timedated properties without performing synchronous D-Bus
 * I/O. The output is cleared first and populated only when the required
 * Timezone property is available and valid.
 */
bool ss_system_time_service_read(
    SsSystemTimeService *service,
    SsSystemTimeState *state);

/**
 * Install the single state-change observer for this service.
 * callback and user_data are borrowed; a later call replaces both.
 */
void ss_system_time_service_set_changed_callback(
    SsSystemTimeService *service,
    SsSystemTimeChangedCallback callback,
    gpointer user_data);

/** Request one absolute IANA time-zone change through timedated. */
void ss_system_time_service_set_timezone_async(
    SsSystemTimeService *service,
    const char *timezone,
    GCancellable *cancellable,
    SsSystemTimeCompletionCallback callback,
    gpointer user_data);

/** Request a change to timedated's network-time state. */
void ss_system_time_service_set_ntp_async(
    SsSystemTimeService *service,
    bool enabled,
    GCancellable *cancellable,
    SsSystemTimeCompletionCallback callback,
    gpointer user_data);

/**
 * Set the absolute system clock.
 *
 * unix_time_usec is Unix time in microseconds. The timedated relative flag is
 * deliberately false; interactive authorisation remains enabled.
 */
void ss_system_time_service_set_time_async(
    SsSystemTimeService *service,
    int64_t unix_time_usec,
    GCancellable *cancellable,
    SsSystemTimeCompletionCallback callback,
    gpointer user_data);

#ifdef __cplusplus
}
#endif
#endif
