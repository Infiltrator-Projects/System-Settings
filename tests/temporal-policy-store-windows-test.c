// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "temporal-policy-store-private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "Windows temporal-policy test failed: %s (%s:%d)\\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static void write_policy_file(const wchar_t *path,
                              const char *text,
                              size_t length)
{
    HANDLE file;
    DWORD written = 0U;

    file = CreateFileW(path, GENERIC_WRITE, 0U, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(file != INVALID_HANDLE_VALUE);
    if (length > 0U) {
        CHECK(WriteFile(file, text, (DWORD)length, &written, NULL));
        CHECK(written == (DWORD)length);
    }
    CHECK(CloseHandle(file));
}

int main(void)
{
    static const char valid[] =
        "version=3\n"
        "clock-mode=standard-24\n"
        "calendar=gregorian\n"
        "show-seconds=true\n"
        "location-configured=false\n"
        "latitude=0.000000\n"
        "longitude=0.000000\n";
    wchar_t temp_root[MAX_PATH];
    wchar_t unique_path[MAX_PATH];
    wchar_t policy_path[MAX_PATH];
    InfiltratrTemporalPolicyV3 policy;
    bool found = true;
    DWORD temp_length;

    temp_length = GetTempPathW(MAX_PATH, temp_root);
    CHECK(temp_length > 0U && temp_length < MAX_PATH);
    CHECK(GetTempFileNameW(temp_root, L"sst", 0U, unique_path) != 0U);
    CHECK(DeleteFileW(unique_path));
    CHECK(CreateDirectoryW(unique_path, NULL));
    CHECK(swprintf(policy_path, MAX_PATH, L"%ls\\presentation.conf",
                   unique_path) > 0);

    CHECK(ss_windows_temporal_policy_load_file(
        policy_path, &policy, &found));
    CHECK(!found);
    CHECK(strcmp(policy.clock_mode, "standard") == 0);

    write_policy_file(policy_path, "", 0U);
    found = true;
    CHECK(ss_windows_temporal_policy_load_file(
        policy_path, &policy, &found));
    CHECK(!found);
    CHECK(strcmp(policy.calendar, "gregorian") == 0);

    write_policy_file(
        policy_path,
        "version=3\nclock-mode=broken\n",
        strlen("version=3\nclock-mode=broken\n"));
    found = true;
    CHECK(ss_windows_temporal_policy_load_file(
        policy_path, &policy, &found));
    CHECK(!found);
    CHECK(strcmp(policy.clock_mode, "standard") == 0);

    write_policy_file(policy_path, valid, sizeof(valid) - 1U);
    found = false;
    CHECK(ss_windows_temporal_policy_load_file(
        policy_path, &policy, &found));
    CHECK(found);
    CHECK(strcmp(policy.clock_mode, "standard-24") == 0);
    CHECK(policy.show_seconds);

    /* A valid prefix followed by an embedded NUL and garbage is malformed. */
    char nul_document[sizeof(valid) + 4U];
    memcpy(nul_document, valid, sizeof(valid));
    memcpy(nul_document + sizeof(valid), "junk", 4U);
    write_policy_file(policy_path, nul_document, sizeof(nul_document));
    CHECK(ss_windows_temporal_policy_load_file(policy_path, &policy, &found));
    CHECK(!found);

    CHECK(infiltratr_temporal_policy_v3_default(&policy));
    strcpy(policy.clock_mode, "standard-12");
    CHECK(ss_windows_temporal_policy_save_file(policy_path, &policy));
    found = false;
    CHECK(ss_windows_temporal_policy_load_file(
        policy_path, &policy, &found));
    CHECK(found);
    CHECK(strcmp(policy.clock_mode, "standard-12") == 0);

    CHECK(DeleteFileW(policy_path));
    CHECK(RemoveDirectoryW(unique_path));
    return EXIT_SUCCESS;
}
