// SPDX-License-Identifier: GPL-3.0-or-later
#include "builtin-module-host.h"
#include "home-temporal-provider.h"

#include <glib.h>
#include <stdbool.h>

int main(void)
{
    SsHomeTemporalProvider *provider =
        ss_home_temporal_provider_new();
    const char *target;
    bool registered = false;

    g_assert_nonnull(provider);
    target = ss_home_temporal_provider_page_id(provider);
    g_assert_nonnull(target);

    for (size_t index = 0U;
         index < ss_builtin_module_host_module_count();
         ++index) {
        if (g_strcmp0(
                target,
                ss_builtin_module_host_module_page_id(index)) == 0) {
            registered = true;
            break;
        }
    }

    g_assert_true(registered);
    ss_home_temporal_provider_free(provider);
    return 0;
}
