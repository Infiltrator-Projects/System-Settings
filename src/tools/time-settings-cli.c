// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/date-time-model.h"
#ifndef _WIN32
#include "system-settings/location-metadata.h"
#endif

#include <infiltratr/core.h>

#include <stdio.h>
#include <string.h>

/* A command is one transaction: validation must not publish early options
 * when a later option is malformed. The staging store owns no external state. */
static bool stage_load(InfiltratrTemporalPolicyV3 *policy, bool *found)
{
    *found = false;
    return infiltratr_temporal_policy_v3_default(policy);
}

static bool stage_save(const InfiltratrTemporalPolicyV3 *policy)
{
    return policy != NULL;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [--clock MODE] [--calendar ID] [--seconds on|off] "
            "[--location LAT LON|--clear-location]\n",
            program);
}

static void release_platform_update(
    const SsTemporalPolicyStore *platform,
    bool *locked)
{
    if (platform != NULL && locked != NULL && *locked &&
        platform->end_update != NULL) {
        platform->end_update();
        *locked = false;
    }
}

static bool acquire_locality_transaction(void)
{
#ifdef _WIN32
    return true;
#else
    return ss_location_metadata_transaction_begin();
#endif
}

static void release_locality_transaction(void)
{
#ifndef _WIN32
    ss_location_metadata_transaction_end();
#endif
}

static bool command_touches_location(int argc, char **argv)
{
    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--location") == 0 ||
            strcmp(argv[index], "--clear-location") == 0) {
            return true;
        }
    }
    return false;
}

int main(int argc, char **argv)
{
    SsDateTimeModel model;
    int index;
    int exit_code = 0;
    bool platform_locked = false;
    bool locality_locked = false;
    const SsTemporalPolicyStore *platform =
        ss_platform_temporal_policy_store();
    static const SsTemporalPolicyStore staging = {
        .load = stage_load,
        .save = stage_save,
        .begin_update = NULL,
        .end_update = NULL,
        .compatibility_matches = NULL
    };

    /*
     * Location-changing commands take the locality lock before the policy
     * lock, matching the Linux GUI's lock order. This serializes CLI
     * coordinates with the metadata journal/rollback transaction there.
     * Windows has no separate locality metadata journal, so the helper is a
     * deliberate no-op on that platform.
     */
    if (argc > 1 && command_touches_location(argc, argv)) {
        if (!acquire_locality_transaction()) {
            fputs("Unable to acquire the locality transaction lock.\n", stderr);
            return 2;
        }
        locality_locked = true;
    }

    /*
     * Treat a multi-option CLI invocation as one cross-process transaction.
     * Hold the platform lock through validation/publication so simultaneous
     * GUI/CLI writers cannot overwrite a fresh complete policy.
     */
    if (argc > 1 && platform != NULL &&
        platform->begin_update != NULL) {
        if (platform->end_update == NULL ||
            !platform->begin_update()) {
            fputs("Unable to acquire the temporal policy update lock.\n", stderr);
            if (locality_locked) {
                release_locality_transaction();
            }
            return 2;
        }
        platform_locked = true;
    }

    if (!ss_date_time_model_init(&model, platform)) {
        fputs("Unable to load temporal presentation policy.\n", stderr);
        release_platform_update(platform, &platform_locked);
        if (locality_locked) {
            release_locality_transaction();
        }
        return 1;
    }

    model.store = &staging;
    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--clock") == 0 && index + 1 < argc) {
            if (!ss_date_time_model_set_clock_mode(&model, argv[++index])) {
                fputs("Unable to set clock mode.\n", stderr);
                exit_code = 2;
                goto done;
            }
        } else if (strcmp(argv[index], "--calendar") == 0 &&
                   index + 1 < argc) {
            if (!ss_date_time_model_set_calendar(&model, argv[++index])) {
                fputs("Unable to set calendar.\n", stderr);
                exit_code = 2;
                goto done;
            }
        } else if (strcmp(argv[index], "--seconds") == 0 &&
                   index + 1 < argc) {
            const char *value = argv[++index];
            bool show;
            if (strcmp(value, "on") == 0) {
                show = true;
            } else if (strcmp(value, "off") == 0) {
                show = false;
            } else {
                print_usage(argv[0]);
                exit_code = 2;
                goto done;
            }
            if (!ss_date_time_model_set_show_seconds(&model, show)) {
                fputs("Unable to set seconds policy.\n", stderr);
                exit_code = 2;
                goto done;
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
                exit_code = 2;
                goto done;
            }
        } else if (strcmp(argv[index], "--clear-location") == 0) {
            if (!ss_date_time_model_set_location(
                    &model, false,
                    model.policy.latitude, model.policy.longitude)) {
                fputs("Unable to clear geographic location.\n", stderr);
                exit_code = 2;
                goto done;
            }
        } else {
            print_usage(argv[0]);
            exit_code = 2;
            goto done;
        }
    }

    if (argc > 1) {
        if (platform == NULL || platform->save == NULL ||
            !platform->save(&model.policy)) {
            fputs("Unable to save temporal presentation policy.\n", stderr);
            exit_code = 2;
            goto done;
        }
        model.persisted_policy_present = true;
        if (platform->compatibility_matches != NULL &&
            !platform->compatibility_matches(&model.policy)) {
            fputs(
                "Temporal policy was saved, but native desktop compatibility settings did not fully synchronize.\n",
                stderr);
            exit_code = 3;
            goto done;
        }
    }

done:
    release_platform_update(platform, &platform_locked);
    if (locality_locked) {
        release_locality_transaction();
        locality_locked = false;
    }
    if (exit_code != 0) {
        return exit_code;
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
