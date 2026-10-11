// SPDX-License-Identifier: GPL-3.0-or-later
#include "home-temporal-provider.h"

#include <glib.h>

int main(void)
{
    SsHomeTemporalProvider *provider =
        ss_home_temporal_provider_new();

    g_assert_nonnull(provider);
    g_assert_cmpstr(
        ss_home_temporal_provider_page_id(provider),
        ==,
        "date-time");
    ss_home_temporal_provider_free(provider);
    return 0;
}
