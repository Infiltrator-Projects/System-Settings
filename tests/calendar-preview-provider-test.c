// SPDX-License-Identifier: GPL-3.0-or-later
#include "calendar-preview-provider.h"

#include <glib.h>

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
    return EXIT_SUCCESS;
}
