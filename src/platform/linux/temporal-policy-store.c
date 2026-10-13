// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
/**
 * @file temporal-policy-store.c
 * @brief POSIX temporal-policy persistence plus Cinnamon compatibility seed.
 */
#include "system-settings/temporal-policy-store.h"
#include "system-settings/cinnamon-interface.h"
#include "linux-advisory-lock.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <infiltratr/core.h>
#include <infiltratr/temporal_posix.h>

static SsLinuxAdvisoryLock update_lock = SS_LINUX_ADVISORY_LOCK_INIT;

#define SS_LOCK_RETRY_USEC 5000

static bool begin_update(void)
{
    g_autofree gchar *directory = NULL;
    g_autofree gchar *lock_path = NULL;
    SsLinuxAdvisoryLockResult result;

    /*
     * Keep the lock path allocation-backed just like Common's policy path.
     * A valid long XDG_CONFIG_HOME must not be writable by the persistence
     * layer while being impossible to serialize at the transaction boundary.
     */
    directory = g_build_filename(
        g_get_user_config_dir(), "infiltrator", NULL);
    lock_path = directory != NULL
        ? g_build_filename(directory, "presentation.lock", NULL)
        : NULL;
    if (directory == NULL || lock_path == NULL) {
        return false;
    }

    result = ss_linux_advisory_lock_acquire(
        &update_lock,
        directory,
        lock_path,
        SS_LINUX_ADVISORY_LOCK_MAX_WAIT_USEC,
        SS_LOCK_RETRY_USEC);
    if (result == SS_LINUX_ADVISORY_LOCK_BUSY) {
        g_debug("Temporal policy update lock is busy.");
    } else if (result == SS_LINUX_ADVISORY_LOCK_IO_ERROR) {
        g_warning("Unable to acquire the temporal policy update lock due to an I/O error.");
    }
    return result == SS_LINUX_ADVISORY_LOCK_OK;
}

static void end_update(void)
{
    ss_linux_advisory_lock_release(&update_lock);
}

static void load_cinnamon_defaults(InfiltratrTemporalPolicyV3 *policy)
{
    g_autoptr(GSettings) settings = ss_cinnamon_interface_settings_new();
    const char *mode;

    if (policy == NULL || settings == NULL)
        return;

    mode = g_settings_get_boolean(settings, "clock-use-24h")
        ? "standard-24" : "standard-12";
    infiltratr_copy_string(policy->clock_mode,
                           sizeof(policy->clock_mode),
                           mode);
    policy->show_seconds =
        g_settings_get_boolean(settings, "clock-show-seconds") != FALSE;

}

static bool mirror_cinnamon_compatibility(
    const InfiltratrTemporalPolicyV3 *policy)
{
    g_autoptr(GSettings) settings = ss_cinnamon_interface_settings_new();
    gboolean ok = TRUE;

    if (policy == NULL)
        return false;
    if (settings == NULL)
        return true;

    if (infiltratr_string_equal(policy->clock_mode, "standard-24")) {
        ok = ss_cinnamon_interface_set_clock_use_24h(
                 settings, true) && ok;
    } else if (infiltratr_string_equal(
                   policy->clock_mode, "standard-12")) {
        ok = ss_cinnamon_interface_set_clock_use_24h(
                 settings, false) && ok;
    }

    ok = g_settings_set_boolean(
        settings, "clock-show-seconds",
        policy->show_seconds ? TRUE : FALSE) && ok;
    g_settings_sync();

    if (!ok)
        g_warning("Unable to mirror temporal compatibility settings to Cinnamon.");
    return ok;
}

static bool platform_compatibility_matches(
    const InfiltratrTemporalPolicyV3 *policy)
{
    g_autoptr(GSettings) settings = ss_cinnamon_interface_settings_new();
    bool value = false;

    if (policy == NULL) {
        return false;
    }
    if (settings == NULL) {
        return true;
    }

    if (infiltratr_string_equal(policy->clock_mode, "standard-24") ||
        infiltratr_string_equal(policy->clock_mode, "standard-12")) {
        const bool expected_24h =
            infiltratr_string_equal(policy->clock_mode, "standard-24");
        if (!ss_cinnamon_interface_get_boolean(
                settings, "clock-use-24h", &value) ||
            value != expected_24h) {
            return false;
        }
    }

    return ss_cinnamon_interface_get_boolean(
               settings, "clock-show-seconds", &value) &&
           value == policy->show_seconds;
}

static bool platform_load(InfiltratrTemporalPolicyV3 *policy, bool *found)
{
    /*
     * Invalid/empty optional policy must not brick Settings. Recover to Common
     * defaults, then seed the conventional clock/seconds values from Cinnamon.
     * Permission/I/O failures remain real failures rather than being hidden.
     */
    if (policy == NULL || found == NULL) {
        return false;
    }
    const InfiltratrIoResult result =
        infiltratr_temporal_posix_policy_load(policy, found);

    if (result == INFILTRATR_IO_INVALID_VALUE ||
        result == INFILTRATR_IO_EMPTY) {
        *found = false;
        if (!infiltratr_temporal_policy_v3_default(policy))
            return false;
        load_cinnamon_defaults(policy);
        return true;
    }
    if (result != INFILTRATR_IO_OK)
        return false;

    if (!*found)
        load_cinnamon_defaults(policy);
    return true;
}

static bool platform_save(const InfiltratrTemporalPolicyV3 *policy)
{
    if (infiltratr_temporal_posix_policy_save(policy) != 0)
        return false;

    /*
     * Keep conventional Mint consumers aligned where the richer Infiltrator
     * policy has an exact Cinnamon representation. Extended clock/calendar
     * modes remain Infiltrator-only rather than being forced into a false
     * platform equivalent.
     */
    (void)mirror_cinnamon_compatibility(policy);
    return true;
}

static bool platform_clear(void)
{
    g_autofree gchar *path = NULL;

    /*
     * Common owns validated load/save and the canonical XDG contract. Its
     * pinned API intentionally has no deletion primitive, while System
     * Settings alone needs deletion to restore pre-transaction policy absence.
     * Derive the documented canonical location from GLib's XDG authority here
     * rather than introducing a second persistence implementation.
     */
    path = g_build_filename(
        g_get_user_config_dir(), "infiltrator", "presentation.conf", NULL);
    if (path == NULL) {
        return false;
    }
    if (g_unlink(path) == 0 || errno == ENOENT) {
        return true;
    }

    g_warning("Unable to remove the temporal policy during transaction rollback: %s",
              g_strerror(errno));
    return false;
}

const SsTemporalPolicyStore *ss_platform_temporal_policy_store(void)
{
    static const SsTemporalPolicyStore store = {
        .load = platform_load,
        .save = platform_save,
        .clear = platform_clear,
        .begin_update = begin_update,
        .end_update = end_update,
        .compatibility_matches = platform_compatibility_matches
    };
    return &store;
}
