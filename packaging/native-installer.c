// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L

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
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define FOOTER_SIZE 32U
#define FOOTER_PREFIX "\nSSPAYLOAD="
#define FOOTER_PREFIX_SIZE 11U
#define PAYLOAD_DIGITS 20U

static void fail(const char *message)
{
    fprintf(stderr, "System Settings native installer: %s\n", message);
    exit(EXIT_FAILURE);
}

static bool ends_with(const char *value, const char *suffix)
{
    size_t value_length;
    size_t suffix_length;

    if (value == NULL || suffix == NULL) {
        return false;
    }
    value_length = strlen(value);
    suffix_length = strlen(suffix);
    return value_length >= suffix_length &&
           strcmp(value + value_length - suffix_length, suffix) == 0;
}

static bool command_exists(const char *name)
{
    const char *path = getenv("PATH");
    char *copy;
    char *save = NULL;
    char *part;
    bool found = false;

    if (name == NULL || name[0] == '\0' || path == NULL) {
        return false;
    }
    copy = strdup(path);
    if (copy == NULL) {
        return false;
    }
    for (part = strtok_r(copy, ":", &save);
         part != NULL;
         part = strtok_r(NULL, ":", &save)) {
        char candidate[PATH_MAX];
        const int count =
            snprintf(candidate, sizeof(candidate), "%s/%s", part, name);
        if (count > 0 && (size_t)count < sizeof(candidate) &&
            access(candidate, X_OK) == 0) {
            found = true;
            break;
        }
    }
    free(copy);
    return found;
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
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
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
    return WIFEXITED(status) &&
           WEXITSTATUS(status) == 0 &&
           used > 0U;
}

static bool compiler_supports_gcc_pgo(void)
{
    char output[1024];
    char *argv[] = {"cc", "--version", NULL};

    if (!capture_command(NULL, argv, output, sizeof(output))) {
        return false;
    }
    return strstr(output, "clang") == NULL &&
           (strstr(output, "gcc") != NULL ||
            strstr(output, "GCC") != NULL ||
            strstr(output, "Free Software Foundation") != NULL);
}

static bool write_training_evidence(const char *path)
{
    static const char evidence[] = "trained-pgo\n";
    int fd;
    size_t offset = 0U;

    if (path == NULL || path[0] == '\0') {
        return false;
    }
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        return false;
    }
    while (offset < sizeof(evidence) - 1U) {
        const ssize_t count = write(
            fd, evidence + offset, sizeof(evidence) - 1U - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            (void)close(fd);
            return false;
        }
        offset += (size_t)count;
    }
    const int sync_result = fsync(fd);
    const int close_result = close(fd);
    return sync_result == 0 && close_result == 0;
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
        if (entry == NULL) {
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char child[PATH_MAX];
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) <= 0 ||
            strlen(child) >= sizeof(child) - 1U ||
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

static bool executable_path(const char *argv0, char *out, size_t capacity)
{
    ssize_t length;

    if (out == NULL || capacity < 2U) {
        return false;
    }
    length = readlink("/proc/self/exe", out, capacity - 1U);
    if (length > 0 && (size_t)length < capacity) {
        out[length] = '\0';
        return true;
    }
    if (argv0 == NULL || argv0[0] == '\0') {
        return false;
    }
    const size_t fallback_length = strlen(argv0);
    if (fallback_length >= capacity) {
        return false;
    }
    memcpy(out, argv0, fallback_length + 1U);
    return true;
}

static bool payload_bounds(
    int fd,
    off_t *payload_start,
    uint64_t *payload_size)
{
    struct stat st;
    char footer[FOOTER_SIZE + 1U] = {0};
    char digits[PAYLOAD_DIGITS + 1U] = {0};
    char *end = NULL;
    unsigned long long size;

    if (fd < 0 || payload_start == NULL || payload_size == NULL ||
        fstat(fd, &st) != 0 || st.st_size < (off_t)FOOTER_SIZE ||
        pread(fd, footer, FOOTER_SIZE,
              st.st_size - (off_t)FOOTER_SIZE) != (ssize_t)FOOTER_SIZE) {
        return false;
    }
    if (memcmp(footer, FOOTER_PREFIX, FOOTER_PREFIX_SIZE) != 0 ||
        footer[FOOTER_SIZE - 1U] != '\n') {
        return false;
    }
    memcpy(digits, footer + FOOTER_PREFIX_SIZE, PAYLOAD_DIGITS);
    for (size_t i = 0U; i < PAYLOAD_DIGITS; ++i) {
        if (digits[i] < '0' || digits[i] > '9') {
            return false;
        }
    }
    errno = 0;
    size = strtoull(digits, &end, 10);
    if (errno != 0 || end == NULL || *end != '\0' || size == 0U) {
        return false;
    }
    if ((uint64_t)st.st_size < size + FOOTER_SIZE) {
        return false;
    }
    *payload_size = (uint64_t)size;
    *payload_start =
        st.st_size - (off_t)FOOTER_SIZE - (off_t)size;
    return *payload_start > 0;
}

static bool copy_payload(
    const char *self,
    char *archive_path,
    size_t archive_capacity)
{
    int input = -1;
    int output = -1;
    off_t start;
    uint64_t remaining;
    char temporary[] = "/tmp/system-settings-payload-XXXXXX";
    unsigned char buffer[64U * 1024U];
    bool ok = false;

    input = open(self, O_RDONLY | O_CLOEXEC);
    if (input < 0 || !payload_bounds(input, &start, &remaining)) {
        goto cleanup;
    }
    output = mkstemp(temporary);
    if (output < 0 || fchmod(output, 0600) != 0 ||
        lseek(input, start, SEEK_SET) < 0) {
        goto cleanup;
    }

    while (remaining > 0U) {
        size_t want = remaining > sizeof(buffer)
            ? sizeof(buffer) : (size_t)remaining;
        ssize_t count = read(input, buffer, want);
        if (count <= 0) {
            goto cleanup;
        }
        size_t written = 0U;
        while (written < (size_t)count) {
            ssize_t out_count = write(
                output, buffer + written, (size_t)count - written);
            if (out_count <= 0) {
                goto cleanup;
            }
            written += (size_t)out_count;
        }
        remaining -= (uint64_t)count;
    }

    if (fsync(output) != 0 ||
        strlen(temporary) >= archive_capacity) {
        goto cleanup;
    }
    strcpy(archive_path, temporary);
    ok = true;

cleanup:
    if (output >= 0) {
        close(output);
    }
    if (input >= 0) {
        close(input);
    }
    if (!ok && temporary[0] != '\0') {
        unlink(temporary);
    }
    return ok;
}

static bool extract_payload(
    const char *self,
    const char *destination)
{
    char archive[PATH_MAX] = {0};
    char *tar_argv[] = {
        "tar", "-xzf", archive, "-C", (char *)destination, NULL
    };
    bool ok;

    if (!copy_payload(self, archive, sizeof(archive))) {
        return false;
    }
    ok = run_command(NULL, tar_argv) == 0;
    unlink(archive);
    return ok;
}

static bool find_debian_package(
    const char *directory,
    char *out,
    size_t capacity)
{
    DIR *dir = opendir(directory);
    bool found = false;

    if (dir == NULL) {
        return false;
    }
    for (;;) {
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            break;
        }
        if (strncmp(
                entry->d_name,
                "infiltrator-system-settings_",
                strlen("infiltrator-system-settings_")) != 0 ||
            !ends_with(entry->d_name, ".deb")) {
            continue;
        }
        if (found ||
            snprintf(out, capacity, "%s/%s", directory, entry->d_name) <= 0 ||
            strlen(out) >= capacity - 1U) {
            closedir(dir);
            return false;
        }
        found = true;
    }
    closedir(dir);
    return found;
}

static void show_help(void)
{
    puts("System Settings native installer");
    puts("Usage: ./System-Settings-VERSION-native.run [--extract DIRECTORY|--build-only]");
    puts("Default: build for this CPU with LTO and trained GCC PGO, test, package, then install through APT.");
    puts("--build-only: execute the complete native build/test/package path without installing.");
    puts("Build prerequisites: build-essential cmake pkg-config libgtk-4-dev libgeocode-glib-dev xvfb dbus-x11");
}

int main(int argc, char **argv)
{
    static const char *const required[] = {
        "tar", "cmake", "cc", "make", "ctest", "cpack",
        "pkg-config", "dpkg-deb", "xvfb-run", "dbus-daemon"
    };
    char self[PATH_MAX];
    char work_template[] = "/tmp/system-settings-native-XXXXXX";
    char *work;
    char build[PATH_MAX];
    char deb[PATH_MAX];
    char pgo[PATH_MAX];
    char pgo_evidence[PATH_MAX];
    char generate_flags[PATH_MAX + 128U];
    char final_flags[PATH_MAX + 128U];
    char jobs[32];
    long processors;
    bool build_only = false;
    const bool use_pgo = compiler_supports_gcc_pgo();

    if (argc == 2 &&
        (strcmp(argv[1], "--help") == 0 ||
         strcmp(argv[1], "-h") == 0)) {
        show_help();
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--build-only") == 0) {
        build_only = true;
    }
    if (!executable_path(argv[0], self, sizeof(self))) {
        fail("cannot resolve the installer executable path.");
    }

    if (argc == 3 && strcmp(argv[1], "--extract") == 0) {
        if (argv[2][0] == '\0' || mkdir(argv[2], 0700) != 0) {
            fail("supply a new extraction directory.");
        }
        if (!extract_payload(self, argv[2])) {
            (void)remove_tree(argv[2]);
            fail("could not extract the embedded source payload.");
        }
        return EXIT_SUCCESS;
    }
    if (argc != 1 && !build_only) {
        fprintf(stderr, "Unknown option; use --help.\n");
        return 2;
    }
    if (geteuid() == 0) {
        fail("run as your ordinary user; only final package installation uses sudo.");
    }
    for (size_t i = 0U; i < sizeof(required) / sizeof(required[0]); ++i) {
        if (!command_exists(required[i])) {
            fprintf(stderr, "Missing prerequisite: %s\n", required[i]);
            return EXIT_FAILURE;
        }
    }
    if (!build_only && !command_exists("sudo")) {
        fputs("Missing prerequisite: sudo\n", stderr);
        return EXIT_FAILURE;
    }
    if (!use_pgo) {
        fail("the native profile requires GCC so LTO and trained PGO are both guaranteed.");
    }

    {
        char *gtk_argv[] = {
            "pkg-config", "--exists", "gtk4 >= 4.6", NULL
        };
        char *geocode_argv[] = {
            "pkg-config", "--exists", "geocode-glib-2.0", NULL
        };
        if (run_command(NULL, gtk_argv) != 0 ||
            run_command(NULL, geocode_argv) != 0) {
            fail("install libgtk-4-dev and libgeocode-glib-dev before building.");
        }
    }

    work = mkdtemp(work_template);
    if (work == NULL) {
        fail("could not create a temporary build directory.");
    }
    if (!extract_payload(self, work)) {
        (void)remove_tree(work);
        fail("could not extract the embedded source payload.");
    }
    if (snprintf(build, sizeof(build), "%s/build", work) <= 0 ||
        strlen(build) >= sizeof(build) - 1U) {
        (void)remove_tree(work);
        fail("temporary build path is too long.");
    }

    if (snprintf(pgo, sizeof(pgo), "%s/pgo", work) <= 0 ||
        strlen(pgo) >= sizeof(pgo) - 1U ||
        mkdir(pgo, 0700) != 0 ||
        snprintf(
            pgo_evidence,
            sizeof(pgo_evidence),
            "%s/trained.stamp",
            pgo) <= 0 ||
        strlen(pgo_evidence) >= sizeof(pgo_evidence) - 1U) {
        (void)remove_tree(work);
        fail("could not create the PGO training directory.");
    }

    if (use_pgo) {
        if (snprintf(
                generate_flags,
                sizeof(generate_flags),
                "-O3 -DNDEBUG -march=native -mtune=native -flto -fprofile-generate=%s",
                pgo) <= 0 ||
            snprintf(
                final_flags,
                sizeof(final_flags),
                "-O3 -DNDEBUG -march=native -mtune=native -flto -fprofile-use=%s -fprofile-correction -Wmissing-profile -Wno-error=missing-profile",
                pgo) <= 0 ||
            strlen(generate_flags) >= sizeof(generate_flags) - 1U ||
            strlen(final_flags) >= sizeof(final_flags) - 1U) {
            (void)remove_tree(work);
            fail("native PGO flags are too long.");
        }
    } else {
        if (snprintf(
                final_flags,
                sizeof(final_flags),
                "-O3 -DNDEBUG -march=native -mtune=native -flto") <= 0) {
            (void)remove_tree(work);
            fail("native optimization flags are invalid.");
        }
    }

    processors = sysconf(_SC_NPROCESSORS_ONLN);
    if (processors < 1) {
        processors = 1;
    } else if (processors > 1) {
        --processors;
    }
    snprintf(jobs, sizeof(jobs), "%ld", processors);

    {
        char c_flags_argument[PATH_MAX + 160U];
        char build_profile_argument[64];
        char evidence_argument[PATH_MAX + 64U];
        char common_tests_argument[64];
        char *configure_argv[] = {
            "cmake", "-S", work, "-B", build,
            "-DCMAKE_BUILD_TYPE=Release",
            build_profile_argument,
            "-DBUILD_TESTING=ON",
            common_tests_argument,
            c_flags_argument,
            evidence_argument,
            NULL
        };
        char *build_argv[] = {
            "cmake", "--build", build, "--parallel", jobs, NULL
        };
        char *common_training_argv[] = {
            "cmake", "--build", build, "--parallel", jobs, "--target",
            "infiltratr-arithmetic-smoke",
            "infiltratr-config-smoke",
            "infiltratr-i18n-smoke",
            "infiltratr-token-smoke",
            "infiltratr-timing-smoke",
            "infiltratr-temporal-smoke",
            "infiltratr-graphics-smoke",
            "infiltratr-escape-smoke",
            "infiltratr-design-smoke",
            "infiltratr-format-smoke",
            "infiltratr-dynlib-smoke",
            "infiltratr-portable-smoke",
            "infiltratr-portable-contract",
            "infiltratr-encoding-contract",
            "infiltratr-completion-contract",
            "infiltratr-shared-smoke",
            "infiltratr-core-smoke",
            "infiltratr-posix-contract",
            "infiltratr-posix-path-smoke",
            "infiltratr-posix-io-contract",
            "infiltratr-posix-numeric-contract",
            "infiltratr-temporal-posix-smoke",
            NULL
        };
        char *test_argv[] = {
            "ctest", "--test-dir", build, "--output-on-failure", NULL
        };
        char *cpack_argv[] = {
            "cpack", "-G", "DEB", NULL
        };

        if (use_pgo) {
            if (snprintf(
                    evidence_argument,
                    sizeof(evidence_argument),
                    "-DSYSTEM_SETTINGS_NATIVE_PGO_EVIDENCE=") <= 0 ||
                snprintf(
                    common_tests_argument,
                    sizeof(common_tests_argument),
                    "-DINFILTRATR_COMMON_BUILD_TESTS=ON") <= 0 ||
                snprintf(
                    build_profile_argument,
                    sizeof(build_profile_argument),
                    "-DSYSTEM_SETTINGS_BUILD_PROFILE=development") <= 0 ||
                snprintf(
                    c_flags_argument,
                    sizeof(c_flags_argument),
                    "-DCMAKE_C_FLAGS_RELEASE=%s",
                    generate_flags) <= 0 ||
                run_command(NULL, configure_argv) != 0 ||
                run_command(NULL, build_argv) != 0 ||
                run_command(NULL, common_training_argv) != 0 ||
                run_command(NULL, test_argv) != 0 ||
                !write_training_evidence(pgo_evidence)) {
                (void)remove_tree(work);
                fail("native PGO training build, test suite or evidence publication failed.");
            }
        }

        const int evidence_count = snprintf(
            evidence_argument,
            sizeof(evidence_argument),
            "-DSYSTEM_SETTINGS_NATIVE_PGO_EVIDENCE=%s",
            pgo_evidence);
        if (evidence_count <= 0 ||
            (size_t)evidence_count >= sizeof(evidence_argument) ||
            snprintf(
                common_tests_argument,
                sizeof(common_tests_argument),
                "-DINFILTRATR_COMMON_BUILD_TESTS=OFF") <= 0 ||
            snprintf(
                build_profile_argument,
                sizeof(build_profile_argument),
                "-DSYSTEM_SETTINGS_BUILD_PROFILE=native") <= 0 ||
            snprintf(
                c_flags_argument,
                sizeof(c_flags_argument),
                "-DCMAKE_C_FLAGS_RELEASE=%s",
                final_flags) <= 0 ||
            run_command(NULL, configure_argv) != 0 ||
            run_command(NULL, build_argv) != 0) {
            (void)remove_tree(work);
            fail("native LTO/PGO build failed.");
        }
        if (run_command(NULL, test_argv) != 0) {
            (void)remove_tree(work);
            fail("optimized native test suite failed; package was not installed.");
        }
        if (run_command(build, cpack_argv) != 0 ||
            !find_debian_package(build, deb, sizeof(deb))) {
            (void)remove_tree(work);
            fail("Debian package creation failed.");
        }
    }

    if (!build_only) {
        char *install_argv[] = {
            "sudo", "apt-get", "install", "-y", deb, NULL
        };
        if (run_command(NULL, install_argv) != 0) {
            (void)remove_tree(work);
            fail("APT installation failed.");
        }
    } else {
        puts("Native LTO/PGO build, optimized tests and Debian packaging verified.");
    }

    if (remove_tree(work) != 0) {
        fprintf(stderr, "Warning: could not remove temporary build directory %s\n", work);
    }
    return EXIT_SUCCESS;
}
