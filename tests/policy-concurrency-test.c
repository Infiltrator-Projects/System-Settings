// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L

#include "system-settings/date-time-model.h"
#include "system-settings/temporal-policy-store.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <sys/wait.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            g_error("Policy concurrency test failed: %s (%s:%d)", \
                    #expression, __FILE__, __LINE__); \
        } \
    } while (0)

#define RETRY_ATTEMPTS 20U
#define RETRY_DELAY_USEC 10000U

static const InfiltratrTemporalPolicyV3 *policy_of(
    const SsDateTimeModel *model)
{
    const InfiltratrTemporalPolicyV3 *policy =
        ss_date_time_model_policy(model);
    CHECK(policy != NULL);
    return policy;
}

static int child_edit(int start_fd, bool calendar_edit)
{
    SsDateTimeModel model = SS_DATE_TIME_MODEL_INIT;
    char token;
    int result = EXIT_FAILURE;

    if (!ss_date_time_model_init(
            &model, ss_platform_temporal_policy_store())) {
        return EXIT_FAILURE;
    }
    if (read(start_fd, &token, 1U) != 1) {
        ss_date_time_model_clear(&model);
        return EXIT_FAILURE;
    }

    /*
     * Cross-process policy locks are deliberately short and non-blocking from
     * the caller's point of view. Under instrumentation a legitimate writer
     * can therefore observe BUSY while its peer is still committing. Retry the
     * public operation instead of turning that intended contract into a flaky
     * requirement that every writer win on its first attempt.
     */
    for (guint attempt = 0U; attempt < RETRY_ATTEMPTS; ++attempt) {
        const bool saved = calendar_edit
            ? ss_date_time_model_set_calendar(
                  &model, "egyptian-nabonassar")
            : ss_date_time_model_set_show_seconds(&model, true);
        if (saved) {
            result = EXIT_SUCCESS;
            break;
        }
        if (attempt + 1U < RETRY_ATTEMPTS) {
            g_usleep(RETRY_DELAY_USEC);
        }
    }

    ss_date_time_model_clear(&model);
    return result;
}

static pid_t spawn_cli_edit(
    const char *program,
    const char *option,
    const char *value)
{
    const pid_t child = fork();

    CHECK(child >= 0);
    if (child == 0) {
        execl(program, program, option, value, (char *)NULL);
        _exit(127);
    }
    return child;
}

static int wait_exit_code(pid_t child)
{
    int status = 0;

    CHECK(waitpid(child, &status, 0) == child);
    if (!WIFEXITED(status)) {
        return -1;
    }
    return WEXITSTATUS(status);
}

static int retry_cli_edit(
    const char *program,
    const char *option,
    const char *value)
{
    int exit_code = 2;

    for (guint attempt = 0U; attempt < RETRY_ATTEMPTS; ++attempt) {
        exit_code = wait_exit_code(spawn_cli_edit(program, option, value));
        if (exit_code == EXIT_SUCCESS || exit_code != 2) {
            return exit_code;
        }
        if (attempt + 1U < RETRY_ATTEMPTS) {
            g_usleep(RETRY_DELAY_USEC);
        }
    }
    return exit_code;
}

int main(int argc, char **argv)
{
    g_autofree gchar *root =
        g_dir_make_tmp("ss-policy-concurrency-XXXXXX", NULL);
    g_autofree gchar *policy_path = NULL;
    g_autofree gchar *policy_dir = NULL;
    int start_pipe[2];
    pid_t calendar_child;
    pid_t seconds_child;
    int status = 0;
    SsDateTimeModel initial = SS_DATE_TIME_MODEL_INIT;
    SsDateTimeModel final = SS_DATE_TIME_MODEL_INIT;

    CHECK(argc == 2);
    CHECK(root != NULL);
    CHECK(g_setenv("XDG_CONFIG_HOME", root, TRUE));
    CHECK(g_setenv("GSETTINGS_BACKEND", "memory", TRUE));
    CHECK(ss_date_time_model_init(
        &initial, ss_platform_temporal_policy_store()));
    CHECK(ss_date_time_model_set_clock_mode(
        &initial, "standard-24"));
    CHECK(!policy_of(&initial)->show_seconds);

    CHECK(pipe(start_pipe) == 0);
    calendar_child = fork();
    CHECK(calendar_child >= 0);
    if (calendar_child == 0) {
        close(start_pipe[1]);
        _exit(child_edit(start_pipe[0], true));
    }

    seconds_child = fork();
    CHECK(seconds_child >= 0);
    if (seconds_child == 0) {
        close(start_pipe[1]);
        _exit(child_edit(start_pipe[0], false));
    }

    close(start_pipe[0]);
    CHECK(write(start_pipe[1], "go", 2U) == 2);
    close(start_pipe[1]);

    CHECK(waitpid(calendar_child, &status, 0) == calendar_child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS);
    CHECK(waitpid(seconds_child, &status, 0) == seconds_child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS);

    CHECK(ss_date_time_model_init(
        &final, ss_platform_temporal_policy_store()));
    CHECK(strcmp(
        policy_of(&final)->calendar,
        "egyptian-nabonassar") == 0);
    CHECK(policy_of(&final)->show_seconds);
    CHECK(strcmp(policy_of(&final)->clock_mode, "standard-24") == 0);

    /*
     * Exercise the public CLI transaction boundary too. Each process changes
     * a different field. The platform lock is intentionally bounded, so one
     * initial command may report the retryable exit code 2 under contention.
     * After retry, both successful transactions must retain the peer's field.
     */
    calendar_child = spawn_cli_edit(
        argv[1], "--calendar", "gregorian");
    seconds_child = spawn_cli_edit(
        argv[1], "--seconds", "off");

    const int calendar_exit = wait_exit_code(calendar_child);
    const int seconds_exit = wait_exit_code(seconds_child);

    CHECK(calendar_exit == EXIT_SUCCESS || calendar_exit == 2);
    CHECK(seconds_exit == EXIT_SUCCESS || seconds_exit == 2);
    if (calendar_exit != EXIT_SUCCESS) {
        CHECK(retry_cli_edit(
            argv[1], "--calendar", "gregorian") == EXIT_SUCCESS);
    }
    if (seconds_exit != EXIT_SUCCESS) {
        CHECK(retry_cli_edit(
            argv[1], "--seconds", "off") == EXIT_SUCCESS);
    }

    CHECK(ss_date_time_model_reload(&final));
    CHECK(strcmp(policy_of(&final)->calendar, "gregorian") == 0);
    CHECK(!policy_of(&final)->show_seconds);
    CHECK(strcmp(policy_of(&final)->clock_mode, "standard-24") == 0);

    /*
     * Policy lock contention is bounded too. A wedged peer must result in a
     * retryable failure rather than seconds of unresponsive UI.
     */
    {
        const SsTemporalPolicyStore *store =
            ss_platform_temporal_policy_store();
        int ready_pipe[2];
        CHECK(store != NULL && store->begin_update != NULL &&
              store->end_update != NULL);
        CHECK(pipe(ready_pipe) == 0);
        pid_t holder = fork();
        CHECK(holder >= 0);
        if (holder == 0) {
            char ready = '1';
            close(ready_pipe[0]);
            if (!store->begin_update()) _exit(20);
            if (write(ready_pipe[1], &ready, 1U) != 1) _exit(21);
            close(ready_pipe[1]);
            g_usleep(600000U);
            store->end_update();
            _exit(0);
        }
        close(ready_pipe[1]);
        char ready = 0;
        CHECK(read(ready_pipe[0], &ready, 1U) == 1);
        close(ready_pipe[0]);
        const gint64 start = g_get_monotonic_time();
        CHECK(!store->begin_update());
        const gint64 elapsed = g_get_monotonic_time() - start;
        CHECK(elapsed < 500000);
        CHECK(waitpid(holder, &status, 0) == holder);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS);
        CHECK(store->begin_update());
        store->end_update();
    }

    policy_path = g_build_filename(
        root, "infiltrator", "presentation.conf", NULL);
    policy_dir = g_path_get_dirname(policy_path);
    CHECK(g_remove(policy_path) == 0);
    {
        g_autofree gchar *lock_path = g_build_filename(
            policy_dir, "presentation.lock", NULL);
        (void)g_remove(lock_path);
    }
    CHECK(g_rmdir(policy_dir) == 0);
    CHECK(g_rmdir(root) == 0);
    ss_date_time_model_clear(&final);
    ss_date_time_model_clear(&initial);
    return EXIT_SUCCESS;
}
