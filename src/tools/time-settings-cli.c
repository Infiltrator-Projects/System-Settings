// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/date-time-model.h"

#include <stdio.h>
#include <string.h>

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [--profile system|conventional-12|conventional-24|decimal-10] [--seconds on|off]\n",
            program);
}

int main(int argc, char **argv)
{
    SsDateTimeModel model;
    int index;

    if (!ss_date_time_model_init(&model)) {
        fputs("Unable to load temporal presentation policy.\n", stderr);
        return 1;
    }

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--profile") == 0 && index + 1 < argc) {
            InfiltratrClockProfile profile;
            if (!infiltratr_clock_profile_from_id(argv[++index], &profile) ||
                !ss_date_time_model_set_clock_profile(&model, profile)) {
                fputs("Unable to set clock profile.\n", stderr);
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
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    printf("clock-profile=%s\nshow-seconds=%s\nsource=%s\n",
           infiltratr_clock_profile_id(model.policy.clock_profile),
           model.policy.show_seconds ? "true" : "false",
           model.persisted_policy_present ? "infiltrator-policy" : "platform-default");
    return 0;
}
