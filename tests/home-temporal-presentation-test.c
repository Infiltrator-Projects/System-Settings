// SPDX-License-Identifier: GPL-3.0-or-later
#include "home-temporal-presentation.h"

#include <glib.h>
#include <infiltratr/core.h>
#include <string.h>

int main(void)
{
    InfiltratrTemporalPolicyV3 policy;
    SsHomeTemporalPresentation presentation;
    g_autoptr(GDateTime) noon =
        g_date_time_new_from_unix_utc(INT64_C(43200));

    g_assert_nonnull(noon);
    ss_home_temporal_presentation_init(&presentation);
    g_assert_true(infiltratr_temporal_policy_v3_default(&policy));

    /*
     * The presentation result is an output object, not an in/out object.
     * Deliberately poison the stack to prove the formatter never frees or
     * otherwise inspects incoming pointer values.
     */
    {
        SsHomeTemporalPresentation raw_output;
        memset(&raw_output, 0xA5, sizeof(raw_output));
        infiltratr_copy_string(
            policy.clock_mode, sizeof(policy.clock_mode), "standard-24");
        infiltratr_copy_string(
            policy.calendar, sizeof(policy.calendar), "gregorian");
        policy.show_seconds = false;
        g_assert_true(ss_home_temporal_presentation_format(
            &policy, noon, true, &raw_output));
        g_assert_cmpstr(raw_output.clock_text, ==, "12:00");
        ss_home_temporal_presentation_clear(&raw_output);
    }

    infiltratr_copy_string(
        policy.clock_mode, sizeof(policy.clock_mode), "decimal");
    infiltratr_copy_string(
        policy.calendar, sizeof(policy.calendar), "gregorian");
    policy.show_seconds = true;

    g_assert_true(ss_home_temporal_presentation_format(
        &policy, noon, true, &presentation));
    g_assert_cmpstr(presentation.clock_text, ==, "5:00:00");
    g_assert_nonnull(presentation.date_text);
    g_assert_nonnull(strstr(
        presentation.system_time_text, "5:00:00"));
    ss_home_temporal_presentation_clear(&presentation);

    infiltratr_copy_string(
        policy.clock_mode, sizeof(policy.clock_mode), "standard-24");
    policy.show_seconds = false;
    g_assert_true(ss_home_temporal_presentation_format(
        &policy, noon, false, &presentation));
    g_assert_cmpstr(presentation.clock_text, ==, "12:00");
    g_assert_null(strstr(presentation.clock_text, ":00:"));
    ss_home_temporal_presentation_clear(&presentation);

    return 0;
}
