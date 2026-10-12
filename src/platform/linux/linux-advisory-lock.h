// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SYSTEM_SETTINGS_LINUX_ADVISORY_LOCK_H
#define SYSTEM_SETTINGS_LINUX_ADVISORY_LOCK_H

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <sys/file.h>
#include <unistd.h>

typedef enum SsLinuxAdvisoryLockResult {
    SS_LINUX_ADVISORY_LOCK_OK = 0,
    SS_LINUX_ADVISORY_LOCK_BUSY,
    SS_LINUX_ADVISORY_LOCK_IO_ERROR,
    SS_LINUX_ADVISORY_LOCK_ALREADY_HELD
} SsLinuxAdvisoryLockResult;

typedef struct SsLinuxAdvisoryLock {
    int fd;
} SsLinuxAdvisoryLock;

#define SS_LINUX_ADVISORY_LOCK_INIT { .fd = -1 }

static inline SsLinuxAdvisoryLockResult ss_linux_advisory_lock_acquire(
    SsLinuxAdvisoryLock *lock,
    const char *directory,
    const char *lock_path,
    gint64 wait_usec,
    gint64 retry_usec)
{
    int fd;
    const gint64 deadline = g_get_monotonic_time() + MAX(wait_usec, 0);

    if (lock == NULL || directory == NULL || lock_path == NULL ||
        directory[0] == '\0' || lock_path[0] == '\0') {
        return SS_LINUX_ADVISORY_LOCK_IO_ERROR;
    }
    if (lock->fd >= 0) {
        return SS_LINUX_ADVISORY_LOCK_ALREADY_HELD;
    }
    if (g_mkdir_with_parents(directory, 0700) != 0 && errno != EEXIST) {
        return SS_LINUX_ADVISORY_LOCK_IO_ERROR;
    }
    if (g_chmod(directory, 0700) != 0) {
        return SS_LINUX_ADVISORY_LOCK_IO_ERROR;
    }

    fd = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0) {
        return SS_LINUX_ADVISORY_LOCK_IO_ERROR;
    }

    for (;;) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
            lock->fd = fd;
            return SS_LINUX_ADVISORY_LOCK_OK;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            (void)close(fd);
            return SS_LINUX_ADVISORY_LOCK_IO_ERROR;
        }
        if (wait_usec <= 0 || g_get_monotonic_time() >= deadline) {
            (void)close(fd);
            return SS_LINUX_ADVISORY_LOCK_BUSY;
        }
        g_usleep((gulong)MAX(retry_usec, 1));
    }
}

static inline void ss_linux_advisory_lock_release(
    SsLinuxAdvisoryLock *lock)
{
    if (lock == NULL || lock->fd < 0) {
        return;
    }
    (void)flock(lock->fd, LOCK_UN);
    (void)close(lock->fd);
    lock->fd = -1;
}

#endif
