// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "system-settings/temporal-policy-store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "Temporal policy store test failed: %s (%s:%d)\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static void write_text(const char *path, const char *text)
{
    FILE *stream = fopen(path, "wb");
    CHECK(stream != NULL);
    CHECK(fputs(text, stream) >= 0);
    CHECK(fclose(stream) == 0);
}

int main(void)
{
    char root[] = "/tmp/infiltrator-system-settings-policy-XXXXXX";
    char directory[512];
    char path[640];
    InfiltratrTemporalPolicyV3 policy;
    bool found = true;
    const SsTemporalPolicyStore *store = ss_platform_temporal_policy_store();

    CHECK(store != NULL && store->load != NULL && store->save != NULL);
    CHECK(mkdtemp(root) != NULL);
    CHECK(setenv("XDG_CONFIG_HOME", root, 1) == 0);
    CHECK(snprintf(directory, sizeof(directory), "%s/infiltrator", root) > 0);
    CHECK(snprintf(path, sizeof(path), "%s/presentation.conf", directory) > 0);
    CHECK(mkdir(directory, 0700) == 0);

    write_text(path, "version=3\nclock-mode=not-a-clock\n");
    CHECK(store->load(&policy, &found));
    CHECK(!found);
    CHECK(strcmp(policy.clock_mode, "standard") == 0);
    CHECK(strcmp(policy.calendar, "gregorian") == 0);

    write_text(path,
               "version=3\n"
               "clock-mode=standard-24\n"
               "calendar=gregorian\n"
               "show-seconds=true\n"
               "location-configured=false\n"
               "latitude=0.000000\n"
               "longitude=0.000000\n");
    found = false;
    CHECK(store->load(&policy, &found));
    CHECK(found);
    CHECK(strcmp(policy.clock_mode, "standard-24") == 0);
    CHECK(policy.show_seconds);

    CHECK(unlink(path) == 0);
    CHECK(rmdir(directory) == 0);
    CHECK(rmdir(root) == 0);

    puts("System Settings temporal policy store test passed.");
    return 0;
}
