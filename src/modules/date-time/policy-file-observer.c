// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file policy-file-observer.c
 * @brief Atomic-replacement-safe observation of one policy file.
 */
#include "policy-file-observer.h"

#include <gio/gio.h>
#include <stdbool.h>

struct SsPolicyFileObserver {
    GFile *target_file;
    GFile *target_directory;
    GFile *watch_directory;
    GFileMonitor *monitor;
    SsPolicyFileObserverCallback callback;
    gpointer user_data;
    guint retry_id;
    bool target_exists;
};

static gboolean retry_bind(gpointer user_data);

static bool relevant_event(GFileMonitorEvent event)
{
    switch (event) {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_CREATED:
    case G_FILE_MONITOR_EVENT_DELETED:
    case G_FILE_MONITOR_EVENT_MOVED:
    case G_FILE_MONITOR_EVENT_RENAMED:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_MOVED_OUT:
        return true;
    default:
        return false;
    }
}

static GFile *nearest_existing_directory(
    const SsPolicyFileObserver *observer)
{
    GFile *candidate;

    if (observer == NULL || observer->target_directory == NULL) {
        return NULL;
    }

    candidate = g_object_ref(observer->target_directory);
    while (candidate != NULL &&
           g_file_query_file_type(
               candidate,
               G_FILE_QUERY_INFO_NONE,
               NULL) != G_FILE_TYPE_DIRECTORY) {
        GFile *parent = g_file_get_parent(candidate);
        g_object_unref(candidate);
        candidate = parent;
    }
    return candidate;
}

static void schedule_retry(SsPolicyFileObserver *observer)
{
    if (observer == NULL || observer->retry_id != 0U) {
        return;
    }
    observer->retry_id = g_timeout_add_seconds(
        30U, retry_bind, observer);
    g_source_set_name_by_id(
        observer->retry_id,
        "[system-settings] policy observer rebind retry");
}

static bool bind_monitor(SsPolicyFileObserver *observer);

static void monitor_changed(
    GFileMonitor *monitor G_GNUC_UNUSED,
    GFile *file,
    GFile *other_file,
    GFileMonitorEvent event,
    gpointer user_data)
{
    SsPolicyFileObserver *observer = user_data;
    g_autoptr(GFile) desired_directory = NULL;
    const bool old_exists =
        observer != NULL ? observer->target_exists : false;
    bool target_event = false;
    bool rebound = false;

    if (observer == NULL || !relevant_event(event)) {
        return;
    }

    if (file != NULL && g_file_equal(file, observer->target_file)) {
        target_event = true;
    }
    if (other_file != NULL &&
        g_file_equal(other_file, observer->target_file)) {
        target_event = true;
    }

    desired_directory = nearest_existing_directory(observer);
    if (desired_directory != NULL &&
        (observer->watch_directory == NULL ||
         !g_file_equal(
             desired_directory,
             observer->watch_directory))) {
        rebound = bind_monitor(observer);
    }

    observer->target_exists =
        g_file_query_exists(observer->target_file, NULL);

    /*
     * Atomic writers commonly create a sibling temporary file and rename it
     * over the target. React to either endpoint naming the target, to a
     * target existence transition, or to a parent-directory rebind where the
     * target already exists by the time the parent creation event is handled.
     */
    if ((target_event ||
         old_exists != observer->target_exists ||
         (rebound && observer->target_exists)) &&
        observer->callback != NULL) {
        observer->callback(observer->user_data);
    }
}

static bool bind_monitor(SsPolicyFileObserver *observer)
{
    g_autoptr(GFile) directory = NULL;
    GFileMonitor *monitor;

    if (observer == NULL) {
        return false;
    }

    directory = nearest_existing_directory(observer);
    if (directory == NULL) {
        schedule_retry(observer);
        return false;
    }

    if (observer->monitor != NULL &&
        observer->watch_directory != NULL &&
        g_file_equal(directory, observer->watch_directory)) {
        return true;
    }

    monitor = g_file_monitor_directory(
        directory,
        G_FILE_MONITOR_WATCH_MOVES,
        NULL,
        NULL);
    if (monitor == NULL) {
        schedule_retry(observer);
        return false;
    }

    g_clear_object(&observer->monitor);
    g_clear_object(&observer->watch_directory);
    observer->watch_directory = g_object_ref(directory);
    observer->monitor = monitor;
    g_signal_connect(
        observer->monitor,
        "changed",
        G_CALLBACK(monitor_changed),
        observer);

    if (observer->retry_id != 0U) {
        g_source_remove(observer->retry_id);
        observer->retry_id = 0U;
    }
    return true;
}

static gboolean retry_bind(gpointer user_data)
{
    SsPolicyFileObserver *observer = user_data;

    if (observer == NULL) {
        return G_SOURCE_REMOVE;
    }
    if (bind_monitor(observer)) {
        observer->retry_id = 0U;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

SsPolicyFileObserver *ss_policy_file_observer_new(
    const char *path,
    SsPolicyFileObserverCallback callback,
    gpointer user_data)
{
    SsPolicyFileObserver *observer;

    if (path == NULL || path[0] == '\0' || callback == NULL) {
        return NULL;
    }

    observer = g_new0(SsPolicyFileObserver, 1);
    observer->target_file = g_file_new_for_path(path);
    observer->target_directory =
        g_file_get_parent(observer->target_file);
    observer->callback = callback;
    observer->user_data = user_data;
    observer->target_exists =
        g_file_query_exists(observer->target_file, NULL);

    if (observer->target_directory == NULL) {
        ss_policy_file_observer_free(observer);
        return NULL;
    }

    if (!bind_monitor(observer)) {
        schedule_retry(observer);
    }
    return observer;
}

void ss_policy_file_observer_free(SsPolicyFileObserver *observer)
{
    if (observer == NULL) {
        return;
    }

    if (observer->retry_id != 0U) {
        g_source_remove(observer->retry_id);
        observer->retry_id = 0U;
    }
    g_clear_object(&observer->monitor);
    g_clear_object(&observer->watch_directory);
    g_clear_object(&observer->target_directory);
    g_clear_object(&observer->target_file);
    g_free(observer);
}
