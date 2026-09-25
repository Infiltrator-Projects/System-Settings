// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/location-metadata.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "Location metadata test failed: %s (%s:%d)\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static double absolute_difference(double left, double right)
{
    const double difference = left - right;
    return difference < 0.0 ? -difference : difference;
}

int main(void)
{
    g_autofree gchar *temporary_root = NULL;
    g_autofree gchar *expected_file = NULL;
    SsLocationMetadata saved = {0};
    SsLocationMetadata loaded = {0};
    g_autoptr(GError) error = NULL;

    temporary_root = g_dir_make_tmp(
        "system-settings-location-test-XXXXXX", &error);
    CHECK(temporary_root != NULL);
    CHECK(g_setenv("XDG_CONFIG_HOME", temporary_root, TRUE));

    g_strlcpy(saved.display_name,
              "Mooroopna, Victoria, Australia",
              sizeof(saved.display_name));
    g_strlcpy(saved.country_code, "AU",
              sizeof(saved.country_code));
    g_strlcpy(saved.timezone_id, "Australia/Melbourne",
              sizeof(saved.timezone_id));
    saved.latitude = -36.3949;
    saved.longitude = 145.3610;

    CHECK(ss_location_metadata_save(&saved));
    CHECK(ss_location_metadata_load(&loaded));
    CHECK(strcmp(loaded.display_name, saved.display_name) == 0);
    CHECK(strcmp(loaded.country_code, saved.country_code) == 0);
    CHECK(strcmp(loaded.timezone_id, saved.timezone_id) == 0);
    CHECK(absolute_difference(
              loaded.latitude, saved.latitude) < 0.000001);
    CHECK(absolute_difference(
              loaded.longitude, saved.longitude) < 0.000001);

    expected_file = g_build_filename(
        temporary_root,
        "infiltrator",
        "system-settings",
        "location.ini",
        NULL);
    CHECK(g_file_test(expected_file, G_FILE_TEST_IS_REGULAR));

    saved.latitude = NAN;
    CHECK(!ss_location_metadata_save(&saved));
    saved.latitude = -36.3949;
    memset(saved.display_name, 'x', sizeof(saved.display_name));
    CHECK(!ss_location_metadata_save(&saved));
    static const char *const malformed[] = {
        "[Location]\nName=Bad\nLatitude=nan\nLongitude=0\n",
        "[Location]\nName=Bad\nLatitude=91\nLongitude=0\n",
        "[Location]\nName=\nLatitude=0\nLongitude=0\n",
        "[Location]\nName=Bad\nLatitude=0\nLongitude=inf\n",
        "[Location]\nName=Bad\nCountryCode=TOOLONGCOUNTRY\nLatitude=0\nLongitude=0\n"
    };
    for (size_t i = 0; i < G_N_ELEMENTS(malformed); ++i) {
        CHECK(g_file_set_contents(expected_file, malformed[i], -1, NULL));
        CHECK(!ss_location_metadata_load(&loaded));
        CHECK(loaded.display_name[0] == '\0');
        CHECK(loaded.latitude == 0.0 && loaded.longitude == 0.0);
    }
    CHECK(g_remove(expected_file) == 0);
    {
        g_autofree gchar *settings_dir = g_path_get_dirname(expected_file);
        g_autofree gchar *infiltrator_dir = g_path_get_dirname(settings_dir);
        CHECK(g_rmdir(settings_dir) == 0);
        CHECK(g_rmdir(infiltrator_dir) == 0);
    }
    CHECK(g_rmdir(temporary_root) == 0);
    return 0;
}
