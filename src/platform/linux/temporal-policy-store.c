// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/temporal-policy-store.h"
#include <infiltratr/posix.h>
#include <gio/gio.h>
#include <stddef.h>
#include <string.h>

#define SS_PATH_CAPACITY 4096U
#define SS_POLICY_CAPACITY 1024U

static bool policy_paths(char *directory, size_t directory_size,
                         char *path, size_t path_size)
{
    char config_home[SS_PATH_CAPACITY];
    return infiltratr_xdg_config_home(config_home, sizeof(config_home)) &&
           infiltratr_path_join(directory, directory_size, config_home, "infiltrator") &&
           infiltratr_path_join(path, path_size, directory, "presentation.conf");
}

static GSettings *cinnamon_interface_settings(void)
{
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    GSettingsSchema *schema;
    GSettings *settings;

    if (source == NULL)
        return NULL;

    schema = g_settings_schema_source_lookup(
        source, "org.cinnamon.desktop.interface", TRUE);
    if (schema == NULL)
        return NULL;

    if (!g_settings_schema_has_key(schema, "clock-use-24h") ||
        !g_settings_schema_has_key(schema, "clock-show-seconds")) {
        g_settings_schema_unref(schema);
        return NULL;
    }

    settings = g_settings_new_full(schema, NULL, NULL);
    g_settings_schema_unref(schema);
    return settings;
}

static void load_cinnamon_defaults(InfiltratrTemporalPolicyV3 *policy)
{
    GSettings *settings = cinnamon_interface_settings();
    const char *mode;

    if (policy == NULL || settings == NULL)
        return;

    mode = g_settings_get_boolean(settings, "clock-use-24h")
        ? "standard-24" : "standard-12";
    if (strlen(mode) < sizeof(policy->clock_mode))
        strcpy(policy->clock_mode, mode);
    policy->show_seconds =
        g_settings_get_boolean(settings, "clock-show-seconds") != FALSE;
    g_object_unref(settings);
}

static void mirror_cinnamon_compatibility(
    const InfiltratrTemporalPolicyV3 *policy)
{
    GSettings *settings = cinnamon_interface_settings();
    gboolean ok = TRUE;

    if (policy == NULL || settings == NULL)
        return;

    if (strcmp(policy->clock_mode, "standard-24") == 0) {
        ok = g_settings_set_boolean(settings, "clock-use-24h", TRUE) && ok;
    } else if (strcmp(policy->clock_mode, "standard-12") == 0) {
        ok = g_settings_set_boolean(settings, "clock-use-24h", FALSE) && ok;
    }

    ok = g_settings_set_boolean(
        settings, "clock-show-seconds",
        policy->show_seconds ? TRUE : FALSE) && ok;
    g_settings_sync();

    if (!ok)
        g_warning("Unable to mirror temporal compatibility settings to Cinnamon.");
    g_object_unref(settings);
}

static bool platform_load(InfiltratrTemporalPolicyV3 *policy, bool *found)
{
    char directory[SS_PATH_CAPACITY];
    char path[SS_PATH_CAPACITY];
    char text[SS_POLICY_CAPACITY];
    InfiltratrIoResult result;

    if (policy == NULL || found == NULL ||
        !infiltratr_temporal_policy_v3_default(policy) ||
        !policy_paths(directory, sizeof(directory), path, sizeof(path)))
        return false;

    *found = false;
    result = infiltratr_read_text_file_ex(path, text, sizeof(text), NULL);
    if (result == INFILTRATR_IO_NOT_FOUND) {
        load_cinnamon_defaults(policy);
        return true;
    }
    if (result != INFILTRATR_IO_OK ||
        !infiltratr_temporal_policy_v3_parse(text, policy))
        return false;
    *found = true;
    return true;
}

static bool platform_save(const InfiltratrTemporalPolicyV3 *policy)
{
    char directory[SS_PATH_CAPACITY];
    char path[SS_PATH_CAPACITY];
    char text[SS_POLICY_CAPACITY];
    size_t length = 0U;

    if (policy == NULL ||
        !policy_paths(directory, sizeof(directory), path, sizeof(path)) ||
        infiltratr_mkdir_parents(directory, 0700U) != 0 ||
        !infiltratr_temporal_policy_v3_serialize(policy, text, sizeof(text), &length))
        return false;

    if (infiltratr_atomic_file_write_bytes(
            path, INFILTRATR_ATOMIC_FILE_PRIVATE, text, length) != 0)
        return false;

    /*
     * Keep conventional Mint consumers aligned where the richer Infiltrator
     * policy has an exact Cinnamon representation. Extended clock/calendar
     * modes remain Infiltrator-only rather than being forced into a false
     * platform equivalent.
     */
    mirror_cinnamon_compatibility(policy);
    return true;
}


const SsTemporalPolicyStore *ss_platform_temporal_policy_store(void)
{
    static const SsTemporalPolicyStore store = {
        .load = platform_load,
        .save = platform_save
    };
    return &store;
}
