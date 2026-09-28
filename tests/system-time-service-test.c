// SPDX-License-Identifier: GPL-3.0-or-later
/* The fake timedated lives on a private bus. Never change the host clock. */
#include "system-settings/system-time-service.h"

#include <string.h>

static SsSystemTimeService *service;
static bool ready;
static bool completed;
static bool completion_success;
static gchar *completion_error;
static GDBusMethodInvocation *pending;
static const char *expected_method;
static SsSystemTimeState observed_state;
static guint observed_changes;

static GVariant *get_property(
    GDBusConnection *connection,
    const gchar *sender,
    const gchar *path,
    const gchar *interface,
    const gchar *property,
    GError **error,
    gpointer data)
{
    (void)connection;
    (void)sender;
    (void)path;
    (void)interface;
    (void)error;
    (void)data;

    if (g_str_equal(property, "Timezone")) {
        return g_variant_new_string("Australia/Melbourne");
    }
    return g_variant_new_boolean(TRUE);
}

static void method_call(
    GDBusConnection *connection,
    const gchar *sender,
    const gchar *path,
    const gchar *interface,
    const gchar *method,
    GVariant *parameters,
    GDBusMethodInvocation *invocation,
    gpointer data)
{
    gboolean interactive;

    (void)connection;
    (void)sender;
    (void)path;
    (void)interface;
    (void)data;

    g_assert_nonnull(expected_method);
    g_assert_cmpstr(method, ==, expected_method);
    g_assert_null(pending);

    if (g_str_equal(method, "SetNTP")) {
        gboolean enabled;
        g_variant_get(parameters, "(bb)", &enabled, &interactive);
        g_assert_false(enabled);
        g_assert_true(interactive);
    } else if (g_str_equal(method, "SetTimezone")) {
        const gchar *timezone = NULL;
        g_variant_get(parameters, "(&sb)", &timezone, &interactive);
        g_assert_cmpstr(timezone, ==, "Etc/UTC");
        g_assert_true(interactive);
    } else if (g_str_equal(method, "SetTime")) {
        gint64 usec;
        gboolean relative;
        g_variant_get(
            parameters, "(xbb)",
            &usec, &relative, &interactive);
        g_assert_cmpint(usec, ==, INT64_C(123456789000000));
        g_assert_false(relative);
        g_assert_true(interactive);
    } else {
        g_error("Unexpected fake timedated method: %s", method);
    }

    pending = g_object_ref(invocation);
}

static void on_ready(
    SsSystemTimeService *value,
    const char *error,
    gpointer data)
{
    (void)data;
    g_assert_null(error);
    g_assert_nonnull(value);
    service = value;
    ready = true;
}

static void on_changed(
    SsSystemTimeService *value,
    const SsSystemTimeState *state,
    gpointer data)
{
    (void)value;
    (void)data;
    g_assert_nonnull(state);
    observed_state = *state;
    observed_changes++;
}

static void on_record_complete(
    SsSystemTimeService *value G_GNUC_UNUSED,
    bool success,
    const char *error,
    gpointer data G_GNUC_UNUSED)
{
    completion_success = success;
    g_clear_pointer(&completion_error, g_free);
    completion_error = g_strdup(error);
    completed = true;
}

static void on_lifetime_complete(
    SsSystemTimeService *value,
    bool success,
    const char *error,
    gpointer data G_GNUC_UNUSED)
{
    SsSystemTimeState state;

    g_assert_true(success);
    g_assert_null(error);
    /* The initiating owner has already released its reference. */
    g_assert_true(ss_system_time_service_read(value, &state));
    g_assert_cmpstr(state.timezone, ==, "Australia/Melbourne");
    completed = true;
}

static void reset_completion(const char *method)
{
    expected_method = method;
    completed = false;
    completion_success = false;
    g_clear_pointer(&completion_error, g_free);
    g_assert_null(pending);
}

static void wait_pending(void)
{
    while (pending == NULL) {
        g_main_context_iteration(NULL, TRUE);
    }
}

static void wait_completed(void)
{
    while (!completed) {
        g_main_context_iteration(NULL, TRUE);
    }
}

static void finish_success(void)
{
    g_assert_nonnull(pending);
    g_dbus_method_invocation_return_value(
        pending, g_variant_new("()"));
    g_clear_object(&pending);
}

static void finish_denied(void)
{
    g_assert_nonnull(pending);
    g_dbus_method_invocation_return_dbus_error(
        pending,
        "org.freedesktop.DBus.Error.AccessDenied",
        "Authorisation denied");
    g_clear_object(&pending);
}

static void change_bus_name(
    GDBusConnection *connection,
    const char *method)
{
    g_autoptr(GVariant) reply = NULL;
    GVariant *parameters;

    if (g_str_equal(method, "RequestName")) {
        parameters = g_variant_new(
            "(su)", "org.freedesktop.timedate1", 0U);
    } else {
        g_assert_cmpstr(method, ==, "ReleaseName");
        parameters = g_variant_new(
            "(s)", "org.freedesktop.timedate1");
    }

    reply = g_dbus_connection_call_sync(
        connection,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        method,
        parameters,
        G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE,
        1000,
        NULL,
        NULL);
    g_assert_nonnull(reply);
}

static gboolean deadline(gpointer data)
{
    (void)data;
    g_error("Private timedated fixture timed out");
    return G_SOURCE_REMOVE;
}

int main(void)
{
    static const char xml[] =
        "<node><interface name='org.freedesktop.timedate1'>"
        "<property name='Timezone' type='s' access='read'/>"
        "<property name='CanNTP' type='b' access='read'/>"
        "<property name='NTP' type='b' access='read'/>"
        "<method name='SetNTP'>"
        "<arg type='b' direction='in'/>"
        "<arg type='b' direction='in'/>"
        "</method>"
        "<method name='SetTimezone'>"
        "<arg type='s' direction='in'/>"
        "<arg type='b' direction='in'/>"
        "</method>"
        "<method name='SetTime'>"
        "<arg type='x' direction='in'/>"
        "<arg type='b' direction='in'/>"
        "<arg type='b' direction='in'/>"
        "</method>"
        "</interface></node>";
    static const GDBusInterfaceVTable vtable = {
        method_call, get_property, NULL, {0}
    };
    g_autoptr(GTestDBus) bus =
        g_test_dbus_new(G_TEST_DBUS_NONE);

    g_test_dbus_up(bus);
    g_setenv(
        "DBUS_SYSTEM_BUS_ADDRESS",
        g_test_dbus_get_bus_address(bus),
        TRUE);

    g_autoptr(GDBusConnection) connection =
        g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    g_assert_nonnull(connection);
    change_bus_name(connection, "RequestName");

    g_autoptr(GDBusNodeInfo) info =
        g_dbus_node_info_new_for_xml(xml, NULL);
    guint registration = g_dbus_connection_register_object(
        connection,
        "/org/freedesktop/timedate1",
        info->interfaces[0],
        &vtable,
        NULL,
        NULL,
        NULL);
    g_assert_cmpuint(registration, !=, 0U);
    guint timeout = g_timeout_add_seconds(
        10U, deadline, NULL);

    ss_system_time_service_new_async(NULL, on_ready, NULL);
    while (!ready) {
        g_main_context_iteration(NULL, TRUE);
    }

    SsSystemTimeState initial;
    g_assert_true(ss_system_time_service_read(
        service, &initial));
    g_assert_true(initial.available);
    g_assert_cmpstr(
        initial.timezone, ==, "Australia/Melbourne");
    ss_system_time_service_set_changed_callback(
        service, on_changed, NULL);

    /* Cached values must not masquerade as authority after owner loss. */
    guint before = observed_changes;
    change_bus_name(connection, "ReleaseName");
    while (observed_changes == before ||
           observed_state.available) {
        g_main_context_iteration(NULL, TRUE);
    }
    g_assert_false(observed_state.available);

    before = observed_changes;
    change_bus_name(connection, "RequestName");
    while (observed_changes == before ||
           !observed_state.available) {
        g_main_context_iteration(NULL, TRUE);
    }
    g_assert_cmpstr(
        observed_state.timezone,
        ==,
        "Australia/Melbourne");

    reset_completion("SetTimezone");
    ss_system_time_service_set_timezone_async(
        service,
        "Etc/UTC",
        NULL,
        on_record_complete,
        NULL);
    wait_pending();
    finish_success();
    wait_completed();
    g_assert_true(completion_success);
    g_assert_null(completion_error);

    reset_completion("SetTime");
    ss_system_time_service_set_time_async(
        service,
        INT64_C(123456789000000),
        NULL,
        on_record_complete,
        NULL);
    wait_pending();
    finish_success();
    wait_completed();
    g_assert_true(completion_success);
    g_assert_null(completion_error);

    reset_completion("SetNTP");
    ss_system_time_service_set_ntp_async(
        service,
        false,
        NULL,
        on_record_complete,
        NULL);
    wait_pending();
    finish_denied();
    wait_completed();
    g_assert_false(completion_success);
    g_assert_nonnull(completion_error);
    g_assert_nonnull(strstr(
        completion_error, "Authorisation denied"));

    /* Outstanding calls own the service through their completion callback. */
    reset_completion("SetNTP");
    ss_system_time_service_set_ntp_async(
        service,
        false,
        NULL,
        on_lifetime_complete,
        NULL);
    wait_pending();
    ss_system_time_service_free(service);
    service = NULL;
    finish_success();
    wait_completed();

    g_clear_pointer(&completion_error, g_free);
    g_source_remove(timeout);
    g_dbus_connection_unregister_object(
        connection, registration);
    g_dbus_connection_close_sync(
        connection, NULL, NULL);
    g_clear_object(&connection);
    g_test_dbus_down(bus);
    return 0;
}
