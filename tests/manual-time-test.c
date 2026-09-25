// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/manual-time.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "Manual-time test failed: %s (%s:%d)\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

int main(void)
{
    char text[64];
    int64_t us = -1;

    CHECK(ss_manual_time_mode_supported("standard"));
    CHECK(ss_manual_time_mode_supported("standard-12"));
    CHECK(ss_manual_time_mode_supported("standard-24"));
    CHECK(ss_manual_time_mode_supported("decimal"));
    CHECK(!ss_manual_time_mode_supported("sidereal"));

    CHECK(ss_manual_time_parse(
        "standard-24", true, "18:57:18", &us));
    CHECK(us == INT64_C(68238000000));

    CHECK(ss_manual_time_parse(
        "standard-12", false, "6:57:18 PM", &us));
    CHECK(us == INT64_C(68238000000));
    CHECK(ss_manual_time_parse(
        "standard-12", false, "12:00 AM", &us));
    CHECK(us == 0);
    CHECK(ss_manual_time_parse(
        "standard-12", false, "12:00 PM", &us));
    CHECK(us == INT64_C(43200000000));

    CHECK(ss_manual_time_parse(
        "decimal", true, "5:00:00", &us));
    CHECK(us == INT64_C(43200000000));
    CHECK(ss_manual_time_parse(
        "decimal", true, "9:99:99", &us));
    CHECK(us == INT64_C(86399136000));
    CHECK(!ss_manual_time_parse(
        "decimal", true, "10:00:00", &us));

    CHECK(ss_manual_time_format(
        "decimal", true,
        INT64_C(43200000000), 0, true,
        text, sizeof(text)));
    CHECK(strcmp(text, "5:00:00") == 0);

    static const char *const invalid[] = {
        "", "1", ":20", "12:", "12:00:", "12:00:00junk", "+1:00", "-1:00",
        "9999999999999999999999999999:00", "1:999999999999999999", "1:00:9999999999999",
        "12:00\n", " 12:00", "12:00:60", "23:60", "24:00"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        us = -17;
        CHECK(!ss_manual_time_parse("standard-24", true, invalid[i], &us));
        CHECK(us == -17);
    }
    CHECK(!ss_manual_time_parse("standard-12", false, "12:00 PMjunk", &us));
    CHECK(!ss_manual_time_parse("standard-12", false, "0:00 AM", &us));
    CHECK(ss_manual_time_parse("standard-12", false, "1:02 pm", &us));
    CHECK(!ss_manual_time_parse("decimal", true, "1:100:00", &us));
    return 0;
}
