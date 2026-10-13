// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file shell-module-catalog.c
 * @brief Manifest discovery and searchable presentation metadata for modules.
 */

#include "shell-module-catalog.h"

#include <stdbool.h>

#ifndef SYSTEM_SETTINGS_MODULE_DIR
#define SYSTEM_SETTINGS_MODULE_DIR "/usr/share/infiltrator/system-settings/modules"
#endif
#ifndef SYSTEM_SETTINGS_MODULE_SOURCE_DIR
#define SYSTEM_SETTINGS_MODULE_SOURCE_DIR ""
#endif

static GKeyFile *load_module_manifest(
    const char *manifest_filename,
    const char *manifest_directory)
{
    const bool explicit_directory =
        manifest_directory != NULL && manifest_directory[0] != '\0';
    const char *directories[] = {
        explicit_directory ? manifest_directory : SYSTEM_SETTINGS_MODULE_SOURCE_DIR,
        explicit_directory ? NULL : SYSTEM_SETTINGS_MODULE_DIR,
        NULL
    };

    if (manifest_filename == NULL || manifest_filename[0] == '\0') {
        return NULL;
    }

    for (size_t directory_index = 0U;
         directories[directory_index] != NULL;
         ++directory_index) {
        g_autofree gchar *path = NULL;
        g_autoptr(GKeyFile) key_file = NULL;
        g_autoptr(GError) error = NULL;

        if (directories[directory_index][0] == '\0') {
            continue;
        }
        path = g_build_filename(
            directories[directory_index], manifest_filename, NULL);
        key_file = g_key_file_new();
        if (g_key_file_load_from_file(
                key_file, path, G_KEY_FILE_NONE, &error)) {
            return g_steal_pointer(&key_file);
        }

        /* Missing is an expected fallback condition; malformed or unreadable
         * manifests are not. Keep the shell resilient while retaining enough
         * evidence to diagnose a broken installation or explicit qualification root. */
        if (error != NULL &&
            !(error->domain == G_FILE_ERROR &&
              error->code == G_FILE_ERROR_NOENT)) {
            g_warning(
                "Unable to load System Settings module manifest %s: %s",
                path,
                error->message);
        }
    }
    return NULL;
}

static void append_manifest_value(
    GString *search,
    GKeyFile *key_file,
    const char *group,
    const char *key)
{
    g_autofree gchar *value = NULL;

    if (search == NULL || key_file == NULL || group == NULL || key == NULL) {
        return;
    }
    value = g_key_file_get_string(key_file, group, key, NULL);
    if (value != NULL && value[0] != '\0') {
        g_string_append_c(search, ' ');
        g_string_append(search, value);
    }
}

static gchar *manifest_string_or_fallback(
    GKeyFile *key_file,
    const char *key,
    const char *fallback)
{
    g_autofree gchar *value = NULL;

    if (key_file != NULL && key != NULL) {
        value = g_key_file_get_string(key_file, "Module", key, NULL);
        if (value != NULL && value[0] != '\0') {
            return g_steal_pointer(&value);
        }
    }
    return g_strdup(fallback != NULL ? fallback : "");
}

static gchar *module_search_text_from_manifest(
    GKeyFile *key_file,
    const char *fallback)
{
    g_auto(GStrv) groups = NULL;
    gsize group_count = 0U;
    GString *search;

    if (key_file == NULL) {
        return g_strdup(fallback != NULL ? fallback : "");
    }

    search = g_string_new("");
    append_manifest_value(search, key_file, "Module", "Title");
    append_manifest_value(search, key_file, "Module", "Summary");
    append_manifest_value(search, key_file, "Search", "Keywords");
    groups = g_key_file_get_groups(key_file, &group_count);
    for (gsize group_index = 0U;
         group_index < group_count;
         ++group_index) {
        if (g_str_has_prefix(groups[group_index], "Target ")) {
            append_manifest_value(
                search, key_file, groups[group_index], "Title");
            append_manifest_value(
                search, key_file, groups[group_index], "Keywords");
        }
    }
    if (search->len > 0U) {
        return g_string_free(search, FALSE);
    }
    g_string_free(search, TRUE);
    return g_strdup(fallback != NULL ? fallback : "");
}

void ss_shell_module_metadata_clear(SsShellModuleMetadata *metadata)
{
    if (metadata == NULL) {
        return;
    }
    g_clear_pointer(&metadata->title, g_free);
    g_clear_pointer(&metadata->summary, g_free);
    g_clear_pointer(&metadata->icon, g_free);
    g_clear_pointer(&metadata->search_text, g_free);
}

void ss_shell_module_metadata_load(
    const char *manifest_filename,
    const char *page_id,
    const char *manifest_directory,
    SsShellModuleMetadata *out)
{
    g_autoptr(GKeyFile) manifest = NULL;
    g_autofree gchar *fallback_search = NULL;

    if (out == NULL) {
        return;
    }
    ss_shell_module_metadata_clear(out);
    manifest = load_module_manifest(manifest_filename, manifest_directory);
    out->title = manifest_string_or_fallback(manifest, "Title", page_id);
    out->summary = manifest_string_or_fallback(
        manifest, "Summary", "Settings module");
    out->icon = manifest_string_or_fallback(
        manifest, "Icon", "preferences-system-symbolic");
    fallback_search = g_strdup_printf(
        "%s %s",
        out->title != NULL ? out->title : "",
        out->summary != NULL ? out->summary : "");
    out->search_text = module_search_text_from_manifest(
        manifest, fallback_search);
}
