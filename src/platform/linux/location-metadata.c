// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/location-metadata.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <string.h>

static gchar *metadata_path(void)
{
    return g_build_filename(
        g_get_user_config_dir(),
        "infiltrator",
        "system-settings",
        "location.ini",
        NULL);
}

bool ss_location_metadata_load(SsLocationMetadata *metadata)
{
    g_autoptr(GKeyFile) key_file = NULL;
    g_autofree gchar *path = NULL;
    g_autofree gchar *name = NULL;
    g_autofree gchar *country = NULL;
    g_autofree gchar *timezone = NULL;
    g_autoptr(GError) error = NULL;

    if (metadata == NULL) {
        return false;
    }
    memset(metadata, 0, sizeof(*metadata));

    path = metadata_path();
    key_file = g_key_file_new();
    if (!g_key_file_load_from_file(
            key_file, path, G_KEY_FILE_NONE, &error)) {
        return false;
    }

    name = g_key_file_get_string(
        key_file, "Location", "Name", &error);
    if (error != NULL) {
        return false;
    }
    country = g_key_file_get_string(
        key_file, "Location", "CountryCode", NULL);
    timezone = g_key_file_get_string(
        key_file, "Location", "Timezone", NULL);

    metadata->latitude = g_key_file_get_double(
        key_file, "Location", "Latitude", &error);
    if (error != NULL) {
        return false;
    }
    metadata->longitude = g_key_file_get_double(
        key_file, "Location", "Longitude", &error);
    if (error != NULL) {
        return false;
    }

    if (name == NULL ||
        g_strlcpy(metadata->display_name,
                  name,
                  sizeof(metadata->display_name)) >=
            sizeof(metadata->display_name)) {
        return false;
    }
    if (country != NULL) {
        (void)g_strlcpy(metadata->country_code,
                        country,
                        sizeof(metadata->country_code));
    }
    if (timezone != NULL) {
        (void)g_strlcpy(metadata->timezone_id,
                        timezone,
                        sizeof(metadata->timezone_id));
    }
    return true;
}

bool ss_location_metadata_save(const SsLocationMetadata *metadata)
{
    g_autoptr(GKeyFile) key_file = NULL;
    g_autofree gchar *data = NULL;
    g_autofree gchar *path = NULL;
    g_autofree gchar *directory = NULL;
    gsize length = 0U;

    if (metadata == NULL || metadata->display_name[0] == '\0' ||
        metadata->latitude < -90.0 || metadata->latitude > 90.0 ||
        metadata->longitude < -180.0 || metadata->longitude > 180.0) {
        return false;
    }

    key_file = g_key_file_new();
    g_key_file_set_string(
        key_file, "Location", "Name", metadata->display_name);
    g_key_file_set_string(
        key_file, "Location", "CountryCode", metadata->country_code);
    g_key_file_set_string(
        key_file, "Location", "Timezone", metadata->timezone_id);
    g_key_file_set_double(
        key_file, "Location", "Latitude", metadata->latitude);
    g_key_file_set_double(
        key_file, "Location", "Longitude", metadata->longitude);

    data = g_key_file_to_data(key_file, &length, NULL);
    if (data == NULL) {
        return false;
    }

    path = metadata_path();
    directory = g_path_get_dirname(path);
    if (g_mkdir_with_parents(directory, 0700) != 0 && errno != EEXIST) {
        return false;
    }

    return g_file_set_contents_full(
        path,
        data,
        (gssize)length,
        G_FILE_SET_CONTENTS_CONSISTENT |
            G_FILE_SET_CONTENTS_DURABLE,
        0600,
        NULL);
}
