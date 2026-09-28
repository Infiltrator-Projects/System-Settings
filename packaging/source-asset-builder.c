// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L

/**
 * @file source-asset-builder.c
 * @brief Native release builder for source and CPU-native installer assets.
 *
 * This replaces the former release-only Bash implementation. It never mutates
 * the repository, exports exact Git objects, embeds a byte manifest for Common,
 * builds the self-extracting native installer, and verifies extraction plus
 * CMake provenance before returning success.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct PathList {
    char **items;
    size_t length;
    size_t capacity;
} PathList;

static void fail(const char *message)
{
    fprintf(stderr, "System Settings source asset builder: %s\n", message);
    exit(EXIT_FAILURE);
}

static bool path_join(
    char *out,
    size_t capacity,
    const char *left,
    const char *right)
{
    const int count = snprintf(out, capacity, "%s/%s", left, right);
    return count > 0 && (size_t)count < capacity;
}

static int run_command(const char *cwd, char *const argv[])
{
    pid_t child = fork();
    int status = 0;

    if (child < 0) {
        return -1;
    }
    if (child == 0) {
        if (cwd != NULL && chdir(cwd) != 0) {
            fprintf(stderr, "chdir(%s): %s\n", cwd, strerror(errno));
            _exit(126);
        }
        execvp(argv[0], argv);
        fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static bool capture_command(
    const char *cwd,
    char *const argv[],
    char *out,
    size_t capacity)
{
    int pipefd[2];
    pid_t child;
    size_t used = 0U;
    int status = 0;

    if (out == NULL || capacity < 2U || pipe(pipefd) != 0) {
        return false;
    }

    child = fork();
    if (child < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }
    if (child == 0) {
        if (cwd != NULL && chdir(cwd) != 0) {
            _exit(126);
        }
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(126);
        }
        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipefd[1]);
    while (used + 1U < capacity) {
        ssize_t count = read(
            pipefd[0], out + used, capacity - used - 1U);
        if (count > 0) {
            used += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
    close(pipefd[0]);
    out[used] = '\0';

    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            return false;
        }
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return false;
    }

    while (used > 0U &&
           (out[used - 1U] == '\n' ||
            out[used - 1U] == '\r' ||
            out[used - 1U] == ' ' ||
            out[used - 1U] == '\t')) {
        out[--used] = '\0';
    }
    return used > 0U;
}

static bool mkdir_one(const char *path)
{
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

static int remove_tree(const char *path)
{
    struct stat st;

    if (lstat(path, &st) != 0) {
        return errno == ENOENT ? 0 : -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        return unlink(path);
    }

    DIR *directory = opendir(path);
    if (directory == NULL) {
        return -1;
    }
    for (;;) {
        struct dirent *entry = readdir(directory);
        char child[PATH_MAX];

        if (entry == NULL) {
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!path_join(child, sizeof(child), path, entry->d_name) ||
            remove_tree(child) != 0) {
            closedir(directory);
            return -1;
        }
    }
    if (closedir(directory) != 0) {
        return -1;
    }
    return rmdir(path);
}

static bool write_text(const char *path, const char *text)
{
    FILE *stream = fopen(path, "wb");
    const size_t length = text != NULL ? strlen(text) : 0U;
    bool ok;

    if (stream == NULL) {
        return false;
    }
    ok = fwrite(text, 1U, length, stream) == length &&
         fflush(stream) == 0 &&
         fsync(fileno(stream)) == 0;
    if (fclose(stream) != 0) {
        ok = false;
    }
    return ok;
}

static bool copy_stream(FILE *destination, FILE *source)
{
    unsigned char buffer[64U * 1024U];

    for (;;) {
        const size_t count = fread(buffer, 1U, sizeof(buffer), source);
        if (count > 0U &&
            fwrite(buffer, 1U, count, destination) != count) {
            return false;
        }
        if (count < sizeof(buffer)) {
            return feof(source) != 0 && ferror(source) == 0;
        }
    }
}

static bool append_file(FILE *destination, const char *path)
{
    FILE *source = fopen(path, "rb");
    bool ok;

    if (source == NULL) {
        return false;
    }
    ok = copy_stream(destination, source);
    if (fclose(source) != 0) {
        ok = false;
    }
    return ok;
}

static bool files_equal(const char *left, const char *right)
{
    FILE *a = fopen(left, "rb");
    FILE *b = fopen(right, "rb");
    unsigned char abuf[8192];
    unsigned char bbuf[8192];
    bool equal = false;

    if (a == NULL || b == NULL) {
        goto done;
    }
    for (;;) {
        const size_t acount = fread(abuf, 1U, sizeof(abuf), a);
        const size_t bcount = fread(bbuf, 1U, sizeof(bbuf), b);
        if (acount != bcount ||
            (acount > 0U && memcmp(abuf, bbuf, acount) != 0)) {
            goto done;
        }
        if (acount < sizeof(abuf)) {
            equal = feof(a) != 0 && feof(b) != 0 &&
                    ferror(a) == 0 && ferror(b) == 0;
            break;
        }
    }

done:
    if (a != NULL) fclose(a);
    if (b != NULL) fclose(b);
    return equal;
}

static bool path_list_add(PathList *list, const char *path)
{
    if (list == NULL || path == NULL) {
        return false;
    }
    if (list->length == list->capacity) {
        const size_t next =
            list->capacity == 0U ? 32U : list->capacity * 2U;
        char **items = realloc(list->items, next * sizeof(*items));
        if (items == NULL) {
            return false;
        }
        list->items = items;
        list->capacity = next;
    }
    list->items[list->length] = strdup(path);
    if (list->items[list->length] == NULL) {
        return false;
    }
    ++list->length;
    return true;
}

static void path_list_free(PathList *list)
{
    if (list == NULL) {
        return;
    }
    for (size_t i = 0U; i < list->length; ++i) {
        free(list->items[i]);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static int compare_paths(const void *left, const void *right)
{
    const char *const *a = left;
    const char *const *b = right;
    return strcmp(*a, *b);
}

static bool collect_files(
    const char *root,
    const char *relative,
    PathList *list)
{
    char directory_path[PATH_MAX];
    DIR *directory;

    if (relative[0] == '\0') {
        if (strlen(root) >= sizeof(directory_path)) {
            return false;
        }
        strcpy(directory_path, root);
    } else if (!path_join(
                   directory_path,
                   sizeof(directory_path),
                   root,
                   relative)) {
        return false;
    }

    directory = opendir(directory_path);
    if (directory == NULL) {
        return false;
    }

    for (;;) {
        struct dirent *entry = readdir(directory);
        char child_relative[PATH_MAX];
        char child_path[PATH_MAX];
        struct stat st;

        if (entry == NULL) {
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (relative[0] == '\0') {
            const int child_length = snprintf(
                child_relative,
                sizeof(child_relative),
                "%s",
                entry->d_name);
            if (child_length <= 0 ||
                (size_t)child_length >= sizeof(child_relative)) {
                closedir(directory);
                return false;
            }
        } else if (!path_join(
                       child_relative,
                       sizeof(child_relative),
                       relative,
                       entry->d_name)) {
            closedir(directory);
            return false;
        }

        if (!path_join(
                child_path,
                sizeof(child_path),
                root,
                child_relative) ||
            lstat(child_path, &st) != 0) {
            closedir(directory);
            return false;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!collect_files(root, child_relative, list)) {
                closedir(directory);
                return false;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (strcmp(
                    child_relative,
                    ".system-settings-common-commit") != 0 &&
                strcmp(
                    child_relative,
                    ".system-settings-common-sha256") != 0 &&
                !path_list_add(list, child_relative)) {
                closedir(directory);
                return false;
            }
        } else {
            fprintf(
                stderr,
                "Unsupported exported Common file type: %s\n",
                child_relative);
            closedir(directory);
            return false;
        }
    }

    return closedir(directory) == 0;
}

static bool trees_equal(
    const char *left_root,
    const char *right_root)
{
    PathList left = {0};
    PathList right = {0};
    bool equal = false;

    if (!collect_files(left_root, "", &left) ||
        !collect_files(right_root, "", &right)) {
        goto done;
    }
    qsort(left.items, left.length, sizeof(*left.items), compare_paths);
    qsort(right.items, right.length, sizeof(*right.items), compare_paths);
    if (left.length != right.length) {
        goto done;
    }

    for (size_t i = 0U; i < left.length; ++i) {
        char left_path[PATH_MAX];
        char right_path[PATH_MAX];

        if (strcmp(left.items[i], right.items[i]) != 0 ||
            !path_join(
                left_path, sizeof(left_path),
                left_root, left.items[i]) ||
            !path_join(
                right_path, sizeof(right_path),
                right_root, right.items[i]) ||
            !files_equal(left_path, right_path)) {
            goto done;
        }
    }
    equal = true;

done:
    path_list_free(&left);
    path_list_free(&right);
    return equal;
}

static bool normalize_tree_times(
    const char *root,
    const char *relative,
    time_t epoch)
{
    char directory_path[PATH_MAX];
    DIR *directory;
    struct timespec times[2] = {
        { .tv_sec = epoch, .tv_nsec = 0 },
        { .tv_sec = epoch, .tv_nsec = 0 }
    };

    if (relative[0] == '\0') {
        if (strlen(root) >= sizeof(directory_path)) {
            return false;
        }
        strcpy(directory_path, root);
    } else if (!path_join(
                   directory_path,
                   sizeof(directory_path),
                   root,
                   relative)) {
        return false;
    }

    directory = opendir(directory_path);
    if (directory == NULL) {
        return false;
    }

    for (;;) {
        struct dirent *entry = readdir(directory);
        char child_relative[PATH_MAX];
        char child_path[PATH_MAX];
        struct stat st;

        if (entry == NULL) {
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (relative[0] == '\0') {
            const int count = snprintf(
                child_relative,
                sizeof(child_relative),
                "%s",
                entry->d_name);
            if (count <= 0 ||
                (size_t)count >= sizeof(child_relative)) {
                closedir(directory);
                return false;
            }
        } else if (!path_join(
                       child_relative,
                       sizeof(child_relative),
                       relative,
                       entry->d_name)) {
            closedir(directory);
            return false;
        }

        if (!path_join(
                child_path,
                sizeof(child_path),
                root,
                child_relative) ||
            lstat(child_path, &st) != 0) {
            closedir(directory);
            return false;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!normalize_tree_times(
                    root, child_relative, epoch)) {
                closedir(directory);
                return false;
            }
        } else if (!S_ISREG(st.st_mode)) {
            closedir(directory);
            return false;
        }

        if (utimensat(
                AT_FDCWD,
                child_path,
                times,
                0) != 0) {
            closedir(directory);
            return false;
        }
    }

    if (closedir(directory) != 0) {
        return false;
    }
    return utimensat(
        AT_FDCWD,
        directory_path,
        times,
        0) == 0;
}

static bool sha256_file(
    const char *path,
    char hash[65])
{
    char output[256];
    char *argv[] = {"sha256sum", (char *)path, NULL};

    if (!capture_command(NULL, argv, output, sizeof(output)) ||
        strlen(output) < 64U) {
        return false;
    }
    for (size_t i = 0U; i < 64U; ++i) {
        if (!isxdigit((unsigned char)output[i])) {
            return false;
        }
        hash[i] = (char)tolower((unsigned char)output[i]);
    }
    hash[64] = '\0';
    return true;
}

static bool write_common_manifest(const char *common_root)
{
    PathList list = {0};
    char manifest_path[PATH_MAX];
    FILE *manifest = NULL;
    bool ok = false;

    if (!collect_files(common_root, "", &list) ||
        !path_join(
            manifest_path,
            sizeof(manifest_path),
            common_root,
            ".system-settings-common-sha256")) {
        goto done;
    }

    qsort(list.items, list.length, sizeof(*list.items), compare_paths);
    manifest = fopen(manifest_path, "wb");
    if (manifest == NULL) {
        goto done;
    }

    for (size_t i = 0U; i < list.length; ++i) {
        char file_path[PATH_MAX];
        char hash[65];
        if (!path_join(
                file_path,
                sizeof(file_path),
                common_root,
                list.items[i]) ||
            !sha256_file(file_path, hash) ||
            fprintf(
                manifest,
                "%s  %s\n",
                hash,
                list.items[i]) < 0) {
            goto done;
        }
    }

    ok = fflush(manifest) == 0 &&
         fsync(fileno(manifest)) == 0;

done:
    if (manifest != NULL && fclose(manifest) != 0) {
        ok = false;
    }
    path_list_free(&list);
    return ok;
}

static bool valid_version(const char *value)
{
    unsigned int dots = 0U;
    bool digit = false;

    if (value == NULL || value[0] == '\0') {
        return false;
    }
    for (const unsigned char *p =
             (const unsigned char *)value;
         *p != '\0';
         ++p) {
        if (*p == '.') {
            if (!digit || dots >= 2U) {
                return false;
            }
            ++dots;
            digit = false;
        } else if (isdigit(*p)) {
            digit = true;
        } else {
            return false;
        }
    }
    return dots == 2U && digit;
}

static bool read_trimmed(
    const char *path,
    char *out,
    size_t capacity)
{
    FILE *stream = fopen(path, "rb");
    size_t count;

    if (stream == NULL || out == NULL || capacity < 2U) {
        if (stream != NULL) fclose(stream);
        return false;
    }
    count = fread(out, 1U, capacity - 1U, stream);
    if (ferror(stream) != 0 || !feof(stream)) {
        fclose(stream);
        return false;
    }
    fclose(stream);
    out[count] = '\0';
    while (count > 0U &&
           isspace((unsigned char)out[count - 1U])) {
        out[--count] = '\0';
    }
    return count > 0U;
}

static void show_help(void)
{
    puts("System Settings source asset builder");
    puts("Usage: source-asset-builder OUTPUT_DIRECTORY");
    puts("Exports exact Git HEAD plus pinned Common, creates/verifies the native .run and source ZIP.");
}

int main(int argc, char **argv)
{
    static const char common_relative[] =
        "src/vendor/infiltratr-common";
    char root[PATH_MAX];
    char output[PATH_MAX];
    char version[64];
    char version_path[PATH_MAX];
    char common[PATH_MAX];
    char pin[80];
    char common_head[80];
    char epoch_text[32];
    time_t source_epoch = 0;
    char work_template[] =
        "/tmp/system-settings-source-assets-XXXXXX";
    char *work = NULL;
    char source_root[PATH_MAX];
    char common_export[PATH_MAX];
    char root_tar[PATH_MAX];
    char common_tar[PATH_MAX];
    char marker[PATH_MAX];
    char payload[PATH_MAX];
    char installer_source[PATH_MAX];
    char installer_binary[PATH_MAX];
    char run_path[PATH_MAX];
    char zip_path[PATH_MAX];
    char verified[PATH_MAX];
    char verify_build[PATH_MAX];
    char source_version[PATH_MAX];
    char verified_version[PATH_MAX];
    char source_common_version[PATH_MAX];
    char verified_common_version[PATH_MAX];
    char output_argument[PATH_MAX];
    char root_output_argument[PATH_MAX];
    char common_output_argument[PATH_MAX];
    FILE *run = NULL;
    struct stat payload_stat;
    bool ok = false;

    if (argc == 2 &&
        (strcmp(argv[1], "--help") == 0 ||
         strcmp(argv[1], "-h") == 0)) {
        show_help();
        return EXIT_SUCCESS;
    }
    if (argc != 2 || argv[1][0] == '\0') {
        show_help();
        return 2;
    }

    {
        char *root_argv[] = {
            "git", "rev-parse", "--show-toplevel", NULL
        };
        if (!capture_command(
                NULL, root_argv, root, sizeof(root))) {
            fail("not inside the System Settings Git repository.");
        }
    }
    if (chdir(root) != 0) {
        fail("cannot enter repository root.");
    }

    {
        char *epoch_argv[] = {
            "git", "show", "-s", "--format=%ct", "HEAD", NULL
        };
        char *end = NULL;
        long long parsed;

        if (!capture_command(
                root,
                epoch_argv,
                epoch_text,
                sizeof(epoch_text))) {
            fail("cannot read release commit timestamp.");
        }
        errno = 0;
        parsed = strtoll(epoch_text, &end, 10);
        if (errno != 0 || end == epoch_text ||
            end == NULL || *end != '\0' || parsed <= 0) {
            fail("release commit timestamp is invalid.");
        }
        source_epoch = (time_t)parsed;
    }

    {
        char *diff_argv[] = {
            "git", "diff", "--quiet", "HEAD", "--", NULL
        };
        if (run_command(root, diff_argv) != 0) {
            fail("tracked working tree differs from HEAD.");
        }
    }

    if (!path_join(
            version_path,
            sizeof(version_path),
            root,
            "VERSION") ||
        !read_trimmed(
            version_path, version, sizeof(version)) ||
        !valid_version(version)) {
        fail("VERSION is not a valid major.minor.patch value.");
    }
    if (!path_join(
            common,
            sizeof(common),
            root,
            common_relative)) {
        fail("Common path is too long.");
    }

    {
        char revspec[PATH_MAX];
        const int revspec_length = snprintf(
            revspec,
            sizeof(revspec),
            "HEAD:%s",
            common_relative);
        if (revspec_length <= 0 ||
            (size_t)revspec_length >= sizeof(revspec)) {
            fail("cannot form Common revision.");
        }
        char *pin_argv[] = {
            "git", "rev-parse", revspec, NULL
        };
        char *common_head_argv[] = {
            "git", "-C", common, "rev-parse", "HEAD", NULL
        };
        if (!capture_command(
                root, pin_argv, pin, sizeof(pin)) ||
            !capture_command(
                root,
                common_head_argv,
                common_head,
                sizeof(common_head)) ||
            strcmp(pin, common_head) != 0) {
            fail("checked-out Common does not match the pinned submodule commit.");
        }
    }

    if (!mkdir_one(argv[1])) {
        fail("cannot create output directory.");
    }
    if (argv[1][0] == '/') {
        const size_t output_length = strlen(argv[1]);
        if (output_length >= sizeof(output)) {
            fail("output directory path is too long.");
        }
        memcpy(output, argv[1], output_length + 1U);
    } else {
        char current[PATH_MAX];
        if (getcwd(current, sizeof(current)) == NULL ||
            !path_join(output, sizeof(output), current, argv[1])) {
            fail("cannot resolve output directory.");
        }
    }

    work = mkdtemp(work_template);
    if (work == NULL ||
        !path_join(
            source_root, sizeof(source_root), work, "source") ||
        mkdir(source_root, 0700) != 0 ||
        !path_join(
            common_export,
            sizeof(common_export),
            source_root,
            common_relative)) {
        fail("cannot create temporary source workspace.");
    }

    {
        char parent[PATH_MAX];
        char *cursor;
        if (strlen(common_export) >= sizeof(parent)) {
            goto cleanup;
        }
        strcpy(parent, common_export);
        for (cursor = parent + strlen(source_root) + 1U;
             *cursor != '\0';
             ++cursor) {
            if (*cursor == '/') {
                *cursor = '\0';
                if (!mkdir_one(parent)) {
                    goto cleanup;
                }
                *cursor = '/';
            }
        }
        if (!mkdir_one(parent)) {
            goto cleanup;
        }
    }

    if (!path_join(root_tar, sizeof(root_tar), work, "root.tar") ||
        !path_join(common_tar, sizeof(common_tar), work, "common.tar")) {
        goto cleanup;
    }
    {
        const int root_output_length = snprintf(
            root_output_argument,
            sizeof(root_output_argument),
            "--output=%s",
            root_tar);
        const int common_output_length = snprintf(
            common_output_argument,
            sizeof(common_output_argument),
            "--output=%s",
            common_tar);
        if (root_output_length <= 0 ||
            (size_t)root_output_length >= sizeof(root_output_argument) ||
            common_output_length <= 0 ||
            (size_t)common_output_length >= sizeof(common_output_argument)) {
            goto cleanup;
        }
    }

    {
        char *archive_root[] = {
            "git", "archive", "--format=tar",
            root_output_argument, "HEAD", NULL
        };
        char *extract_root[] = {
            "tar", "-xf", root_tar, "-C", source_root, NULL
        };
        char *archive_common[] = {
            "git", "-C", common, "archive", "--format=tar",
            common_output_argument, pin, NULL
        };
        char *extract_common[] = {
            "tar", "-xf", common_tar, "-C", common_export, NULL
        };
        if (run_command(root, archive_root) != 0 ||
            run_command(root, extract_root) != 0 ||
            run_command(root, archive_common) != 0 ||
            run_command(root, extract_common) != 0) {
            goto cleanup;
        }
    }

    if (!path_join(
            marker,
            sizeof(marker),
            common_export,
            ".system-settings-common-commit")) {
        goto cleanup;
    }
    {
        char marker_text[128];
        const int count = snprintf(
            marker_text, sizeof(marker_text), "%s\n", pin);
        if (count <= 0 || (size_t)count >= sizeof(marker_text) ||
            !write_text(marker, marker_text) ||
            !write_common_manifest(common_export)) {
            goto cleanup;
        }
    }

    /*
     * Git archives are content-stable but generated provenance files would
     * otherwise inherit wall-clock mtimes. Normalize the complete exported
     * tree to the release commit epoch before either archive is created.
     */
    if (!normalize_tree_times(source_root, "", source_epoch)) {
        goto cleanup;
    }

    if (!path_join(payload, sizeof(payload), work, "source.tar.gz")) {
        goto cleanup;
    }
    {
        char epoch_argument[64];
        const int epoch_length = snprintf(
            epoch_argument,
            sizeof(epoch_argument),
            "--mtime=@%lld",
            (long long)source_epoch);
        char *tar_argv[] = {
            "tar",
            "--sort=name",
            epoch_argument,
            "--owner=0",
            "--group=0",
            "--numeric-owner",
            "-czf", payload,
            "-C", source_root, ".",
            NULL
        };
        if (epoch_length <= 0 ||
            (size_t)epoch_length >= sizeof(epoch_argument)) {
            goto cleanup;
        }
        if (run_command(root, tar_argv) != 0) {
            goto cleanup;
        }
    }

    if (!path_join(
            installer_source,
            sizeof(installer_source),
            source_root,
            "packaging/native-installer.c") ||
        !path_join(
            installer_binary,
            sizeof(installer_binary),
            work,
            "native-installer")) {
        goto cleanup;
    }
    {
        char *compile_argv[] = {
            "cc", "-std=c11", "-O2",
            "-Wall", "-Wextra", "-Wpedantic", "-Werror",
            "-o", installer_binary, installer_source, NULL
        };
        if (run_command(root, compile_argv) != 0) {
            goto cleanup;
        }
    }

    {
        const int run_name_length = snprintf(
            output_argument,
            sizeof(output_argument),
            "System-Settings-%s-native.run",
            version);
        if (run_name_length <= 0 ||
            (size_t)run_name_length >= sizeof(output_argument)) {
            goto cleanup;
        }
    }
    if (!path_join(
            run_path,
            sizeof(run_path),
            output,
            output_argument) ||
        stat(payload, &payload_stat) != 0 ||
        payload_stat.st_size <= 0) {
        goto cleanup;
    }

    run = fopen(run_path, "wb");
    if (run == NULL ||
        !append_file(run, installer_binary) ||
        !append_file(run, payload) ||
        fprintf(
            run,
            "\nSSPAYLOAD=%020llu\n",
            (unsigned long long)payload_stat.st_size) != 32 ||
        fflush(run) != 0 ||
        fsync(fileno(run)) != 0) {
        if (run != NULL) {
            fclose(run);
            run = NULL;
        }
        goto cleanup;
    }
    if (fclose(run) != 0) {
        run = NULL;
        goto cleanup;
    }
    run = NULL;
    if (chmod(run_path, 0755) != 0) {
        goto cleanup;
    }

    {
        const int zip_name_length = snprintf(
            output_argument,
            sizeof(output_argument),
            "System-Settings-%s-source.zip",
            version);
        if (zip_name_length <= 0 ||
            (size_t)zip_name_length >= sizeof(output_argument)) {
            goto cleanup;
        }
    }
    if (!path_join(
            zip_path,
            sizeof(zip_path),
            output,
            output_argument)) {
        goto cleanup;
    }
    {
        char *zip_argv[] = {
            "cmake", "-E", "tar", "cf", zip_path,
            "--format=zip", "--", ".", NULL
        };
        if (run_command(source_root, zip_argv) != 0) {
            goto cleanup;
        }
    }

    {
        char *readelf_argv[] = {
            "readelf", "-h", run_path, NULL
        };
        char *help_argv[] = {
            run_path, "--help", NULL
        };
        if (run_command(root, readelf_argv) != 0 ||
            run_command(root, help_argv) != 0) {
            goto cleanup;
        }
    }

    if (!path_join(verified, sizeof(verified), work, "verified")) {
        goto cleanup;
    }
    {
        char *extract_argv[] = {
            run_path, "--extract", verified, NULL
        };
        if (run_command(root, extract_argv) != 0) {
            goto cleanup;
        }
    }

    if (!trees_equal(source_root, verified)) {
        goto cleanup;
    }

    if (!path_join(
            source_version,
            sizeof(source_version),
            source_root,
            "VERSION") ||
        !path_join(
            verified_version,
            sizeof(verified_version),
            verified,
            "VERSION") ||
        !path_join(
            source_common_version,
            sizeof(source_common_version),
            common_export,
            "VERSION") ||
        !path_join(
            verified_common_version,
            sizeof(verified_common_version),
            verified,
            "src/vendor/infiltratr-common/VERSION") ||
        !files_equal(source_version, verified_version) ||
        !files_equal(source_common_version, verified_common_version)) {
        goto cleanup;
    }

    if (!path_join(
            verify_build,
            sizeof(verify_build),
            work,
            "verify-build")) {
        goto cleanup;
    }
    {
        char *configure_argv[] = {
            "cmake", "-S", verified, "-B", verify_build,
            "-DBUILD_TESTING=OFF",
            "-DSYSTEM_SETTINGS_BUILD_PROFILE=generic",
            NULL
        };
        if (run_command(root, configure_argv) != 0) {
            goto cleanup;
        }
    }

    ok = true;

cleanup:
    if (run != NULL) {
        fclose(run);
    }
    if (work != NULL && remove_tree(work) != 0) {
        fprintf(stderr, "Warning: could not remove temporary workspace %s\n", work);
    }
    if (!ok) {
        fail("asset generation or verification failed.");
    }
    return EXIT_SUCCESS;
}
