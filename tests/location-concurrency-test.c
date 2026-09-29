// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L

#include "system-settings/date-time-model.h"
#include "system-settings/location-metadata.h"
#include "system-settings/temporal-policy-store.h"

#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int publish_locality(
    const char *name,
    double latitude,
    double longitude,
    guint hold_after_policy_us)
{
    SsDateTimeModel model;
    SsLocationMetadata metadata = {0};
    int result = 1;

    if (!ss_location_metadata_transaction_begin()) {
        return 10;
    }
    if (!ss_date_time_model_init(
            &model, ss_platform_temporal_policy_store())) {
        goto done;
    }

    g_strlcpy(metadata.display_name, name, sizeof(metadata.display_name));
    g_strlcpy(metadata.country_code, "AU", sizeof(metadata.country_code));
    metadata.latitude = latitude;
    metadata.longitude = longitude;

    if (!ss_location_metadata_stage(&metadata)) {
        goto done;
    }
    if (!ss_date_time_model_set_location(
            &model, true, latitude, longitude)) {
        (void)ss_location_metadata_discard_staged();
        goto done;
    }

    if (hold_after_policy_us != 0U) {
        g_usleep(hold_after_policy_us);
    }
    if (!ss_location_metadata_finish_staged()) {
        goto done;
    }
    result = 0;

done:
    ss_location_metadata_transaction_end();
    return result;
}

static int wait_ok(pid_t child)
{
    int status = 0;
    if (waitpid(child, &status, 0) != child ||
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        return 1;
    }
    return 0;
}

int main(void)
{
    g_autofree gchar *root =
        g_dir_make_tmp("ss-location-concurrency-XXXXXX", NULL);
    CHECK(root != NULL);
    CHECK(g_setenv("XDG_CONFIG_HOME", root, TRUE));
    CHECK(g_setenv("GSETTINGS_BACKEND", "memory", TRUE));

    pid_t first = fork();
    CHECK(first >= 0);
    if (first == 0) {
        _exit(publish_locality(
            "Writer A", -36.3949, 145.3610, 150000U));
    }

    g_usleep(20000U);
    pid_t second = fork();
    CHECK(second >= 0);
    if (second == 0) {
        _exit(publish_locality(
            "Writer B", -37.8136, 144.9631, 0U));
    }

    CHECK(wait_ok(first) == 0);
    CHECK(wait_ok(second) == 0);

    SsDateTimeModel final_model;
    SsLocationMetadata final_metadata = {0};
    CHECK(ss_date_time_model_init(
        &final_model, ss_platform_temporal_policy_store()));
    CHECK(ss_location_metadata_load(&final_metadata));
    CHECK(final_model.policy.location_configured);
    CHECK(fabs(final_model.policy.latitude -
               final_metadata.latitude) < 0.000001);
    CHECK(fabs(final_model.policy.longitude -
               final_metadata.longitude) < 0.000001);

    g_autofree gchar *pending = g_build_filename(
        root, "infiltrator", "system-settings", "location.pending", NULL);
    CHECK(!g_file_test(pending, G_FILE_TEST_EXISTS));

    /*
     * A stalled peer must not freeze the caller for seconds. The production
     * lock gives ordinary short transactions time to finish, then fails fast.
     */
    {
        int ready_pipe[2];
        CHECK(pipe(ready_pipe) == 0);
        pid_t holder = fork();
        CHECK(holder >= 0);
        if (holder == 0) {
            char ready = '1';
            close(ready_pipe[0]);
            if (!ss_location_metadata_transaction_begin()) _exit(20);
            if (write(ready_pipe[1], &ready, 1U) != 1) _exit(21);
            close(ready_pipe[1]);
            g_usleep(600000U);
            ss_location_metadata_transaction_end();
            _exit(0);
        }
        close(ready_pipe[1]);
        char ready = 0;
        CHECK(read(ready_pipe[0], &ready, 1U) == 1);
        close(ready_pipe[0]);
        const gint64 start = g_get_monotonic_time();
        CHECK(!ss_location_metadata_transaction_begin());
        const gint64 elapsed = g_get_monotonic_time() - start;
        CHECK(elapsed < 500000);
        CHECK(wait_ok(holder) == 0);
        CHECK(ss_location_metadata_transaction_begin());
        ss_location_metadata_transaction_end();
    }

    return 0;
}
