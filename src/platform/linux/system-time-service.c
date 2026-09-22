// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/system-time-service.h"

#include <string.h>

#define TIMEDATE_BUS "org.freedesktop.timedate1"
#define TIMEDATE_PATH "/org/freedesktop/timedate1"
#define TIMEDATE_IFACE "org.freedesktop.timedate1"

typedef struct SsSystemTimeCall {
    SsSystemTimeService *service;
    SsSystemTimeCompletionCallback callback;
    gpointer user_data;
} SsSystemTimeCall;

typedef struct SsSystemTimeNewCall {
    SsSystemTimeReadyCallback callback;
    gpointer user_data;
} SsSystemTimeNewCall;

struct SsSystemTimeService {
    GDBusProxy *proxy;
    gulong properties_changed_id;
    SsSystemTimeChangedCallback changed_callback;
    gpointer changed_user_data;
};

static bool variant_boolean(GDBusProxy *proxy,
                            const char *name,
                            bool fallback)
{
    g_autoptr(GVariant) value = NULL;

    if (proxy == NULL || name == NULL) {
        return fallback;
    }
    value = g_dbus_proxy_get_cached_property(proxy, name);
    if (value == NULL || !g_variant_is_of_type(
            value, G_VARIANT_TYPE_BOOLEAN)) {
        return fallback;
    }
    return g_variant_get_boolean(value) != FALSE;
}

bool ss_system_time_service_read(
    SsSystemTimeService *service,
    SsSystemTimeState *state)
{
    g_autoptr(GVariant) timezone = NULL;
    const char *timezone_text;

    if (state == NULL) {
        return false;
    }
    memset(state, 0, sizeof(*state));

    if (service == NULL || service->proxy == NULL) {
        return false;
    }

    timezone = g_dbus_proxy_get_cached_property(
        service->proxy, "Timezone");
    if (timezone == NULL ||
        !g_variant_is_of_type(timezone, G_VARIANT_TYPE_STRING)) {
        return false;
    }

    timezone_text = g_variant_get_string(timezone, NULL);
    if (timezone_text == NULL ||
        g_strlcpy(state->timezone,
                  timezone_text,
                  sizeof(state->timezone)) >= sizeof(state->timezone)) {
        return false;
    }

    state->available = true;
    state->can_ntp = variant_boolean(service->proxy, "CanNTP", false);
    state->ntp_enabled =
        variant_boolean(service->proxy, "NTP", false);
    return true;
}

static void emit_changed(SsSystemTimeService *service)
{
    SsSystemTimeState state;

    if (service == NULL || service->changed_callback == NULL ||
        !ss_system_time_service_read(service, &state)) {
        return;
    }

    service->changed_callback(
        service, &state, service->changed_user_data);
}

static void on_properties_changed(
    GDBusProxy *proxy G_GNUC_UNUSED,
    GVariant *changed_properties G_GNUC_UNUSED,
    const gchar *const *invalidated_properties G_GNUC_UNUSED,
    gpointer user_data)
{
    emit_changed(user_data);
}

static SsSystemTimeService *service_from_proxy(GDBusProxy *proxy)
{
    SsSystemTimeService *service;

    if (proxy == NULL) {
        return NULL;
    }

    service = g_new0(SsSystemTimeService, 1);
    service->proxy = proxy;
    service->properties_changed_id = g_signal_connect(
        service->proxy,
        "g-properties-changed",
        G_CALLBACK(on_properties_changed),
        service);
    return service;
}

static void new_proxy_finished(
    GObject *source G_GNUC_UNUSED,
    GAsyncResult *result,
    gpointer user_data)
{
    SsSystemTimeNewCall *call = user_data;
    g_autoptr(GError) error = NULL;
    GDBusProxy *proxy;
    SsSystemTimeService *service;

    proxy = g_dbus_proxy_new_for_bus_finish(result, &error);
    service = service_from_proxy(proxy);

    if (call != NULL && call->callback != NULL) {
        call->callback(
            service,
            error != NULL ? error->message : NULL,
            call->user_data);
    } else {
        ss_system_time_service_free(service);
    }
    g_free(call);
}

void ss_system_time_service_new_async(
    GCancellable *cancellable,
    SsSystemTimeReadyCallback callback,
    gpointer user_data)
{
    SsSystemTimeNewCall *call = g_new0(SsSystemTimeNewCall, 1);

    call->callback = callback;
    call->user_data = user_data;

    g_dbus_proxy_new_for_bus(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        TIMEDATE_BUS,
        TIMEDATE_PATH,
        TIMEDATE_IFACE,
        cancellable,
        new_proxy_finished,
        call);
}

void ss_system_time_service_free(SsSystemTimeService *service)
{
    if (service == NULL) {
        return;
    }
    if (service->proxy != NULL &&
        service->properties_changed_id != 0U) {
        g_signal_handler_disconnect(
            service->proxy, service->properties_changed_id);
    }
    g_clear_object(&service->proxy);
    g_free(service);
}

void ss_system_time_service_set_changed_callback(
    SsSystemTimeService *service,
    SsSystemTimeChangedCallback callback,
    gpointer user_data)
{
    if (service == NULL) {
        return;
    }
    service->changed_callback = callback;
    service->changed_user_data = user_data;
}

static void call_finished(GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
    SsSystemTimeCall *call = user_data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GVariant) reply = NULL;
    bool success = false;

    if (call == NULL) {
        return;
    }

    reply = g_dbus_proxy_call_finish(
        G_DBUS_PROXY(source), result, &error);
    success = reply != NULL;

    if (call->callback != NULL) {
        call->callback(
            call->service,
            success,
            error != NULL ? error->message : NULL,
            call->user_data);
    }
    g_free(call);
}

static void begin_call(SsSystemTimeService *service,
                       const char *method,
                       GVariant *parameters,
                       GCancellable *cancellable,
                       SsSystemTimeCompletionCallback callback,
                       gpointer user_data)
{
    SsSystemTimeCall *call;

    if (service == NULL || service->proxy == NULL ||
        method == NULL || parameters == NULL) {
        if (callback != NULL) {
            callback(service, false,
                     "The operating-system time service is unavailable.",
                     user_data);
        }
        return;
    }

    call = g_new0(SsSystemTimeCall, 1);
    call->service = service;
    call->callback = callback;
    call->user_data = user_data;

    g_dbus_proxy_call(
        service->proxy,
        method,
        parameters,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        cancellable,
        call_finished,
        call);
}

void ss_system_time_service_set_timezone_async(
    SsSystemTimeService *service,
    const char *timezone,
    GCancellable *cancellable,
    SsSystemTimeCompletionCallback callback,
    gpointer user_data)
{
    if (timezone == NULL || timezone[0] == '\0') {
        if (callback != NULL) {
            callback(service, false,
                     "A valid time-zone identifier is required.",
                     user_data);
        }
        return;
    }

    begin_call(
        service,
        "SetTimezone",
        g_variant_new("(sb)", timezone, TRUE),
        cancellable,
        callback,
        user_data);
}

void ss_system_time_service_set_ntp_async(
    SsSystemTimeService *service,
    bool enabled,
    GCancellable *cancellable,
    SsSystemTimeCompletionCallback callback,
    gpointer user_data)
{
    begin_call(
        service,
        "SetNTP",
        g_variant_new("(bb)", enabled ? TRUE : FALSE, TRUE),
        cancellable,
        callback,
        user_data);
}

void ss_system_time_service_set_time_async(
    SsSystemTimeService *service,
    int64_t unix_time_usec,
    GCancellable *cancellable,
    SsSystemTimeCompletionCallback callback,
    gpointer user_data)
{
    begin_call(
        service,
        "SetTime",
        g_variant_new("(xbb)",
                      (gint64)unix_time_usec,
                      FALSE,
                      TRUE),
        cancellable,
        callback,
        user_data);
}
