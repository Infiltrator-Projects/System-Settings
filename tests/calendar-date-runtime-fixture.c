// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Synthetic provider for the executable system Calendar Resources ABI.
 * This fixture tests discovery/invocation, not calendar algorithms.
 */
#include <infiltratr/temporal_calendar.h>

#include <string.h>

enum { TEST_JDN = 2461305 };

static bool test_fields(
    const InfiltratrTemporalCalendarProvider *provider,
    int64_t jdn, InfiltratrTemporalCalendarFields *fields)
{
    (void)provider;
    if (jdn != TEST_JDN || fields == NULL) return false;
    *fields = (InfiltratrTemporalCalendarFields){
        238, 9, 13, 0, false
    };
    return true;
}

static bool test_format(
    const InfiltratrTemporalCalendarProvider *provider,
    int64_t jdn, InfiltratrTemporalCalendarDatePart part,
    char *buffer, size_t capacity, size_t *length)
{
    static const char value[] = "13 Gutenberg 238";
    (void)provider;
    if (jdn != TEST_JDN ||
        part != INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_FULL ||
        buffer == NULL || capacity < sizeof(value)) return false;
    memcpy(buffer, value, sizeof(value));
    if (length != NULL) *length = sizeof(value) - 1U;
    return true;
}

static bool test_period_start(
    const InfiltratrTemporalCalendarProvider *provider,
    int64_t jdn, int64_t *result)
{
    (void)provider;
    if (result == NULL || jdn != TEST_JDN) return false;
    *result = TEST_JDN;
    return true;
}

static bool test_add_periods(
    const InfiltratrTemporalCalendarProvider *provider,
    int64_t jdn, int32_t amount, int64_t *result)
{
    (void)provider;
    if (result == NULL || jdn != TEST_JDN) return false;
    *result = jdn + (int64_t)amount * INT64_C(30);
    return true;
}

static bool test_add_years(
    const InfiltratrTemporalCalendarProvider *provider,
    int64_t jdn, int32_t amount, int64_t *result)
{
    (void)provider;
    if (result == NULL || jdn != TEST_JDN) return false;
    *result = jdn + (int64_t)amount * INT64_C(365);
    return true;
}

static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CALENDAR,
    "positivist", "Synthetic Positivist Fixture", false, false, false
};
static const InfiltratrTemporalCalendarProvider provider = {
    sizeof(InfiltratrTemporalCalendarProvider),
    INFILTRATR_TEMPORAL_CALENDAR_PROVIDER_ABI,
    "positivist",
    test_fields, test_format, test_period_start,
    test_add_periods, test_add_years
};

#ifdef _WIN32
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif

FIXTURE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void);
FIXTURE_EXPORT const InfiltratrTemporalCalendarProvider *
infiltratr_temporal_calendar_provider_v1(void);

FIXTURE_EXPORT const InfiltratrTemporalResourceMetadata *
infiltratr_temporal_resource_metadata_v1(void)
{
    return &metadata;
}

FIXTURE_EXPORT const InfiltratrTemporalCalendarProvider *
infiltratr_temporal_calendar_provider_v1(void)
{
    return &provider;
}
