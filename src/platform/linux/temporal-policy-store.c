// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal-policy-store.c
 * @brief POSIX temporal-policy persistence plus Cinnamon compatibility seed.
 */
#include "system-settings/temporal-policy-store.h"
#include "system-settings/cinnamon-interface.h"

#include <infiltratr/core.h>
#include <infiltratr/temporal_posix.h>

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

static void mirror_cinnamon_compatibility(
    const InfiltratrTemporalPolicyV3 *policy)
{
    g_autoptr(GSettings) settings = ss_cinnamon_interface_settings_new();
    gboolean ok = TRUE;

    if (policy == NULL || settings == NULL)
        return;

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
    mirror_cinnamon_compatibility(policy);
    return true;
}

const SsTemporalPolicyStore *ss_platform_temporal_policy_store(void)
{
    static const SsTemporalPolicyStore store = {
        .load = platform_load,
        .save = platform_save
    };
    return &store;
}
