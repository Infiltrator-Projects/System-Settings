// SPDX-License-Identifier: GPL-3.0-or-later
#include "calendar-preview-provider.h"

#include <infiltratr/dynlib.h>

#include <glib-object.h>

#include <stddef.h>
#include <string.h>

#define CALENDAR_RUNTIME_SONAME "libcalendar-plus.so.0"
#define CALENDAR_ID_CAPACITY 64U

typedef int (*TimeModeFromStringFn)(const char *mode);
typedef char *(*FormatTimeAtLocationFn)(
    int mode,
    int64_t unix_microseconds,
    int utc_offset_seconds,
    int show_seconds,
    int vertical,
    double latitude,
    double longitude);
typedef void *(*CalendarSystemNewFn)(const char *calendar_id);
typedef char *(*CalendarSystemFormatDateFn)(
    void *calendar,
    int gregorian_year,
    int gregorian_month,
    int gregorian_day,
    const char *part);

struct SsCalendarPreviewProvider {
    InfiltratrDynlib library;
    TimeModeFromStringFn time_mode_from_string;
    FormatTimeAtLocationFn format_time_at_location;
    CalendarSystemNewFn calendar_system_new;
    CalendarSystemFormatDateFn calendar_system_format_date;
    void *calendar;
    char calendar_id[CALENDAR_ID_CAPACITY];
};

static bool bind_runtime(SsCalendarPreviewProvider *provider)
{
    InfiltratrDynlibBinding bindings[] = {
        {
            .name = "calendar_plus_time_mode_from_string",
            .destination = &provider->time_mode_from_string,
            .destination_size = sizeof(provider->time_mode_from_string),
            .required = true
        },
        {
            .name = "calendar_plus_format_time_at_location",
            .destination = &provider->format_time_at_location,
            .destination_size = sizeof(provider->format_time_at_location),
            .required = true
        },
        {
            .name = "calendar_plus_calendar_system_new",
            .destination = &provider->calendar_system_new,
            .destination_size = sizeof(provider->calendar_system_new),
            .required = true
        },
        {
            .name = "calendar_plus_calendar_system_format_date",
            .destination = &provider->calendar_system_format_date,
            .destination_size = sizeof(provider->calendar_system_format_date),
            .required = true
        }
    };

    return infiltratr_dynlib_bind_symbols(
        &provider->library,
        bindings,
        sizeof(bindings) / sizeof(bindings[0]));
}

SsCalendarPreviewProvider *ss_calendar_preview_provider_new_from(
    const char *library_name)
{
    SsCalendarPreviewProvider *provider;

    if (library_name == NULL || library_name[0] == '\0') {
        return NULL;
    }

    provider = g_new0(SsCalendarPreviewProvider, 1);
    provider->library = (InfiltratrDynlib)INFILTRATR_DYNLIB_INIT;

    if (!infiltratr_dynlib_open(&provider->library, library_name) ||
        !bind_runtime(provider)) {
        ss_calendar_preview_provider_free(provider);
        return NULL;
    }
    return provider;
}

SsCalendarPreviewProvider *ss_calendar_preview_provider_new(void)
{
    return ss_calendar_preview_provider_new_from(
        CALENDAR_RUNTIME_SONAME);
}

void ss_calendar_preview_provider_free(
    SsCalendarPreviewProvider *provider)
{
    if (provider == NULL) {
        return;
    }

    if (provider->calendar != NULL) {
        g_object_unref(provider->calendar);
        provider->calendar = NULL;
    }
    infiltratr_dynlib_close(&provider->library);
    g_free(provider);
}

bool ss_calendar_preview_provider_available(
    const SsCalendarPreviewProvider *provider)
{
    return provider != NULL &&
           infiltratr_dynlib_is_open(&provider->library) &&
           provider->time_mode_from_string != NULL &&
           provider->format_time_at_location != NULL &&
           provider->calendar_system_new != NULL &&
           provider->calendar_system_format_date != NULL;
}

char *ss_calendar_preview_provider_format_clock(
    SsCalendarPreviewProvider *provider,
    const char *clock_mode,
    int64_t unix_microseconds,
    int utc_offset_seconds,
    bool show_seconds,
    double latitude,
    double longitude)
{
    int mode;
    char *formatted;

    if (!ss_calendar_preview_provider_available(provider) ||
        clock_mode == NULL || clock_mode[0] == '\0') {
        return NULL;
    }

    mode = provider->time_mode_from_string(clock_mode);
    if (mode == 0) {
        return NULL;
    }

    formatted = provider->format_time_at_location(
        mode,
        unix_microseconds,
        utc_offset_seconds,
        show_seconds ? 1 : 0,
        0,
        latitude,
        longitude);
    if (formatted == NULL || formatted[0] == '\0') {
        g_free(formatted);
        return NULL;
    }
    return formatted;
}

static bool select_calendar(
    SsCalendarPreviewProvider *provider,
    const char *calendar_id)
{
    void *calendar;

    if (!ss_calendar_preview_provider_available(provider) ||
        calendar_id == NULL || calendar_id[0] == '\0' ||
        strlen(calendar_id) >= sizeof(provider->calendar_id)) {
        return false;
    }

    if (provider->calendar != NULL &&
        strcmp(provider->calendar_id, calendar_id) == 0) {
        return true;
    }

    calendar = provider->calendar_system_new(calendar_id);
    if (calendar == NULL) {
        return false;
    }

    if (provider->calendar != NULL) {
        g_object_unref(provider->calendar);
    }
    provider->calendar = calendar;
    g_strlcpy(
        provider->calendar_id,
        calendar_id,
        sizeof(provider->calendar_id));
    return true;
}

char *ss_calendar_preview_provider_format_date(
    SsCalendarPreviewProvider *provider,
    const char *calendar_id,
    int gregorian_year,
    int gregorian_month,
    int gregorian_day)
{
    char *formatted;

    if (!select_calendar(provider, calendar_id)) {
        return NULL;
    }

    formatted = provider->calendar_system_format_date(
        provider->calendar,
        gregorian_year,
        gregorian_month,
        gregorian_day,
        "full");
    if (formatted == NULL || formatted[0] == '\0') {
        g_free(formatted);
        return NULL;
    }
    return formatted;
}
