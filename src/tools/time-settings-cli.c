// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/date-time-model.h"

#include <infiltratr/core.h>

#include <stdio.h>
#include <string.h>

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [--clock MODE] [--calendar ID] [--seconds on|off] "
            "[--location LAT LON|--clear-location]\n",
            program);
}

int main(int argc, char **argv)
{
    SsDateTimeModel model;
    int index;

    if (!ss_date_time_model_init(&model, ss_platform_temporal_policy_store())) {
        fputs("Unable to load temporal presentation policy.\n", stderr);
        return 1;
    }

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--clock") == 0 && index + 1 < argc) {
            if (!ss_date_time_model_set_clock_mode(&model, argv[++index])) {
                fputs("Unable to set clock mode.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--calendar") == 0 &&
                   index + 1 < argc) {
            if (!ss_date_time_model_set_calendar(&model, argv[++index])) {
                fputs("Unable to set calendar.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--seconds") == 0 &&
                   index + 1 < argc) {
            const char *value = argv[++index];
            bool show;
            if (strcmp(value, "on") == 0)
                show = true;
            else if (strcmp(value, "off") == 0)
                show = false;
            else {
                print_usage(argv[0]);
                return 2;
            }
            if (!ss_date_time_model_set_show_seconds(&model, show)) {
                fputs("Unable to set seconds policy.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--location") == 0 &&
                   index + 2 < argc) {
            const char *latitude_text = argv[++index];
            const char *longitude_text = argv[++index];
            double latitude;
            double longitude;

            if (!infiltratr_parse_double_range(
                    latitude_text, -90.0, 90.0, &latitude) ||
                !infiltratr_parse_double_range(
                    longitude_text, -180.0, 180.0, &longitude) ||
                !ss_date_time_model_set_location(
                    &model, true, latitude, longitude)) {
                fputs("Unable to set geographic location.\n", stderr);
                return 2;
            }
        } else if (strcmp(argv[index], "--clear-location") == 0) {
            if (!ss_date_time_model_set_location(
                    &model, false,
                    model.policy.latitude, model.policy.longitude)) {
                fputs("Unable to clear geographic location.\n", stderr);
                return 2;
            }
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    printf("clock-mode=%s\n"
           "calendar=%s\n"
           "show-seconds=%s\n"
           "location-configured=%s\n"
           "latitude=%.6f\n"
           "longitude=%.6f\n"
           "source=%s\n",
           model.policy.clock_mode,
           model.policy.calendar,
           model.policy.show_seconds ? "true" : "false",
           model.policy.location_configured ? "true" : "false",
           model.policy.latitude,
           model.policy.longitude,
           model.persisted_policy_present
               ? "infiltrator-policy" : "platform-default");
    return 0;
}
