// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/cinnamon-interface.h"

GSettings *ss_cinnamon_interface_settings_new(void)
{
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    GSettingsSchema *schema;
    GSettings *settings;

    if (source == NULL) {
        return NULL;
    }

    schema = g_settings_schema_source_lookup(
        source, "org.cinnamon.desktop.interface", TRUE);
    if (schema == NULL) {
        return NULL;
    }

    if (!g_settings_schema_has_key(schema, "clock-use-24h") ||
        !g_settings_schema_has_key(schema, "clock-show-seconds")) {
        g_settings_schema_unref(schema);
        return NULL;
    }

    settings = g_settings_new_full(schema, NULL, NULL);
    g_settings_schema_unref(schema);
    return settings;
}

static GSettingsSchema *lookup_interface_schema(void)
{
    GSettingsSchemaSource *source =
        g_settings_schema_source_get_default();

    if (source == NULL) {
        return NULL;
    }
    return g_settings_schema_source_lookup(
        source, "org.cinnamon.desktop.interface", TRUE);
}

bool ss_cinnamon_interface_has_key(const char *key)
{
    GSettingsSchema *schema;
    bool result;

    if (key == NULL) {
        return false;
    }
    schema = lookup_interface_schema();
    if (schema == NULL) {
        return false;
    }
    result = g_settings_schema_has_key(schema, key) != FALSE;
    g_settings_schema_unref(schema);
    return result;
}

bool ss_cinnamon_interface_get_boolean(
    GSettings *settings,
    const char *key,
    bool *value)
{
    g_autoptr(GVariant) variant = NULL;

    if (settings == NULL || key == NULL || value == NULL ||
        !ss_cinnamon_interface_has_key(key)) {
        return false;
    }
    variant = g_settings_get_value(settings, key);
    if (variant == NULL ||
        !g_variant_is_of_type(variant, G_VARIANT_TYPE_BOOLEAN)) {
        return false;
    }
    *value = g_variant_get_boolean(variant) != FALSE;
    return true;
}

bool ss_cinnamon_interface_set_boolean(
    GSettings *settings,
    const char *key,
    bool value)
{
    if (settings == NULL || key == NULL ||
        !ss_cinnamon_interface_has_key(key)) {
        return false;
    }
    return g_settings_set_boolean(
        settings, key, value ? TRUE : FALSE) != FALSE;
}

bool ss_cinnamon_interface_get_first_day(
    GSettings *settings,
    int *value)
{
    g_autoptr(GVariant) variant = NULL;

    if (settings == NULL || value == NULL ||
        !ss_cinnamon_interface_has_key("first-day-of-week")) {
        return false;
    }
    variant = g_settings_get_value(
        settings, "first-day-of-week");
    if (variant == NULL ||
        !g_variant_is_of_type(variant, G_VARIANT_TYPE_INT32)) {
        return false;
    }
    *value = g_variant_get_int32(variant);
    return true;
}

bool ss_cinnamon_interface_set_first_day(
    GSettings *settings,
    int value)
{
    if (settings == NULL ||
        (value != 7 && value != 0 && value != 1) ||
        !ss_cinnamon_interface_has_key("first-day-of-week")) {
        return false;
    }
    return g_settings_set_int(
        settings, "first-day-of-week", value) != FALSE;
}

bool ss_cinnamon_interface_set_clock_use_24h(
    GSettings *settings,
    bool use_24h)
{
    GSettingsSchemaSource *source;
    GSettingsSchema *schema;
    GSettings *gnome = NULL;
    bool ok;

    ok = ss_cinnamon_interface_set_boolean(
        settings, "clock-use-24h", use_24h);

    source = g_settings_schema_source_get_default();
    if (source == NULL) {
        return ok;
    }
    schema = g_settings_schema_source_lookup(
        source, "org.gnome.desktop.interface", TRUE);
    if (schema == NULL) {
        return ok;
    }

    if (g_settings_schema_has_key(schema, "clock-format")) {
        gnome = g_settings_new_full(schema, NULL, NULL);
        ok = g_settings_set_string(
                 gnome,
                 "clock-format",
                 use_24h ? "24h" : "12h") != FALSE && ok;
        g_object_unref(gnome);
    }
    g_settings_schema_unref(schema);
    g_settings_sync();
    return ok;
}
