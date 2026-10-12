// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-settings/date-time-model.h"
#ifndef _WIN32
#include "system-settings/location-metadata.h"
#endif

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
    SsDateTimeModel model = SS_DATE_TIME_MODEL_INIT;
    SsDateTimePolicyDraft *draft = NULL;
    int index;
    int exit_code = 0;
    bool platform_locked = false;
    bool locality_locked = false;
    bool location_change_requested = false;
    bool location_clear_requested = false;
    bool output_persisted = false;
    InfiltratrTemporalPolicyV3 original_policy = {0};
    InfiltratrTemporalPolicyV3 output_policy = {0};
#ifndef _WIN32
    SsLocationMetadata location_candidate = {0};
    bool location_staged = false;
#endif
    const SsTemporalPolicyStore *platform =
        ss_platform_temporal_policy_store();

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

    {
        const InfiltratrTemporalPolicyV3 *authoritative =
            ss_date_time_model_policy(&model);
        if (authoritative == NULL) {
            fputs("Unable to read temporal presentation policy.\n", stderr);
            exit_code = 1;
            goto done;
        }
        original_policy = *authoritative;
        output_policy = *authoritative;
        output_persisted = ss_date_time_model_has_persisted_policy(&model);
        draft = ss_date_time_policy_draft_new(authoritative);
    }
    if (draft == NULL) {
        fputs("Unable to stage temporal presentation policy.\n", stderr);
        exit_code = 1;
        goto done;
    }

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--clock") == 0 && index + 1 < argc) {
            if (!ss_date_time_policy_draft_set_clock_mode(
                    draft, argv[++index])) {
                fputs("Unable to set clock mode.\n", stderr);
                exit_code = 2;
                goto done;
            }
        } else if (strcmp(argv[index], "--calendar") == 0 &&
                   index + 1 < argc) {
            if (!ss_date_time_policy_draft_set_calendar(
                    draft, argv[++index])) {
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
            if (!ss_date_time_policy_draft_set_show_seconds(draft, show)) {
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
                !ss_date_time_policy_draft_set_location(
                    draft, true, latitude, longitude)) {
                fputs("Unable to set geographic location.\n", stderr);
                exit_code = 2;
                goto done;
            }
            location_change_requested = true;
            location_clear_requested = false;
#ifndef _WIN32
            memset(&location_candidate, 0, sizeof(location_candidate));
            infiltratr_copy_string(
                location_candidate.display_name,
                sizeof(location_candidate.display_name),
                "Custom coordinates");
            location_candidate.latitude = latitude;
            location_candidate.longitude = longitude;
#endif
        } else if (strcmp(argv[index], "--clear-location") == 0) {
            const InfiltratrTemporalPolicyV3 *candidate =
                ss_date_time_policy_draft_policy(draft);
            if (candidate == NULL ||
                !ss_date_time_policy_draft_set_location(
                    draft,
                    false,
                    candidate->latitude,
                    candidate->longitude)) {
                fputs("Unable to clear geographic location.\n", stderr);
                exit_code = 2;
                goto done;
            }
            location_change_requested = true;
            location_clear_requested = true;
        } else {
            print_usage(argv[0]);
            exit_code = 2;
            goto done;
        }
    }

    if (argc > 1) {
        const InfiltratrTemporalPolicyV3 *candidate =
            ss_date_time_policy_draft_policy(draft);

        if (candidate == NULL) {
            fputs("Unable to read staged temporal presentation policy.\n", stderr);
            exit_code = 2;
            goto done;
        }
#ifndef _WIN32
        if (location_change_requested && !location_clear_requested) {
            if (!ss_location_metadata_stage(&location_candidate)) {
                fputs("Unable to stage locality metadata.\n", stderr);
                exit_code = 2;
                goto done;
            }
            location_staged = true;
        }
#endif

        if (platform == NULL || platform->save == NULL ||
            !platform->save(candidate)) {
#ifndef _WIN32
            if (location_staged) {
                ss_location_metadata_discard_staged();
                location_staged = false;
            }
#endif
            fputs("Unable to save temporal presentation policy.\n", stderr);
            exit_code = 2;
            goto done;
        }
        output_policy = *candidate;
        output_persisted = true;

#ifndef _WIN32
        if (location_change_requested) {
            const bool metadata_ok = location_clear_requested
                ? ss_location_metadata_clear()
                : ss_location_metadata_finish_staged();
            if (!metadata_ok) {
                const bool rolled_back =
                    platform->save(&original_policy);
                if (rolled_back && location_staged) {
                    ss_location_metadata_discard_staged();
                }
                /*
                 * If rollback failed, retain a staged locality journal so the
                 * next System Settings process can reconcile it against the
                 * policy that actually reached disk.
                 */
                fputs(
                    rolled_back
                        ? "Locality metadata could not be committed; the previous temporal policy was restored.\n"
                        : "Locality metadata could not be committed and policy rollback failed; recovery is required.\n",
                    stderr);
                exit_code = 3;
                goto done;
            }
            location_staged = false;
        }
#endif

        if (platform->compatibility_matches != NULL &&
            !platform->compatibility_matches(candidate)) {
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
    ss_date_time_policy_draft_free(draft);
    ss_date_time_model_clear(&model);
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
           output_policy.clock_mode,
           output_policy.calendar,
           output_policy.show_seconds ? "true" : "false",
           output_policy.location_configured ? "true" : "false",
           output_policy.latitude,
           output_policy.longitude,
           output_persisted ? "infiltrator-policy" : "platform-default");
    return 0;
}
