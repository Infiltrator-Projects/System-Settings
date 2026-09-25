// SPDX-License-Identifier: GPL-3.0-or-later
#include "home-temporal-presentation.h"

#include <glib.h>
#include <infiltratr/core.h>
#include <string.h>

int main(void)
{
    InfiltratrTemporalPolicyV3 policy;
    SsHomeTemporalPresentation presentation = {0};
    g_autoptr(GDateTime) noon =
        g_date_time_new_from_unix_utc(INT64_C(43200));

    g_assert_nonnull(noon);
    g_assert_true(infiltratr_temporal_policy_v3_default(&policy));
    g_assert_true(infiltratr_copy_string(
        policy.clock_mode, sizeof(policy.clock_mode), "decimal"));
    g_assert_true(infiltratr_copy_string(
        policy.calendar, sizeof(policy.calendar), "gregorian"));
    policy.show_seconds = true;

    g_assert_true(ss_home_temporal_presentation_format(
        &policy, noon, true, &presentation));
    g_assert_cmpstr(presentation.clock_text, ==, "5:00:00");
    g_assert_nonnull(presentation.date_text);
    g_assert_nonnull(strstr(
        presentation.system_time_text, "5:00:00"));
    ss_home_temporal_presentation_clear(&presentation);

    g_assert_true(infiltratr_copy_string(
        policy.clock_mode, sizeof(policy.clock_mode), "standard-24"));
    policy.show_seconds = false;
    g_assert_true(ss_home_temporal_presentation_format(
        &policy, noon, false, &presentation));
    g_assert_cmpstr(presentation.clock_text, ==, "12:00");
    g_assert_null(strstr(presentation.clock_text, ":00:"));
    ss_home_temporal_presentation_clear(&presentation);

    return 0;
}
