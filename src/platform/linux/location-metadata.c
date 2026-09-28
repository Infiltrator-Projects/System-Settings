// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file location-metadata.c
 * @brief Durable Linux persistence for user-selected locality metadata.
 *
 * The file is deliberately separate from native time-zone state. It stores
 * only the richer locality evidence that Mint/Linux does not otherwise expose
 * as one authoritative setting.
 */
#include "system-settings/location-metadata.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <math.h>
#include <string.h>

/* Resolve through GLib's XDG configuration directory contract. */
char *ss_location_metadata_path_alloc(void)
{
    return g_build_filename(
        g_get_user_config_dir(),
        "infiltrator",
        "system-settings",
        "location.ini",
        NULL);
}

static gchar *metadata_pending_path(void)
{
    return g_build_filename(
        g_get_user_config_dir(),
        "infiltrator",
        "system-settings",
        "location.pending",
        NULL);
}

/* Validate fixed arrays before passing them to NUL-terminated GLib APIs. */
static bool metadata_valid(const SsLocationMetadata *value)
{
    return value != NULL && value->display_name[0] != '\0' &&
        memchr(value->display_name, '\0', sizeof(value->display_name)) != NULL &&
        memchr(value->country_code, '\0', sizeof(value->country_code)) != NULL &&
        memchr(value->timezone_id, '\0', sizeof(value->timezone_id)) != NULL &&
        g_utf8_validate(value->display_name, -1, NULL) &&
        g_utf8_validate(value->country_code, -1, NULL) &&
        g_utf8_validate(value->timezone_id, -1, NULL) &&
        isfinite(value->latitude) && isfinite(value->longitude) &&
        value->latitude >= -90.0 && value->latitude <= 90.0 &&
        value->longitude >= -180.0 && value->longitude <= 180.0;
}

static bool metadata_load_path(
    const char *path,
    SsLocationMetadata *metadata)
{
    SsLocationMetadata candidate = {0};
    g_autoptr(GKeyFile) key_file = NULL;
    g_autofree gchar *name = NULL;
    g_autofree gchar *country = NULL;
    g_autofree gchar *timezone = NULL;
    g_autoptr(GError) error = NULL;

    if (metadata == NULL) {
        return false;
    }
    memset(metadata, 0, sizeof(*metadata));

    if (path == NULL || path[0] == '\0') {
        return false;
    }

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

    candidate.latitude = g_key_file_get_double(
        key_file, "Location", "Latitude", &error);
    if (error != NULL) {
        return false;
    }
    candidate.longitude = g_key_file_get_double(
        key_file, "Location", "Longitude", &error);
    if (error != NULL) {
        return false;
    }

    if (name == NULL ||
        g_strlcpy(candidate.display_name,
                  name,
                  sizeof(candidate.display_name)) >=
            sizeof(candidate.display_name)) {
        return false;
    }
    if (country != NULL) {
        if (g_strlcpy(candidate.country_code, country,
                      sizeof(candidate.country_code)) >= sizeof(candidate.country_code)) {
            return false;
        }
    }
    if (timezone != NULL) {
        if (g_strlcpy(candidate.timezone_id, timezone,
                      sizeof(candidate.timezone_id)) >= sizeof(candidate.timezone_id)) {
            return false;
        }
    }
    if (!metadata_valid(&candidate)) {
        return false;
    }
    *metadata = candidate;
    return true;
}

static bool metadata_save_path(
    const char *path,
    const SsLocationMetadata *metadata)
{
    g_autoptr(GKeyFile) key_file = NULL;
    g_autofree gchar *data = NULL;
    g_autofree gchar *directory = NULL;
    gsize length = 0U;

    if (path == NULL || path[0] == '\0' || !metadata_valid(metadata)) {
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

    directory = g_path_get_dirname(path);
    if (g_mkdir_with_parents(directory, 0700) != 0 && errno != EEXIST) {
        return false;
    }
    /*
     * g_mkdir_with_parents() does not tighten an already existing directory.
     * Locality metadata is private user state, so repair the leaf directory
     * mode before publishing the file as well as creating new directories
     * with 0700.
     */
    if (g_chmod(directory, 0700) != 0) {
        return false;
    }

    /*
     * CONSISTENT requests replacement through a temporary file and DURABLE
     * requests publication to stable storage. Metadata is private user state,
     * hence mode 0600.
     */
    return g_file_set_contents_full(
        path,
        data,
        (gssize)length,
        G_FILE_SET_CONTENTS_CONSISTENT |
            G_FILE_SET_CONTENTS_DURABLE,
        0600,
        NULL);
}


bool ss_location_metadata_load(SsLocationMetadata *metadata)
{
    g_autofree gchar *path = ss_location_metadata_path_alloc();
    return metadata_load_path(path, metadata);
}

bool ss_location_metadata_save(const SsLocationMetadata *metadata)
{
    g_autofree gchar *path = ss_location_metadata_path_alloc();
    return metadata_save_path(path, metadata);
}

bool ss_location_metadata_stage(const SsLocationMetadata *metadata)
{
    g_autofree gchar *path = metadata_pending_path();
    return metadata_save_path(path, metadata);
}

void ss_location_metadata_discard_staged(void)
{
    g_autofree gchar *path = metadata_pending_path();

    if (path != NULL && g_remove(path) != 0 && errno != ENOENT) {
        g_warning("Unable to remove stale locality transaction journal.");
    }
}

bool ss_location_metadata_finish_staged(void)
{
    SsLocationMetadata staged = {0};
    g_autofree gchar *path = metadata_pending_path();

    if (path == NULL || !metadata_load_path(path, &staged)) {
        return false;
    }
    if (!ss_location_metadata_save(&staged)) {
        return false;
    }
    if (g_remove(path) != 0 && errno != ENOENT) {
        return false;
    }
    return true;
}

bool ss_location_metadata_recover(
    bool location_configured,
    double latitude,
    double longitude)
{
    SsLocationMetadata staged = {0};
    g_autofree gchar *path = metadata_pending_path();
    double latitude_difference;
    double longitude_difference;

    if (path == NULL || !g_file_test(path, G_FILE_TEST_EXISTS)) {
        return true;
    }
    if (!metadata_load_path(path, &staged)) {
        ss_location_metadata_discard_staged();
        return true;
    }

    latitude_difference = staged.latitude - latitude;
    longitude_difference = staged.longitude - longitude;
    if (latitude_difference < 0.0) {
        latitude_difference = -latitude_difference;
    }
    if (longitude_difference < 0.0) {
        longitude_difference = -longitude_difference;
    }

    if (location_configured &&
        latitude_difference < 0.000001 &&
        longitude_difference < 0.000001) {
        return ss_location_metadata_finish_staged();
    }

    ss_location_metadata_discard_staged();
    return true;
}
