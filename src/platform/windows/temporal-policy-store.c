// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal-policy-store.c
 * @brief Windows LocalAppData persistence for temporal presentation policy.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include "system-settings/temporal-policy-store.h"
#include "temporal-policy-store-private.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define SS_POLICY_CAPACITY 1024U

/*
 * Resolve the supported per-user storage root through Known Folders rather
 * than assuming an environment variable or a fixed profile layout.
 */
static bool build_paths(wchar_t **directory_out, wchar_t **path_out)
{
    static const wchar_t directory_name[] = L"Infiltrator";
    static const wchar_t file_name[] = L"presentation.conf";
    PWSTR base = NULL;
    wchar_t *directory = NULL;
    wchar_t *path = NULL;
    size_t base_length;
    size_t directory_length;
    bool needs_separator;

    if (directory_out == NULL || path_out == NULL ||
        FAILED(SHGetKnownFolderPath(&FOLDERID_LocalAppData,
                                    KF_FLAG_CREATE, NULL, &base)) ||
        base == NULL) {
        return false;
    }

    base_length = wcslen(base);
    needs_separator = base_length > 0U &&
        base[base_length - 1U] != L'\\' &&
        base[base_length - 1U] != L'/';
    directory_length = base_length + (needs_separator ? 1U : 0U) +
        (sizeof(directory_name) / sizeof(directory_name[0]) - 1U);

    directory = (wchar_t *)calloc(directory_length + 1U, sizeof(wchar_t));
    path = (wchar_t *)calloc(directory_length + 1U +
        (sizeof(file_name) / sizeof(file_name[0])), sizeof(wchar_t));
    if (directory == NULL || path == NULL) {
        free(directory);
        free(path);
        CoTaskMemFree(base);
        return false;
    }

    memcpy(directory, base, base_length * sizeof(wchar_t));
    if (needs_separator) {
        directory[base_length++] = L'\\';
    }
    memcpy(directory + base_length, directory_name,
           sizeof(directory_name) - sizeof(wchar_t));

    memcpy(path, directory, directory_length * sizeof(wchar_t));
    path[directory_length] = L'\\';
    memcpy(path + directory_length + 1U, file_name, sizeof(file_name));

    CoTaskMemFree(base);
    *directory_out = directory;
    *path_out = path;
    return true;
}

bool ss_windows_temporal_policy_load_file(
    const wchar_t *path,
    InfiltratrTemporalPolicyV3 *policy,
    bool *found)
{
    HANDLE file;
    LARGE_INTEGER size;
    char text[SS_POLICY_CAPACITY];
    DWORD read_count = 0U;
    bool ok = false;

    if (path == NULL || policy == NULL || found == NULL ||
        !infiltratr_temporal_policy_v3_default(policy)) {
        return false;
    }
    *found = false;

    file = CreateFileW(path, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND ||
               error == ERROR_PATH_NOT_FOUND;
    }

    if (!GetFileSizeEx(file, &size)) {
        goto done;
    }

    /*
     * Policy is optional enrichment. Empty/oversized content is treated like
     * an absent valid policy, matching the Linux recovery contract rather than
     * preventing Date & Time from opening.
     */
    if (size.QuadPart <= 0 ||
        size.QuadPart >= (LONGLONG)sizeof(text)) {
        ok = true;
        goto done;
    }

    if (!ReadFile(file, text, (DWORD)size.QuadPart, &read_count, NULL) ||
        read_count != (DWORD)size.QuadPart) {
        goto done;
    }

    text[read_count] = '\0';
    if (memchr(text, '\0', read_count) != NULL ||
        !infiltratr_temporal_policy_v3_parse(text, policy)) {
        if (!infiltratr_temporal_policy_v3_default(policy)) {
            goto done;
        }
        ok = true;
        goto done;
    }

    *found = true;
    ok = true;

done:
    CloseHandle(file);
    return ok;
}

bool ss_windows_temporal_policy_save_file(
    const wchar_t *path,
    const InfiltratrTemporalPolicyV3 *policy)
{
    static volatile LONG sequence;
    wchar_t *temporary = NULL;
    bool temporary_owned = false;
    char text[SS_POLICY_CAPACITY];
    size_t length = 0U;
    size_t path_length;
    HANDLE file = INVALID_HANDLE_VALUE;
    DWORD written = 0U;
    bool ok = false;

    if (path == NULL || policy == NULL ||
        !infiltratr_temporal_policy_v3_serialize(
            policy, text, sizeof(text), &length)) {
        return false;
    }

    path_length = wcslen(path);
    temporary = (wchar_t *)calloc(path_length + 40U, sizeof(wchar_t));
    if (temporary == NULL) {
        return false;
    }
    memcpy(temporary, path, path_length * sizeof(wchar_t));

    /* Each writer exclusively creates its own sibling. CREATE_ALWAYS on a
     * shared .tmp name allowed another process to replace our staged bytes
     * between close and rename. Last completed publication wins. */
    for (unsigned int attempt = 0U; attempt < 32U; ++attempt) {
        if (swprintf(temporary + path_length, 40U, L".%lu.%lu.tmp",
                     (unsigned long)GetCurrentProcessId(),
                     (unsigned long)(DWORD)InterlockedIncrement(&sequence)) < 0) {
            goto done;
        }
        file = CreateFileW(temporary, GENERIC_WRITE, 0U, NULL, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            temporary_owned = true;
            break;
        }
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) {
            goto done;
        }
    }
    if (file == INVALID_HANDLE_VALUE) {
        goto done;
    }
    if (!WriteFile(file, text, (DWORD)length, &written, NULL) ||
        written != (DWORD)length || !FlushFileBuffers(file)) {
        goto done;
    }
    CloseHandle(file);
    file = INVALID_HANDLE_VALUE;

    if (!MoveFileExW(temporary, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        goto done;
    }
    ok = true;

done:
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    if (!ok && temporary_owned) {
        DeleteFileW(temporary);
    }
    free(temporary);
    return ok;
}

static bool platform_load(InfiltratrTemporalPolicyV3 *policy,
                          bool *found)
{
    wchar_t *directory = NULL;
    wchar_t *path = NULL;
    bool ok;

    if (!build_paths(&directory, &path)) {
        return false;
    }
    ok = ss_windows_temporal_policy_load_file(path, policy, found);
    free(directory);
    free(path);
    return ok;
}

static bool platform_save(const InfiltratrTemporalPolicyV3 *policy)
{
    wchar_t *directory = NULL;
    wchar_t *path = NULL;
    bool ok = false;

    if (policy == NULL || !build_paths(&directory, &path)) {
        return false;
    }

    if (!CreateDirectoryW(directory, NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        goto done;
    }

    ok = ss_windows_temporal_policy_save_file(path, policy);

done:
    free(directory);
    free(path);
    return ok;
}


const SsTemporalPolicyStore *ss_platform_temporal_policy_store(void)
{
    static const SsTemporalPolicyStore store = {
        .load = platform_load,
        .save = platform_save
    };
    return &store;
}
