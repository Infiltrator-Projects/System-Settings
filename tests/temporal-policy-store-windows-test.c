// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include "system-settings/temporal-policy-store.h"

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

static bool test_paths(wchar_t **directory_out, wchar_t **path_out)
{
    PWSTR base = NULL;
    wchar_t *directory;
    wchar_t *path;
    size_t base_length;
    size_t directory_length;

    if (directory_out == NULL || path_out == NULL ||
        FAILED(SHGetKnownFolderPath(&FOLDERID_LocalAppData,
                                    KF_FLAG_CREATE, NULL, &base)) ||
        base == NULL) {
        return false;
    }

    base_length = wcslen(base);
    directory_length = base_length + wcslen(L"\\Infiltrator");
    directory = calloc(directory_length + 1U, sizeof(*directory));
    path = calloc(directory_length + wcslen(L"\\presentation.conf") + 1U,
                  sizeof(*path));
    if (directory == NULL || path == NULL) {
        free(directory);
        free(path);
        CoTaskMemFree(base);
        return false;
    }

    memcpy(directory, base, base_length * sizeof(*directory));
    memcpy(directory + base_length, L"\\Infiltrator",
           (wcslen(L"\\Infiltrator") + 1U) * sizeof(*directory));
    memcpy(path, directory, directory_length * sizeof(*path));
    memcpy(path + directory_length, L"\\presentation.conf",
           (wcslen(L"\\presentation.conf") + 1U) * sizeof(*path));

    CoTaskMemFree(base);
    *directory_out = directory;
    *path_out = path;
    return true;
}

static void write_policy_file(const wchar_t *directory,
                              const wchar_t *path,
                              const char *text,
                              size_t length)
{
    HANDLE file;
    DWORD written = 0U;

    CHECK(CreateDirectoryW(directory, NULL) ||
          GetLastError() == ERROR_ALREADY_EXISTS);
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
    const SsTemporalPolicyStore *store = ss_platform_temporal_policy_store();
    InfiltratrTemporalPolicyV3 policy;
    wchar_t *directory = NULL;
    wchar_t *path = NULL;
    bool found = true;

    CHECK(store != NULL && store->load != NULL && store->save != NULL);
    CHECK(test_paths(&directory, &path));
    (void)DeleteFileW(path);

    CHECK(store->load(&policy, &found));
    CHECK(!found);
    CHECK(strcmp(policy.clock_mode, "standard") == 0);

    write_policy_file(directory, path, "", 0U);
    found = true;
    CHECK(store->load(&policy, &found));
    CHECK(!found);
    CHECK(strcmp(policy.calendar, "gregorian") == 0);

    write_policy_file(directory, path,
                      "version=3\nclock-mode=broken\n",
                      strlen("version=3\nclock-mode=broken\n"));
    found = true;
    CHECK(store->load(&policy, &found));
    CHECK(!found);
    CHECK(strcmp(policy.clock_mode, "standard") == 0);

    write_policy_file(directory, path, valid, sizeof(valid) - 1U);
    found = false;
    CHECK(store->load(&policy, &found));
    CHECK(found);
    CHECK(strcmp(policy.clock_mode, "standard-24") == 0);
    CHECK(policy.show_seconds);

    CHECK(infiltratr_temporal_policy_v3_default(&policy));
    strcpy(policy.clock_mode, "standard-12");
    CHECK(store->save(&policy));
    found = false;
    CHECK(store->load(&policy, &found));
    CHECK(found);
    CHECK(strcmp(policy.clock_mode, "standard-12") == 0);

    CHECK(DeleteFileW(path));
    free(directory);
    free(path);
    return EXIT_SUCCESS;
}
