// SPDX-License-Identifier: GPL-3.0-or-later
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

typedef void (*SsSystemTimeChangedCallback)(
    SsSystemTimeService *service,
    const SsSystemTimeState *state,
    gpointer user_data);

typedef void (*SsSystemTimeCompletionCallback)(
    SsSystemTimeService *service,
    bool success,
    const char *error_message,
    gpointer user_data);

/**
 * Open the systemd timedated service used by current Mint releases.
 *
 * The returned service is unprivileged. Protected mutations are authorised by
 * timedated/polkit per operation; System Settings never asks for or stores an
 * administrator password itself.
 */
SsSystemTimeService *ss_system_time_service_new(GError **error);
void ss_system_time_service_free(SsSystemTimeService *service);

bool ss_system_time_service_read(
    SsSystemTimeService *service,
    SsSystemTimeState *state);

void ss_system_time_service_set_changed_callback(
    SsSystemTimeService *service,
    SsSystemTimeChangedCallback callback,
    gpointer user_data);

void ss_system_time_service_set_timezone_async(
    SsSystemTimeService *service,
    const char *timezone,
    GCancellable *cancellable,
    SsSystemTimeCompletionCallback callback,
    gpointer user_data);

void ss_system_time_service_set_ntp_async(
    SsSystemTimeService *service,
    bool enabled,
    GCancellable *cancellable,
    SsSystemTimeCompletionCallback callback,
    gpointer user_data);

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
