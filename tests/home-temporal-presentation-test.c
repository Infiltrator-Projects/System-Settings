// SPDX-License-Identifier: GPL-3.0-or-later
#include "home-temporal-presentation.h"
#include "system-settings/temporal-policy-store.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <infiltratr/core.h>
#include <infiltratr/temporal_clock_resource.h>
#include <string.h>

static guint count_character(const char *text, char needle)
{
    guint count = 0U;

    if (text == NULL) {
        return 0U;
    }
    for (; *text != '\0'; ++text) {
        if (*text == needle) {
            ++count;
        }
    }
    return count;
}

static void assert_clock_text(const SsHomeTemporalPresentation *value,
    const char *id,const char *expected)
{
    InfiltratrTemporalClockModule *module=infiltratr_temporal_clock_module_open_id(id);
    if (module != NULL) g_assert_cmpstr(value->clock_text,==,expected);
    else g_assert_nonnull(strstr(value->clock_text,"resource unavailable"));
    infiltratr_temporal_clock_module_free(module);
}

int main(void)
{
    g_autofree gchar *root =
        g_dir_make_tmp("ss-home-temporal-XXXXXX", NULL);
    g_assert_nonnull(root);
    g_setenv("XDG_CONFIG_HOME", root, TRUE);
    g_setenv("GSETTINGS_BACKEND", "memory", TRUE);
    g_setenv("LANGUAGE", "en_AU", TRUE);

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
        InfiltratrTemporalClockModule *standard =
            infiltratr_temporal_clock_module_open_id("standard-24");
        if (standard != NULL) {
            g_assert_cmpstr(raw_output.clock_text, ==, "12:00");
        } else {
            g_assert_nonnull(strstr(raw_output.clock_text, "resource unavailable"));
        }
        infiltratr_temporal_clock_module_free(standard);
        ss_home_temporal_presentation_clear(&raw_output);
    }

    infiltratr_copy_string(
        policy.clock_mode, sizeof(policy.clock_mode), "decimal");
    infiltratr_copy_string(
        policy.calendar, sizeof(policy.calendar), "gregorian");
    policy.show_seconds = true;

    {
        InfiltratrTemporalClockModule *installed =
            infiltratr_temporal_clock_module_open_id("decimal");
        g_assert_true(ss_home_temporal_presentation_format(
            &policy, noon, true, &presentation));
        if (installed != NULL) {
            g_assert_cmpstr(presentation.clock_text, ==, "5:00:00");
            g_assert_nonnull(strstr(
                presentation.system_time_text, "5:00:00"));
        } else {
            g_assert_nonnull(strstr(
                presentation.clock_text, "resource unavailable"));
        }
        g_assert_nonnull(presentation.date_text);
        ss_home_temporal_presentation_clear(&presentation);
        infiltratr_temporal_clock_module_free(installed);
    }

    /*
     * Native Julian/MJD are mandatory, not preferred. When absent, display
     * resource-unavailable rather than using a duplicate built-in formatter.
     */
    for (gsize i = 0; i < 2U; ++i) {
        const char *id = i == 0U ? "julian" : "modified-julian";
        char expected[192] = {0};
        size_t length = 0U;
        InfiltratrTemporalClockModule *installed =
            infiltratr_temporal_clock_module_open_id(id);
        infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), id);
        g_assert_true(ss_home_temporal_presentation_format(
            &policy, noon, true, &presentation));
        if (installed != NULL) {
            g_assert_true(infiltratr_temporal_clock_format(
                installed, "en_AU", INT64_C(43200000000), 0, true, false,
                0.0, 0.0, expected, sizeof(expected), &length));
        } else {
            g_assert_nonnull(strstr(presentation.clock_text,
                                    "resource unavailable"));
        }
        if (installed != NULL)
            g_assert_cmpstr(presentation.clock_text, ==, expected);
        ss_home_temporal_presentation_clear(&presentation);
        infiltratr_temporal_clock_module_free(installed);
    }

    infiltratr_copy_string(
        policy.clock_mode, sizeof(policy.clock_mode), "standard-24");
    policy.show_seconds = false;
    g_assert_true(ss_home_temporal_presentation_format(
        &policy, noon, false, &presentation));
    {
        InfiltratrTemporalClockModule *standard =
            infiltratr_temporal_clock_module_open_id("standard-24");
        if (standard != NULL) {
            g_assert_cmpstr(presentation.clock_text, ==, "12:00");
            g_assert_null(strstr(presentation.clock_text, ":00:"));
        } else {
            g_assert_nonnull(strstr(presentation.clock_text,
                                    "resource unavailable"));
        }
        infiltratr_temporal_clock_module_free(standard);
    }
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
        /*
         * Home must never inherit a precision clock's second-level cadence.
         * The detailed Date & Time surface owns that presentation.
         */
        g_assert_cmpuint(
            ss_home_temporal_presenter_refresh_interval_ms(presenter),
            ==,
            60000U);
        ss_home_temporal_presentation_init(&first);
        ss_home_temporal_presentation_init(&second);
        g_assert_true(ss_home_temporal_presenter_format_now(
            presenter, &first));
        {
            InfiltratrTemporalClockModule *standard =
                infiltratr_temporal_clock_module_open_id("standard-24");
            if (standard != NULL) {
                g_assert_cmpuint(count_character(first.clock_text, ':'), ==, 1U);
            } else {
                g_assert_nonnull(strstr(first.clock_text,
                                        "resource unavailable"));
            }
            infiltratr_temporal_clock_module_free(standard);
        }

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

        infiltratr_copy_string(
            policy.clock_mode, sizeof(policy.clock_mode), "decimal");
        policy.show_seconds = true;
        g_assert_true(store->save(&policy));
        presenter = ss_home_temporal_presenter_new();
        g_assert_nonnull(presenter);
        g_assert_cmpuint(
            ss_home_temporal_presenter_refresh_interval_ms(presenter),
            ==,
            60000U);
        {
            SsHomeTemporalPresentation coarse;
            ss_home_temporal_presentation_init(&coarse);
            InfiltratrTemporalClockModule *installed =
                infiltratr_temporal_clock_module_open_id("decimal");
            g_assert_true(ss_home_temporal_presenter_format_now(
                presenter, &coarse));
            if (installed != NULL) {
                g_assert_cmpuint(
                    count_character(coarse.clock_text, ':'), ==, 1U);
            } else {
                g_assert_nonnull(strstr(
                    coarse.clock_text, "resource unavailable"));
            }
            ss_home_temporal_presentation_clear(&coarse);
            infiltratr_temporal_clock_module_free(installed);
        }
        ss_home_temporal_presenter_free(presenter);

        policy.show_seconds = false;
        g_assert_true(store->save(&policy));
        presenter = ss_home_temporal_presenter_new();
        g_assert_nonnull(presenter);
        g_assert_cmpuint(
            ss_home_temporal_presenter_refresh_interval_ms(presenter),
            ==,
            60000U);
        ss_home_temporal_presenter_free(presenter);
        g_assert_cmpuint(
            ss_home_temporal_presenter_refresh_interval_ms(NULL),
            ==,
            60000U);
    }

    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "chinese-time");
    policy.show_seconds = true;
    g_assert_true(ss_home_temporal_presentation_format(&policy, noon, true, &presentation));
    assert_clock_text(&presentation,"chinese-time","Wǔ, second half");
    ss_home_temporal_presentation_clear(&presentation);
    g_setenv("LANGUAGE", "zh_TW", TRUE);
    g_assert_true(ss_home_temporal_presentation_format(&policy, noon, true, &presentation));
    assert_clock_text(&presentation,"chinese-time","午正");
    ss_home_temporal_presentation_clear(&presentation);
    infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), "standard-12");
    policy.show_seconds = false;
    g_assert_true(ss_home_temporal_presentation_format(&policy, noon, true, &presentation));
    {
        InfiltratrTemporalClockModule *standard =
            infiltratr_temporal_clock_module_open_id("standard-12");
        if (standard != NULL)
            g_assert_cmpstr(presentation.clock_text, ==, "下午12:00");
        else
            g_assert_nonnull(strstr(presentation.clock_text,
                                    "resource unavailable"));
        infiltratr_temporal_clock_module_free(standard);
    }
    ss_home_temporal_presentation_clear(&presentation);
    g_setenv("LANGUAGE", "ja_JP", TRUE);
    g_assert_true(ss_home_temporal_presentation_format(&policy, noon, true, &presentation));
    {
        InfiltratrTemporalClockModule *standard =
            infiltratr_temporal_clock_module_open_id("standard-12");
        if (standard != NULL)
            g_assert_cmpstr(presentation.clock_text, ==, "午後12:00");
        else
            g_assert_nonnull(strstr(presentation.clock_text,
                                    "resource unavailable"));
        infiltratr_temporal_clock_module_free(standard);
    }
    ss_home_temporal_presentation_clear(&presentation);
    g_setenv("LANGUAGE", "en_AU", TRUE);
    {
        const char *modes[] = { "japanese-temporal", "japanese-temporal-early" };
        g_autoptr(GDateTime) j2000 = g_date_time_new_utc(2000, 1, 1, 12, 0, 0.0);
        policy.location_configured = true;
        policy.latitude = 0.0;
        policy.longitude = 0.0;
        for (gsize i = 0; i < G_N_ELEMENTS(modes); i++) {
            infiltratr_copy_string(policy.clock_mode, sizeof(policy.clock_mode), modes[i]);
            g_assert_true(ss_home_temporal_presentation_format(&policy, j2000, true, &presentation));
            assert_clock_text(&presentation,modes[i],"Mi · 四 bells");
            ss_home_temporal_presentation_clear(&presentation);
        }
    }

    g_autofree gchar *policy_file = g_build_filename(
        root, "infiltrator", "presentation.conf", NULL);
    g_autofree gchar *policy_dir = g_path_get_dirname(policy_file);
    g_assert_cmpint(g_remove(policy_file), ==, 0);
    g_assert_cmpint(g_rmdir(policy_dir), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    return 0;
}
