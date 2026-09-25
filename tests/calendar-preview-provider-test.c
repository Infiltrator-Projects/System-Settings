// SPDX-License-Identifier: GPL-3.0-or-later
#include "calendar-preview-provider.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, \
                    "Calendar preview provider test failed: %s (%s:%d)\\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

int main(int argc, char **argv)
{
    SsCalendarPreviewProvider *provider;
    char *clock_text;
    char *date_text;

    CHECK(argc == 2);

    provider = ss_calendar_preview_provider_new_from(argv[1]);
    CHECK(provider != NULL);
    CHECK(ss_calendar_preview_provider_available(provider));

    clock_text = ss_calendar_preview_provider_format_clock(
        provider,
        "internet",
        INT64_C(1789986798000000),
        36000,
        true,
        -36.4,
        145.35);
    CHECK(clock_text != NULL);
    CHECK(strcmp(clock_text, "@481") == 0);
    g_free(clock_text);

    date_text = ss_calendar_preview_provider_format_date(
        provider,
        "positivist",
        2026,
        9,
        21);
    CHECK(date_text != NULL);
    CHECK(strcmp(date_text, "13 Gutenberg 238") == 0);
    g_free(date_text);

    ss_calendar_preview_provider_free(provider);
    /* GLib retains static types after provider teardown. They must stay valid. */
    GType fixture_type = g_type_from_name("SsPreviewFixtureObject");
    CHECK(fixture_type != G_TYPE_INVALID);
    GObject *retained_type_object = g_object_new(fixture_type, NULL);
    CHECK(retained_type_object != NULL);
    g_object_unref(retained_type_object);

    {
        g_autofree gchar *root = NULL;
        g_autofree gchar *runtime_path = NULL;
        g_autoptr(GError) error = NULL;
        g_autoptr(GFile) source = NULL;
        g_autoptr(GFile) destination = NULL;

        root = g_dir_make_tmp(
            "system-settings-calendar-runtime-XXXXXX", &error);
        CHECK(root != NULL);
        provider =
            ss_calendar_preview_provider_new_with_root_for_test(root);
        CHECK(provider != NULL);

        clock_text = ss_calendar_preview_provider_format_clock(
            provider,
            "internet",
            INT64_C(1789986798000000),
            36000,
            true,
            -36.4,
            145.35);
        CHECK(clock_text == NULL);

        runtime_path = g_build_filename(
            root, "libcalendar-plus.so.0", NULL);
        source = g_file_new_for_path(argv[1]);
        destination = g_file_new_for_path(runtime_path);
        CHECK(g_file_copy(
            source,
            destination,
            G_FILE_COPY_OVERWRITE,
            NULL,
            NULL,
            NULL,
            &error));

        ss_calendar_preview_provider_force_retry_for_test(provider);
        clock_text = ss_calendar_preview_provider_format_clock(
            provider,
            "internet",
            INT64_C(1789986798000000),
            36000,
            true,
            -36.4,
            145.35);
        CHECK(clock_text != NULL);
        CHECK(strcmp(clock_text, "@481") == 0);
        g_free(clock_text);

        date_text = ss_calendar_preview_provider_format_date(
            provider,
            "positivist",
            2026,
            9,
            21);
        CHECK(date_text != NULL);
        CHECK(strcmp(date_text, "13 Gutenberg 238") == 0);
        g_free(date_text);

        ss_calendar_preview_provider_free(provider);
        CHECK(g_remove(runtime_path) == 0);
        CHECK(g_rmdir(root) == 0);
    }

    return EXIT_SUCCESS;
}
