// SPDX-License-Identifier: GPL-3.0-or-later
/* The fake timedated lives on a private bus. Never change the host clock. */
#include "system-settings/system-time-service.h"

static SsSystemTimeService *service;
static bool ready;
static bool completed;
static GDBusMethodInvocation *pending;
static SsSystemTimeState observed_state;
static guint observed_changes;

static GVariant *get_property(GDBusConnection *connection, const gchar *sender,
    const gchar *path, const gchar *interface, const gchar *property,
    GError **error, gpointer data)
{
    (void)connection; (void)sender; (void)path; (void)interface; (void)error; (void)data;
    if (g_str_equal(property, "Timezone")) return g_variant_new_string("Australia/Melbourne");
    return g_variant_new_boolean(TRUE);
}

static void method_call(GDBusConnection *connection, const gchar *sender,
    const gchar *path, const gchar *interface, const gchar *method,
    GVariant *parameters, GDBusMethodInvocation *invocation, gpointer data)
{
    gboolean enabled, interactive;
    (void)connection; (void)sender; (void)path; (void)interface; (void)data;
    g_assert_cmpstr(method, ==, "SetNTP");
    g_variant_get(parameters, "(bb)", &enabled, &interactive);
    g_assert_false(enabled);
    g_assert_true(interactive);
    pending = g_object_ref(invocation);
}

static void on_ready(SsSystemTimeService *value, const char *error, gpointer data)
{
    (void)data;
    g_assert_null(error);
    g_assert_nonnull(value);
    service = value;
    ready = true;
}

static void on_changed(SsSystemTimeService *value,
                       const SsSystemTimeState *state,
                       gpointer data)
{
    (void)value;
    (void)data;
    g_assert_nonnull(state);
    observed_state = *state;
    observed_changes++;
}

static void on_complete(SsSystemTimeService *value, bool success,
                        const char *error, gpointer data)
{
    (void)data;
    SsSystemTimeState state;
    g_assert_true(success);
    g_assert_null(error);
    /* The caller already released its ownership before this callback. */
    g_assert_true(ss_system_time_service_read(value, &state));
    g_assert_cmpstr(state.timezone, ==, "Australia/Melbourne");
    completed = true;
}

static void change_bus_name(GDBusConnection *connection, const char *method)
{
    g_autoptr(GVariant) reply = NULL;
    GVariant *parameters;

    if (g_str_equal(method, "RequestName")) {
        parameters = g_variant_new("(su)", "org.freedesktop.timedate1", 0U);
    } else {
        g_assert_cmpstr(method, ==, "ReleaseName");
        parameters = g_variant_new("(s)", "org.freedesktop.timedate1");
    }

    reply = g_dbus_connection_call_sync(connection,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        method, parameters, G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, NULL);
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
        "<method name='SetNTP'><arg type='b' direction='in'/>"
        "<arg type='b' direction='in'/></method></interface></node>";
    static const GDBusInterfaceVTable vtable = {method_call, get_property, NULL, {0}};
    g_autoptr(GTestDBus) bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(bus);
    g_setenv("DBUS_SYSTEM_BUS_ADDRESS", g_test_dbus_get_bus_address(bus), TRUE);
    g_autoptr(GDBusConnection) connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    g_assert_nonnull(connection);
    change_bus_name(connection, "RequestName");
    g_autoptr(GDBusNodeInfo) info = g_dbus_node_info_new_for_xml(xml, NULL);
    guint registration = g_dbus_connection_register_object(connection,
        "/org/freedesktop/timedate1", info->interfaces[0], &vtable, NULL, NULL, NULL);
    g_assert_cmpuint(registration, !=, 0U);
    guint timeout = g_timeout_add_seconds(10U, deadline, NULL);

    ss_system_time_service_new_async(NULL, on_ready, NULL);
    while (!ready) g_main_context_iteration(NULL, TRUE);

    SsSystemTimeState initial;
    g_assert_true(ss_system_time_service_read(service, &initial));
    g_assert_true(initial.available);
    g_assert_cmpstr(initial.timezone, ==, "Australia/Melbourne");
    ss_system_time_service_set_changed_callback(service, on_changed, NULL);

    /* Losing the well-known name must surface unavailable state even if the
     * proxy still holds old cached property values. */
    guint before = observed_changes;
    change_bus_name(connection, "ReleaseName");
    while (observed_changes == before || observed_state.available) {
        g_main_context_iteration(NULL, TRUE);
    }
    g_assert_false(observed_state.available);

    /* Reacquiring the name must restore an authoritative readable snapshot. */
    before = observed_changes;
    change_bus_name(connection, "RequestName");
    while (observed_changes == before || !observed_state.available) {
        g_main_context_iteration(NULL, TRUE);
    }
    g_assert_cmpstr(observed_state.timezone, ==, "Australia/Melbourne");

    ss_system_time_service_set_ntp_async(service, false, NULL, on_complete, NULL);
    while (pending == NULL) g_main_context_iteration(NULL, TRUE);
    ss_system_time_service_free(service);
    service = NULL;
    g_dbus_method_invocation_return_value(pending, g_variant_new("()"));
    g_clear_object(&pending);
    while (!completed) g_main_context_iteration(NULL, TRUE);

    g_source_remove(timeout);
    g_dbus_connection_unregister_object(connection, registration);
    g_dbus_connection_close_sync(connection, NULL, NULL);
    g_clear_object(&connection);
    g_test_dbus_down(bus);
    return 0;
}
