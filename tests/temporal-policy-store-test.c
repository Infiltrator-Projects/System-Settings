// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "system-settings/temporal-policy-store.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_text(const char *path, const char *text)
{
    FILE *stream = fopen(path, "wb");
    assert(stream != NULL);
    assert(fputs(text, stream) >= 0);
    assert(fclose(stream) == 0);
}

int main(void)
{
    char root[] = "/tmp/infiltrator-system-settings-policy-XXXXXX";
    char directory[512];
    char path[640];
    InfiltratrTemporalPolicyV3 policy;
    bool found = true;
    const SsTemporalPolicyStore *store = ss_platform_temporal_policy_store();

    assert(store != NULL && store->load != NULL && store->save != NULL);
    assert(mkdtemp(root) != NULL);
    assert(setenv("XDG_CONFIG_HOME", root, 1) == 0);
    assert(snprintf(directory, sizeof(directory), "%s/infiltrator", root) > 0);
    assert(snprintf(path, sizeof(path), "%s/presentation.conf", directory) > 0);
    assert(mkdir(directory, 0700) == 0);

    write_text(path, "version=3\nclock-mode=not-a-clock\n");
    assert(store->load(&policy, &found));
    assert(!found);
    assert(strcmp(policy.clock_mode, "standard") == 0);
    assert(strcmp(policy.calendar, "gregorian") == 0);

    write_text(path,
               "version=3\n"
               "clock-mode=standard-24\n"
               "calendar=gregorian\n"
               "show-seconds=true\n"
               "location-configured=false\n"
               "latitude=0.000000\n"
               "longitude=0.000000\n");
    found = false;
    assert(store->load(&policy, &found));
    assert(found);
    assert(strcmp(policy.clock_mode, "standard-24") == 0);
    assert(policy.show_seconds);

    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
    assert(rmdir(root) == 0);

    puts("System Settings temporal policy store test passed.");
    return 0;
}
