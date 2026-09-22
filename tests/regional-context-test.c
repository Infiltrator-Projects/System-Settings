// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/regional-context.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "Regional context test failed: %s (%s:%d)\n", \
                    #expression, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

static double absolute_difference(double left, double right)
{
    double value = left - right;
    return value < 0.0 ? -value : value;
}

int main(void)
{
    static const char fixture[] =
        "# country\tcoordinates\tTZ\tcomments\n"
        "AU\t-3749+14458\tAustralia/Melbourne\tVictoria\n"
        "US\t+404251-0740023\tAmerica/New_York\tEastern\n";
    char path[] = "/tmp/system-settings-zone-tab-XXXXXX";
    char city[SS_TIMEZONE_CITY_CAPACITY];
    double latitude = 0.0;
    double longitude = 0.0;
    int fd = g_mkstemp(path);

    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(g_file_set_contents(path, fixture, -1, NULL));

    CHECK(ss_regional_context_lookup_timezone_reference(
        path, "Australia/Melbourne", &latitude, &longitude));
    CHECK(absolute_difference(latitude, -37.8166666667) < 0.000001);
    CHECK(absolute_difference(longitude, 144.9666666667) < 0.000001);

    CHECK(ss_regional_context_lookup_timezone_reference(
        path, "America/New_York", &latitude, &longitude));
    CHECK(absolute_difference(latitude, 40.7141666667) < 0.000001);
    CHECK(absolute_difference(longitude, -74.0063888889) < 0.000001);

    CHECK(!ss_regional_context_lookup_timezone_reference(
        path, "Australia/Nowhere", &latitude, &longitude));

    CHECK(ss_regional_context_city_name(
        "America/Port_of_Spain", city, sizeof(city)));
    CHECK(strcmp(city, "Port of Spain") == 0);
    CHECK(!ss_regional_context_city_name(
        "../bad", city, sizeof(city)));

    {
        g_autoptr(GPtrArray) zones =
            ss_regional_context_list_timezones("US/Eastern");
        bool melbourne_found = false;
        bool current_alias_found = false;
        size_t index;

        CHECK(zones != NULL);
        CHECK(zones->len > 0U);
        for (index = 0U; index < zones->len; ++index) {
            const char *zone =
                g_ptr_array_index(zones, (guint)index);
            if (strcmp(zone, "Australia/Melbourne") == 0) {
                melbourne_found = true;
            }
            if (strcmp(zone, "US/Eastern") == 0) {
                current_alias_found = true;
            }
        }
        CHECK(melbourne_found);
        CHECK(current_alias_found);
    }

    {
        char nearest[SS_TIMEZONE_ID_CAPACITY] = {0};

        CHECK(ss_regional_context_nearest_timezone(
            "AU", -36.3949, 145.3610,
            nearest, sizeof(nearest)));
        CHECK(strcmp(nearest, "Australia/Melbourne") == 0);
    }

    CHECK(g_remove(path) == 0);
    return 0;
}
