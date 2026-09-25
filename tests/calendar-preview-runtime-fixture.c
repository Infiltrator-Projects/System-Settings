// SPDX-License-Identifier: GPL-3.0-or-later
#include <glib-object.h>

#include <stdint.h>
#include <string.h>

int calendar_plus_time_mode_from_string(const char *mode);
char *calendar_plus_format_time_at_location(
    int mode,
    int64_t unix_microseconds,
    int utc_offset_seconds,
    int show_seconds,
    int vertical,
    double latitude,
    double longitude);
char *calendar_plus_format_time(
    int mode,
    int64_t unix_microseconds,
    int utc_offset_seconds,
    int show_seconds,
    int vertical,
    double longitude);
GObject *calendar_plus_calendar_system_new(const char *calendar_id);
char *calendar_plus_calendar_system_format_date(
    GObject *calendar,
    int gregorian_year,
    int gregorian_month,
    int gregorian_day,
    const char *part);

int calendar_plus_time_mode_from_string(const char *mode)
{
    return mode != NULL && strcmp(mode, "internet") == 0 ? 1 : 0;
}

char *calendar_plus_format_time_at_location(
    int mode,
    int64_t unix_microseconds,
    int utc_offset_seconds,
    int show_seconds,
    int vertical,
    double latitude,
    double longitude)
{
    (void)unix_microseconds;
    (void)utc_offset_seconds;
    (void)show_seconds;
    (void)vertical;
    (void)latitude;
    (void)longitude;
    return mode == 1 ? g_strdup("@481") : g_strdup("");
}

char *calendar_plus_format_time(
    int mode,
    int64_t unix_microseconds,
    int utc_offset_seconds,
    int show_seconds,
    int vertical,
    double longitude)
{
    (void)unix_microseconds;
    (void)utc_offset_seconds;
    (void)show_seconds;
    (void)vertical;
    (void)longitude;
    return mode == 1 ? g_strdup("@481") : g_strdup("");
}

/* A callback in the DSO proves that retaining only the GType name is unsafe
 * if the module is unloaded after its provider is freed. */
static void fixture_finalize(GObject *object)
{
    GObjectClass *parent = g_type_class_peek_parent(G_OBJECT_GET_CLASS(object));
    parent->finalize(object);
}

static void fixture_class_init(gpointer klass, gpointer data)
{
    (void)data;
    G_OBJECT_CLASS(klass)->finalize = fixture_finalize;
}

GObject *calendar_plus_calendar_system_new(const char *calendar_id)
{
    if (calendar_id == NULL || strcmp(calendar_id, "positivist") != 0) {
        return NULL;
    }
    GType type = g_type_from_name("SsPreviewFixtureObject");
    if (type == G_TYPE_INVALID) {
        type = g_type_register_static_simple(G_TYPE_OBJECT, "SsPreviewFixtureObject",
            sizeof(GObjectClass), fixture_class_init, sizeof(GObject), NULL, 0);
    }
    return g_object_new(type, NULL);
}

char *calendar_plus_calendar_system_format_date(
    GObject *calendar,
    int gregorian_year,
    int gregorian_month,
    int gregorian_day,
    const char *part)
{
    if (calendar == NULL ||
        gregorian_year != 2026 ||
        gregorian_month != 9 ||
        gregorian_day != 21 ||
        part == NULL ||
        strcmp(part, "full") != 0) {
        return g_strdup("");
    }
    return g_strdup("13 Gutenberg 238");
}
