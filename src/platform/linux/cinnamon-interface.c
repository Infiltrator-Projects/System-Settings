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
