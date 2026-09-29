// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/location-metadata.h"
#include "system-settings/temporal-policy-store.h"

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
    g_autofree gchar *settings_dir = NULL;
    g_autofree gchar *lock_file = NULL;
    SsLocationMetadata saved = {0};
    GStatBuf settings_stat;
    SsLocationMetadata loaded = {0};
    g_autoptr(GError) error = NULL;

    temporary_root = g_dir_make_tmp(
        "system-settings-location-test-XXXXXX", &error);
    CHECK(temporary_root != NULL);
    CHECK(g_setenv("XDG_CONFIG_HOME", temporary_root, TRUE));
    CHECK(g_setenv("GSETTINGS_BACKEND", "memory", TRUE));

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

    settings_dir = g_path_get_dirname(expected_file);
    CHECK(settings_dir != NULL);
    CHECK(g_chmod(settings_dir, 0755) == 0);
    CHECK(ss_location_metadata_save(&saved));
    CHECK(g_stat(settings_dir, &settings_stat) == 0);
    CHECK((settings_stat.st_mode & 0777) == 0700);

    /*
     * The transaction lock is process-owned and must be explicitly released;
     * a second begin in the same process is rejected rather than silently
     * weakening serialization.
     */
    CHECK(ss_location_metadata_transaction_begin());
    CHECK(!ss_location_metadata_transaction_begin());
    ss_location_metadata_transaction_end();
    CHECK(ss_location_metadata_transaction_begin());
    ss_location_metadata_transaction_end();

    /*
     * Interrupted paired locality writes leave a private journal. A matching
     * authoritative policy finalizes it; a non-matching policy discards it.
     */
    {
        SsLocationMetadata staged = saved;
        g_strlcpy(
            staged.display_name,
            "Staged locality",
            sizeof(staged.display_name));
        staged.latitude = -37.1000;
        staged.longitude = 146.2000;

        CHECK(ss_location_metadata_stage(&staged));
        CHECK(ss_location_metadata_recover(
            false, staged.latitude, staged.longitude));
        CHECK(!ss_location_metadata_load(&loaded));
    CHECK(ss_location_metadata_load_status(&loaded) ==
          SS_LOCATION_METADATA_LOAD_MISSING);

        {
            const SsTemporalPolicyStore *store =
                ss_platform_temporal_policy_store();
            InfiltratrTemporalPolicyV3 authoritative;
            CHECK(store != NULL && store->save != NULL);
            CHECK(infiltratr_temporal_policy_v3_default(&authoritative));
            authoritative.location_configured = true;
            authoritative.latitude = staged.latitude;
            authoritative.longitude = staged.longitude;
            CHECK(store->save(&authoritative));
        }

        CHECK(ss_location_metadata_stage(&staged));
        CHECK(ss_location_metadata_recover(
            true, staged.latitude, staged.longitude));
        CHECK(ss_location_metadata_load(&loaded));
        CHECK(strcmp(loaded.display_name, "Staged locality") == 0);
        CHECK(absolute_difference(
                  loaded.latitude, staged.latitude) < 0.000001);
        CHECK(absolute_difference(
                  loaded.longitude, staged.longitude) < 0.000001);

        saved = staged;
    }

    CHECK(ss_location_metadata_clear());
    CHECK(!g_file_test(expected_file, G_FILE_TEST_EXISTS));
    CHECK(ss_location_metadata_clear());
    CHECK(ss_location_metadata_save(&saved));
    CHECK(g_file_test(expected_file, G_FILE_TEST_IS_REGULAR));

    /*
     * Recovery of a policy with no configured location removes metadata left
     * by a crash after the policy commit but before metadata cleanup.
     */
    {
        const SsTemporalPolicyStore *store =
            ss_platform_temporal_policy_store();
        InfiltratrTemporalPolicyV3 authoritative;
        CHECK(store != NULL && store->save != NULL);
        CHECK(infiltratr_temporal_policy_v3_default(&authoritative));
        CHECK(store->save(&authoritative));
    }
    CHECK(ss_location_metadata_recover(false, 0.0, 0.0));
    CHECK(!g_file_test(expected_file, G_FILE_TEST_EXISTS));
    CHECK(ss_location_metadata_save(&saved));

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
        CHECK(ss_location_metadata_load_status(&loaded) ==
              SS_LOCATION_METADATA_LOAD_INVALID);
        CHECK(loaded.display_name[0] == '\0');
        CHECK(loaded.latitude == 0.0 && loaded.longitude == 0.0);
    }
    CHECK(g_remove(expected_file) == 0);

    /*
     * A path that exists but cannot be read as a regular key file is an I/O
     * failure, never "missing". Recovery must preserve an unreadable pending
     * journal rather than deleting evidence it cannot classify.
     */
    CHECK(g_file_set_contents(
        expected_file,
        "[Location]\nName=Unreadable\nLatitude=0\nLongitude=0\n",
        -1,
        NULL));
    CHECK(g_chmod(expected_file, 0000) == 0);
    CHECK(ss_location_metadata_load_status(&loaded) ==
          SS_LOCATION_METADATA_LOAD_IO_ERROR);
    CHECK(g_chmod(expected_file, 0600) == 0);
    CHECK(g_remove(expected_file) == 0);
    {
        SsLocationMetadata staged = {0};
        g_autofree gchar *pending_file = g_build_filename(
            settings_dir, "location.pending", NULL);
        g_strlcpy(staged.display_name, "Unreadable journal",
                  sizeof(staged.display_name));
        staged.latitude = -36.0;
        staged.longitude = 145.0;
        CHECK(ss_location_metadata_stage(&staged));
        CHECK(g_chmod(pending_file, 0000) == 0);
        CHECK(!ss_location_metadata_recover(false, 0.0, 0.0));
        CHECK(g_file_test(pending_file, G_FILE_TEST_IS_REGULAR));
        CHECK(g_chmod(pending_file, 0600) == 0);
        CHECK(ss_location_metadata_discard_staged());
    }
    {
        g_autofree gchar *policy_file = g_build_filename(
            temporary_root, "infiltrator", "presentation.conf", NULL);
        CHECK(g_remove(policy_file) == 0);
    }
    lock_file = g_build_filename(settings_dir, "location.lock", NULL);
    CHECK(lock_file != NULL);
    CHECK(g_remove(lock_file) == 0);
    {
        g_autofree gchar *infiltrator_dir = g_path_get_dirname(settings_dir);
        CHECK(g_rmdir(settings_dir) == 0);
        CHECK(g_rmdir(infiltrator_dir) == 0);
    }
    CHECK(g_rmdir(temporary_root) == 0);
    return 0;
}
