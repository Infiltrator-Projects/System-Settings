// SPDX-License-Identifier: GPL-3.0-or-later
#include <infiltratr/temporal_posix.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, \
                    "Calendar date provider integration failed: %s (%s:%d)\n", \
                    #expression, __FILE__, __LINE__); \
            return EXIT_FAILURE; \
        } \
    } while (0)

int main(int argc, char **argv)
{
    InfiltratrTemporalDateProvider *provider;
    char date_text[INFILTRATR_TEMPORAL_DATE_CAPACITY] = "unchanged";
    size_t length = 123U;

    CHECK(argc == 2);
    CHECK(infiltratr_temporal_posix_date_provider_new_from("relative.so") == NULL);

    provider = infiltratr_temporal_posix_date_provider_new_from(argv[1]);
    CHECK(provider != NULL);
    CHECK(infiltratr_temporal_posix_format_date(
        provider,
        "positivist",
        2026,
        9,
        21,
        "full",
        date_text,
        sizeof(date_text),
        &length));
    CHECK(strcmp(date_text, "13 Gutenberg 238") == 0);
    CHECK(length == strlen(date_text));

    strcpy(date_text, "unchanged");
    length = 123U;
    CHECK(!infiltratr_temporal_posix_format_date(
        provider,
        "unknown-calendar",
        2026,
        9,
        21,
        "full",
        date_text,
        sizeof(date_text),
        &length));
    CHECK(strcmp(date_text, "unchanged") == 0);
    CHECK(length == 123U);

    infiltratr_temporal_posix_date_provider_free(provider);
    return EXIT_SUCCESS;
}
