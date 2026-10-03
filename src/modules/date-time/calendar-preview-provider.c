// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file calendar-preview-provider.c
 * @brief Lazy dynamic discovery for Calendar-owned date previews.
 *
 * Common is the authoritative clock renderer. This bridge exists only for
 * calendar systems whose date formatter is supplied by the optional Calendar
 * runtime, and degrades cleanly when that runtime is not installed.
 */
#define _GNU_SOURCE
#include "calendar-preview-provider.h"
#include <dlfcn.h>

#include <infiltratr/dynlib.h>

#include <glib.h>
#include <glib-object.h>

#include <stddef.h>
#include <string.h>

#define CALENDAR_RUNTIME_SONAME "libcalendar-plus.so.0"
#define CALENDAR_RUNTIME_REALNAME "libcalendar-plus.so.0.0.0"
#define CALENDAR_ID_CAPACITY 64U
#define DISCOVERY_RETRY_INITIAL_USEC (5 * G_USEC_PER_SEC)
#define DISCOVERY_RETRY_MAX_USEC (60 * G_USEC_PER_SEC)

#ifndef SYSTEM_SETTINGS_LIBRARY_ARCHITECTURE
#define SYSTEM_SETTINGS_LIBRARY_ARCHITECTURE ""
#endif

typedef GObject *(*CalendarSystemNewFn)(const char *calendar_id);
typedef char *(*CalendarSystemFormatDateFn)(
    GObject *calendar,
    int gregorian_year,
    int gregorian_month,
    int gregorian_day,
    const char *part);

/*
 * Function pointers are meaningful only while library is open. calendar is a
 * cached instance for calendar_id and must be dropped before the library is
 * closed. Failed lazy discovery is exponentially throttled so a missing
 * optional runtime cannot turn a live preview timer into repeated filesystem
 * scans on the GTK thread.
 */
struct SsCalendarPreviewProvider {
    InfiltratrDynlib library;
    CalendarSystemNewFn calendar_system_new;
    CalendarSystemFormatDateFn calendar_system_format_date;
    GObject *calendar;
    char calendar_id[CALENDAR_ID_CAPACITY];
    bool discover_default_runtime;
    char *discovery_root_override;
    gint64 retry_after_monotonic_us;
    gint64 retry_delay_us;
};

static void reset_bindings(SsCalendarPreviewProvider *provider)
{
    provider->calendar_system_new = NULL;
    provider->calendar_system_format_date = NULL;
}

static bool has_calendar_runtime(const SsCalendarPreviewProvider *provider)
{
    return provider != NULL &&
           infiltratr_dynlib_is_open(&provider->library) &&
           provider->calendar_system_new != NULL &&
           provider->calendar_system_format_date != NULL;
}

static bool bind_runtime(SsCalendarPreviewProvider *provider)
{
    InfiltratrDynlibBinding bindings[] = {
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

    reset_bindings(provider);
    if (!infiltratr_dynlib_bind_symbols(
            &provider->library,
            bindings,
            sizeof(bindings) / sizeof(bindings[0]))) {
        reset_bindings(provider);
        return false;
    }
    return has_calendar_runtime(provider);
}

/*
 * Rebinding is destructive by design: close any previous handle and clear all
 * symbol state before trying the new candidate so pointers can never outlive
 * the library that supplied them.
 */
static bool open_runtime(
    SsCalendarPreviewProvider *provider,
    const char *library_name)
{
    if (library_name == NULL || library_name[0] == '\0') {
        return false;
    }

    if (infiltratr_dynlib_is_open(&provider->library)) {
        infiltratr_dynlib_close(&provider->library);
    }
    reset_bindings(provider);

    if (!infiltratr_dynlib_open(&provider->library, library_name)) {
        return false;
    }
    if (!bind_runtime(provider)) {
        infiltratr_dynlib_close(&provider->library);
        return false;
    }

    /*
     * Calendar may register static GTypes whose class/vtable callbacks remain
     * in GLib's process-wide registry after the last object is released. Pin
     * the accepted Linux runtime so registered type code remains executable.
     */
    void *resident = dlopen(library_name, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
    if (resident == NULL) {
        reset_bindings(provider);
        infiltratr_dynlib_close(&provider->library);
        return false;
    }
    (void)dlclose(resident);
    return true;
}

static bool try_directory(
    SsCalendarPreviewProvider *provider,
    const char *directory)
{
    static const char *const names[] = {
        CALENDAR_RUNTIME_SONAME,
        CALENDAR_RUNTIME_REALNAME
    };

    if (directory == NULL || directory[0] == '\0') {
        return false;
    }

    for (size_t index = 0U;
         index < sizeof(names) / sizeof(names[0]);
         ++index) {
        g_autofree gchar *candidate =
            g_build_filename(directory, names[index], NULL);

        if (g_file_test(candidate, G_FILE_TEST_EXISTS) &&
            open_runtime(provider, candidate)) {
            return true;
        }
    }
    return false;
}

static bool try_arch_directory(
    SsCalendarPreviewProvider *provider,
    const char *root)
{
    const char *architecture = SYSTEM_SETTINGS_LIBRARY_ARCHITECTURE;

    if (architecture[0] == '\0') {
        return false;
    }

    g_autofree gchar *directory =
        g_build_filename(root, architecture, NULL);
    return try_directory(provider, directory);
}

static bool discover_runtime(SsCalendarPreviewProvider *provider)
{
    static const char *const roots[] = {
        "/usr/lib",
        "/lib",
        "/usr/local/lib",
        "/usr/lib64",
        "/lib64"
    };

    if (provider->discovery_root_override != NULL) {
        return try_arch_directory(
                   provider, provider->discovery_root_override) ||
               try_directory(
                   provider, provider->discovery_root_override);
    }

    for (size_t index = 0U;
         index < sizeof(roots) / sizeof(roots[0]);
         ++index) {
        if (try_arch_directory(provider, roots[index]) ||
            try_directory(provider, roots[index])) {
            return true;
        }
    }

    /*
     * Optional in-process Calendar code is admitted only from explicit
     * system-owned roots. Bare-soname resolution can consult environment-
     * controlled search paths and is outside the trusted runtime contract.
     */
    return false;
}

static bool ensure_runtime(SsCalendarPreviewProvider *provider)
{
    gint64 now;

    if (provider == NULL) {
        return false;
    }
    if (has_calendar_runtime(provider)) {
        return true;
    }
    if (!provider->discover_default_runtime) {
        return false;
    }

    now = g_get_monotonic_time();
    if (provider->retry_after_monotonic_us > now) {
        return false;
    }

    if (provider->retry_delay_us <= 0) {
        provider->retry_delay_us = DISCOVERY_RETRY_INITIAL_USEC;
    }
    provider->retry_after_monotonic_us =
        now + provider->retry_delay_us;
    if (discover_runtime(provider)) {
        provider->retry_after_monotonic_us = 0;
        provider->retry_delay_us = DISCOVERY_RETRY_INITIAL_USEC;
        return true;
    }
    provider->retry_delay_us = MIN(
        provider->retry_delay_us * 2,
        (gint64)DISCOVERY_RETRY_MAX_USEC);
    return false;
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

    if (!open_runtime(provider, library_name)) {
        ss_calendar_preview_provider_free(provider);
        return NULL;
    }
    return provider;
}

SsCalendarPreviewProvider *ss_calendar_preview_provider_new(void)
{
    SsCalendarPreviewProvider *provider =
        g_new0(SsCalendarPreviewProvider, 1);

    provider->library = (InfiltratrDynlib)INFILTRATR_DYNLIB_INIT;
    provider->discover_default_runtime = true;
    provider->retry_delay_us = DISCOVERY_RETRY_INITIAL_USEC;
    return provider;
}

SsCalendarPreviewProvider *ss_calendar_preview_provider_new_with_root_for_test(
    const char *root)
{
    SsCalendarPreviewProvider *provider;

    if (root == NULL || root[0] == '\0') {
        return NULL;
    }

    provider = ss_calendar_preview_provider_new();
    provider->discovery_root_override = g_strdup(root);
    return provider;
}

void ss_calendar_preview_provider_force_retry_for_test(
    SsCalendarPreviewProvider *provider)
{
    if (provider != NULL) {
        provider->retry_after_monotonic_us = 0;
        provider->retry_delay_us = DISCOVERY_RETRY_INITIAL_USEC;
    }
}

void ss_calendar_preview_provider_free(
    SsCalendarPreviewProvider *provider)
{
    if (provider == NULL) {
        return;
    }

    g_clear_object(&provider->calendar);
    infiltratr_dynlib_close(&provider->library);
    g_clear_pointer(&provider->discovery_root_override, g_free);
    g_free(provider);
}

bool ss_calendar_preview_provider_available(
    const SsCalendarPreviewProvider *provider)
{
    return has_calendar_runtime(provider);
}

/*
 * Cache one Calendar object because repeated preview refreshes normally target
 * the same chronology. A changed identifier is constructed first, then
 * atomically replaces the old cached object.
 */
static bool select_calendar(
    SsCalendarPreviewProvider *provider,
    const char *calendar_id)
{
    GObject *calendar;

    if (!ensure_runtime(provider) ||
        !has_calendar_runtime(provider) ||
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

    g_clear_object(&provider->calendar);
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
