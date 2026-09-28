// SPDX-License-Identifier: GPL-3.0-or-later
#include "home-temporal-presentation.h"
#include "system-settings/temporal-policy-store.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <infiltratr/core.h>
#include <string.h>

int main(void)
{
    g_autofree gchar *root =
        g_dir_make_tmp("ss-home-temporal-XXXXXX", NULL);
    g_assert_nonnull(root);
    g_setenv("XDG_CONFIG_HOME", root, TRUE);
    g_setenv("GSETTINGS_BACKEND", "memory", TRUE);

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

    /*
     * A transient policy read failure must preserve the last valid policy
     * rather than freezing Home at its last rendered text.
     */
    {
        const SsTemporalPolicyStore *store =
            ss_platform_temporal_policy_store();
        SsHomeTemporalPresenter *presenter;
        SsHomeTemporalPresentation first;
        SsHomeTemporalPresentation second;
        g_autofree gchar *policy_path = g_build_filename(
            root, "infiltrator", "presentation.conf", NULL);

        g_assert_nonnull(store);
        g_assert_true(infiltratr_temporal_policy_v3_default(&policy));
        infiltratr_copy_string(
            policy.clock_mode, sizeof(policy.clock_mode), "standard-24");
        policy.show_seconds = true;
        g_assert_true(store->save(&policy));

        presenter = ss_home_temporal_presenter_new();
        g_assert_nonnull(presenter);
        g_assert_cmpuint(
            ss_home_temporal_presenter_refresh_interval_ms(presenter),
            ==,
            250U);
        ss_home_temporal_presentation_init(&first);
        ss_home_temporal_presentation_init(&second);
        g_assert_true(ss_home_temporal_presenter_format_now(
            presenter, &first));

        g_assert_cmpint(g_chmod(policy_path, 0000), ==, 0);
        for (unsigned int i = 0U; i < 100U; ++i) {
            while (g_main_context_iteration(NULL, FALSE)) {
            }
            g_usleep(1000U);
        }

        g_assert_true(ss_home_temporal_presenter_format_now(
            presenter, &second));
        g_assert_nonnull(second.clock_text);
        g_assert_cmpint(g_chmod(policy_path, 0600), ==, 0);

        ss_home_temporal_presentation_clear(&first);
        ss_home_temporal_presentation_clear(&second);
        ss_home_temporal_presenter_free(presenter);

        policy.show_seconds = false;
        g_assert_true(store->save(&policy));
        presenter = ss_home_temporal_presenter_new();
        g_assert_nonnull(presenter);
        g_assert_cmpuint(
            ss_home_temporal_presenter_refresh_interval_ms(presenter),
            ==,
            1000U);
        ss_home_temporal_presenter_free(presenter);
    }

    g_autofree gchar *policy_file = g_build_filename(
        root, "infiltrator", "presentation.conf", NULL);
    g_autofree gchar *policy_dir = g_path_get_dirname(policy_file);
    g_assert_cmpint(g_remove(policy_file), ==, 0);
    g_assert_cmpint(g_rmdir(policy_dir), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    return 0;
}
