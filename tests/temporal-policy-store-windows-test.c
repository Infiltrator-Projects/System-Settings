// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "temporal-policy-store-private.h"

#include <stdbool.h>
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

#define WRITER_COUNT 8U
#define WRITER_ITERATIONS 64U

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

static int writer_process(const char *mode)
{
    wchar_t policy_path[MAX_PATH];
    InfiltratrTemporalPolicyV3 policy;
    DWORD length = GetEnvironmentVariableW(
        L"SS_TEST_POLICY_PATH", policy_path, MAX_PATH);

    if (length == 0U || length >= MAX_PATH ||
        !infiltratr_temporal_policy_v3_default(&policy)) {
        return EXIT_FAILURE;
    }

    if (strcmp(mode, "--writer12") == 0) {
        memcpy(policy.clock_mode, "standard-12", sizeof("standard-12"));
        policy.show_seconds = false;
    } else if (strcmp(mode, "--writer24") == 0) {
        memcpy(policy.clock_mode, "standard-24", sizeof("standard-24"));
        policy.show_seconds = true;
    } else {
        return EXIT_FAILURE;
    }

    for (unsigned int i = 0U; i < WRITER_ITERATIONS; ++i) {
        if (!ss_windows_temporal_policy_save_file(policy_path, &policy)) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

static void run_concurrent_writers(const wchar_t *policy_path)
{
    wchar_t executable[MAX_PATH];
    HANDLE processes[WRITER_COUNT] = {0};
    DWORD executable_length = GetModuleFileNameW(NULL, executable, MAX_PATH);

    CHECK(executable_length > 0U && executable_length < MAX_PATH);
    CHECK(SetEnvironmentVariableW(L"SS_TEST_POLICY_PATH", policy_path));

    for (DWORD i = 0U; i < WRITER_COUNT; ++i) {
        STARTUPINFOW startup = {0};
        PROCESS_INFORMATION process = {0};
        wchar_t command[MAX_PATH + 64U];
        const wchar_t *argument = (i % 2U) == 0U
            ? L"--writer12" : L"--writer24";
        int length;

        startup.cb = sizeof(startup);
        length = swprintf(
            command, sizeof(command) / sizeof(command[0]),
            L"\"%ls\" %ls", executable, argument);
        CHECK(length > 0 &&
              (size_t)length < sizeof(command) / sizeof(command[0]));
        CHECK(CreateProcessW(
            NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
            NULL, NULL, &startup, &process));
        CHECK(CloseHandle(process.hThread));
        processes[i] = process.hProcess;
    }

    CHECK(WaitForMultipleObjects(
              WRITER_COUNT, processes, TRUE, 30000U) == WAIT_OBJECT_0);

    for (DWORD i = 0U; i < WRITER_COUNT; ++i) {
        DWORD exit_code = STILL_ACTIVE;
        CHECK(GetExitCodeProcess(processes[i], &exit_code));
        CHECK(exit_code == EXIT_SUCCESS);
        CHECK(CloseHandle(processes[i]));
    }
    CHECK(SetEnvironmentVariableW(L"SS_TEST_POLICY_PATH", NULL));
}

static void check_no_stale_temporaries(const wchar_t *policy_path)
{
    wchar_t pattern[MAX_PATH + 16U];
    WIN32_FIND_DATAW data;
    HANDLE search;
    int length = swprintf(
        pattern, sizeof(pattern) / sizeof(pattern[0]),
        L"%ls.*.tmp", policy_path);

    CHECK(length > 0 &&
          (size_t)length < sizeof(pattern) / sizeof(pattern[0]));
    search = FindFirstFileW(pattern, &data);
    CHECK(search == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
}

int main(int argc, char **argv)
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

    if (argc == 2 &&
        (strcmp(argv[1], "--writer12") == 0 ||
         strcmp(argv[1], "--writer24") == 0)) {
        return writer_process(argv[1]);
    }

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
    memcpy(policy.clock_mode, "standard-12", sizeof("standard-12"));
    CHECK(ss_windows_temporal_policy_save_file(policy_path, &policy));
    found = false;
    CHECK(ss_windows_temporal_policy_load_file(
        policy_path, &policy, &found));
    CHECK(found);
    CHECK(strcmp(policy.clock_mode, "standard-12") == 0);

    /* Cross-process writers exercise the exact race that a shared .tmp name
     * previously allowed. The final document may be either complete writer,
     * but it must parse as one whole policy and leave no sibling temporary. */
    run_concurrent_writers(policy_path);
    found = false;
    CHECK(ss_windows_temporal_policy_load_file(policy_path, &policy, &found));
    CHECK(found);
    if (strcmp(policy.clock_mode, "standard-12") == 0) {
        CHECK(!policy.show_seconds);
    } else {
        CHECK(strcmp(policy.clock_mode, "standard-24") == 0);
        CHECK(policy.show_seconds);
    }
    check_no_stale_temporaries(policy_path);

    CHECK(DeleteFileW(policy_path));
    CHECK(RemoveDirectoryW(unique_path));
    return EXIT_SUCCESS;
}
