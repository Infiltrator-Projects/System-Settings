// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file regional-context.c
 * @brief Linux IANA time-zone discovery and tzdata reference-coordinate logic.
 */
#include "system-settings/regional-context.h"

#include <glib.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static bool timezone_id_valid(const char *value)
{
    const unsigned char *cursor;

    if (value == NULL || value[0] == '\0' ||
        strlen(value) >= SS_TIMEZONE_ID_CAPACITY) {
        return false;
    }

    for (cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor) {
        const unsigned char ch = *cursor;
        if ((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '/' || ch == '_' || ch == '-' || ch == '+') {
            continue;
        }
        return false;
    }

    return strstr(value, "..") == NULL;
}

static bool copy_timezone_id(char *destination,
                             size_t capacity,
                             const char *value)
{
    if (destination == NULL || capacity == 0U ||
        !timezone_id_valid(value)) {
        return false;
    }
    return g_strlcpy(destination, value, capacity) < capacity;
}

static bool read_timezone_file(char *destination, size_t capacity)
{
    gchar *contents = NULL;
    gsize length = 0U;
    bool ok = false;

    if (!g_file_get_contents("/etc/timezone", &contents, &length, NULL) ||
        contents == NULL || length == 0U) {
        g_free(contents);
        return false;
    }

    g_strstrip(contents);
    ok = copy_timezone_id(destination, capacity, contents);
    g_free(contents);
    return ok;
}

static bool read_timezone_localtime_link(char *destination, size_t capacity)
{
    gchar *link = g_file_read_link("/etc/localtime", NULL);
    gchar *canonical;
    const char *marker = "/usr/share/zoneinfo/";
    const char *zone;
    bool ok = false;

    if (link == NULL) {
        return false;
    }

    canonical = g_canonicalize_filename(link, "/etc");
    g_free(link);
    if (canonical == NULL) {
        return false;
    }

    zone = strstr(canonical, marker);
    if (zone != NULL) {
        zone += strlen(marker);
        ok = copy_timezone_id(destination, capacity, zone);
    }

    g_free(canonical);
    return ok;
}

static bool read_timezone_glib(char *destination, size_t capacity)
{
    GTimeZone *zone = g_time_zone_new_local();
    const char *identifier;
    bool ok = false;

    if (zone == NULL) {
        return false;
    }
    identifier = g_time_zone_get_identifier(zone);
    if (identifier != NULL) {
        ok = copy_timezone_id(destination, capacity, identifier);
    }
    g_time_zone_unref(zone);
    return ok;
}

/*
 * tzdata zone.tab coordinates use ISO 6709 compact forms:
 * latitude  ±DDMM[SS], longitude ±DDDMM[SS].
 * Degree width is therefore supplied by the caller (2 for latitude, 3 for
 * longitude), while minutes/optional seconds must stay below 60 and the poles/
 * antimeridian cannot carry a non-zero fractional component.
 */
static bool parse_component(const char *text,
                            size_t length,
                            size_t degree_digits,
                            double maximum,
                            double *value)
{
    size_t index;
    unsigned int degrees = 0U;
    unsigned int minutes = 0U;
    unsigned int seconds = 0U;
    int sign;

    if (text == NULL || value == NULL ||
        (text[0] != '+' && text[0] != '-') ||
        (length != 1U + degree_digits + 2U &&
         length != 1U + degree_digits + 4U)) {
        return false;
    }

    sign = text[0] == '-' ? -1 : 1;
    for (index = 1U; index < length; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }

    for (index = 0U; index < degree_digits; ++index) {
        degrees = degrees * 10U +
                  (unsigned int)(text[1U + index] - '0');
    }
    minutes = (unsigned int)(text[1U + degree_digits] - '0') * 10U +
              (unsigned int)(text[2U + degree_digits] - '0');

    if (length == 1U + degree_digits + 4U) {
        seconds =
            (unsigned int)(text[3U + degree_digits] - '0') * 10U +
            (unsigned int)(text[4U + degree_digits] - '0');
    }

    if (minutes >= 60U || seconds >= 60U ||
        (double)degrees > maximum ||
        ((double)degrees == maximum && (minutes != 0U || seconds != 0U))) {
        return false;
    }

    *value = (double)sign *
             ((double)degrees +
              (double)minutes / 60.0 +
              (double)seconds / 3600.0);
    return true;
}

static bool parse_zone_coordinate(const char *coordinate,
                                  double *latitude,
                                  double *longitude)
{
    size_t length;
    size_t split;

    if (coordinate == NULL || latitude == NULL || longitude == NULL) {
        return false;
    }

    length = strlen(coordinate);
    for (split = 1U; split < length; ++split) {
        if (coordinate[split] == '+' || coordinate[split] == '-') {
            break;
        }
    }
    if (split >= length) {
        return false;
    }

    return parse_component(coordinate, split, 2U, 90.0, latitude) &&
           parse_component(coordinate + split,
                           length - split,
                           3U,
                           180.0,
                           longitude);
}

bool ss_regional_context_lookup_timezone_reference(
    const char *zone_tab_path,
    const char *timezone_id,
    double *latitude,
    double *longitude)
{
    gchar *contents = NULL;
    gchar **lines = NULL;
    bool found = false;
    size_t index;

    if (zone_tab_path == NULL || timezone_id == NULL ||
        latitude == NULL || longitude == NULL ||
        !timezone_id_valid(timezone_id) ||
        !g_file_get_contents(zone_tab_path, &contents, NULL, NULL)) {
        return false;
    }

    lines = g_strsplit(contents, "\n", -1);
    for (index = 0U; lines[index] != NULL; ++index) {
        gchar **fields;

        if (lines[index][0] == '\0' || lines[index][0] == '#') {
            continue;
        }

        fields = g_strsplit(lines[index], "\t", 4);
        if (fields[0] != NULL && fields[1] != NULL &&
            fields[2] != NULL) {
            g_strchomp(fields[2]);
            if (strcmp(fields[2], timezone_id) == 0 &&
                parse_zone_coordinate(fields[1], latitude, longitude)) {
                found = true;
                g_strfreev(fields);
                break;
            }
        }
        g_strfreev(fields);
    }

    g_strfreev(lines);
    g_free(contents);
    return found;
}

bool ss_regional_context_city_name(const char *timezone_id,
                                   char *buffer,
                                   size_t capacity)
{
    const char *source;
    size_t index;
    size_t length;

    if (timezone_id == NULL || buffer == NULL || capacity == 0U ||
        !timezone_id_valid(timezone_id)) {
        return false;
    }

    source = strrchr(timezone_id, '/');
    source = source != NULL ? source + 1 : timezone_id;
    length = strlen(source);
    if (length == 0U || length >= capacity) {
        return false;
    }

    for (index = 0U; index < length; ++index) {
        buffer[index] = source[index] == '_' ? ' ' : source[index];
    }
    buffer[length] = '\0';
    return true;
}

static bool country_field_matches(const char *field,
                                  const char *country_code)
{
    gchar **parts;
    size_t index;
    bool match = false;

    if (field == NULL || country_code == NULL ||
        country_code[0] == '\0') {
        return false;
    }

    parts = g_strsplit(field, ",", -1);
    for (index = 0U; parts[index] != NULL; ++index) {
        if (g_ascii_strcasecmp(parts[index], country_code) == 0) {
            match = true;
            break;
        }
    }
    g_strfreev(parts);
    return match;
}

static double longitude_distance(double left, double right)
{
    double distance = left - right;
    if (distance < 0.0) {
        distance = -distance;
    }
    if (distance > 180.0) {
        distance = 360.0 - distance;
    }
    return distance;
}

static bool nearest_timezone_in_file(const char *path,
                                     const char *country_code,
                                     double latitude,
                                     double longitude,
                                     char *timezone_id,
                                     size_t capacity)
{
    gchar *contents = NULL;
    gchar **lines = NULL;
    bool found = false;
    double best_score = 0.0;
    size_t index;

    if (path == NULL || country_code == NULL ||
        timezone_id == NULL || capacity == 0U ||
        !g_file_get_contents(path, &contents, NULL, NULL)) {
        return false;
    }

    lines = g_strsplit(contents, "\n", -1);
    for (index = 0U; lines[index] != NULL; ++index) {
        gchar **fields;
        double candidate_latitude;
        double candidate_longitude;
        double latitude_distance_value;
        double longitude_distance_value;
        double score;

        if (lines[index][0] == '\0' || lines[index][0] == '#') {
            continue;
        }

        fields = g_strsplit(lines[index], "\t", 4);
        if (fields[0] == NULL || fields[1] == NULL ||
            fields[2] == NULL ||
            !country_field_matches(fields[0], country_code) ||
            !parse_zone_coordinate(fields[1],
                                   &candidate_latitude,
                                   &candidate_longitude)) {
            g_strfreev(fields);
            continue;
        }

        g_strchomp(fields[2]);
        if (!timezone_id_valid(fields[2])) {
            g_strfreev(fields);
            continue;
        }

        latitude_distance_value = candidate_latitude - latitude;
        if (latitude_distance_value < 0.0) {
            latitude_distance_value = -latitude_distance_value;
        }
        longitude_distance_value =
            longitude_distance(candidate_longitude, longitude);

        /*
         * The zone.tab coordinate is only a representative point. A simple
         * bounded angular distance is deliberately used here as a hint rather
         * than pretending these points describe political/time-zone polygons.
         */
        score = latitude_distance_value + longitude_distance_value;
        if (!found || score < best_score) {
            best_score = score;
            if (g_strlcpy(timezone_id,
                          fields[2],
                          capacity) >= capacity) {
                g_strfreev(fields);
                found = false;
                break;
            }
            found = true;
        }
        g_strfreev(fields);
    }

    g_strfreev(lines);
    g_free(contents);
    return found;
}

bool ss_regional_context_nearest_timezone(
    const char *country_code,
    double latitude,
    double longitude,
    char *timezone_id,
    size_t capacity)
{
    static const char *const paths[] = {
        "/usr/share/zoneinfo/zone1970.tab",
        "/usr/share/zoneinfo/zone.tab"
    };
    size_t index;

    if (country_code == NULL || timezone_id == NULL ||
        capacity == 0U ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        return false;
    }

    for (index = 0U; index < sizeof(paths) / sizeof(paths[0]); ++index) {
        if (nearest_timezone_in_file(paths[index],
                                     country_code,
                                     latitude,
                                     longitude,
                                     timezone_id,
                                     capacity)) {
            return true;
        }
    }
    return false;
}

static gint timezone_compare(gconstpointer left, gconstpointer right)
{
    const char *const *left_text = left;
    const char *const *right_text = right;
    return g_strcmp0(*left_text, *right_text);
}

GPtrArray *ss_regional_context_list_timezones(
    const char *current_timezone_id)
{
    static const char *const paths[] = {
        "/usr/share/zoneinfo/zone.tab",
        "/usr/share/zoneinfo/zone1970.tab"
    };
    GPtrArray *zones =
        g_ptr_array_new_with_free_func(g_free);
    GHashTable *seen =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    size_t path_index;

    g_hash_table_add(seen, g_strdup("Etc/UTC"));
    g_ptr_array_add(zones, g_strdup("Etc/UTC"));

    for (path_index = 0U;
         path_index < sizeof(paths) / sizeof(paths[0]);
         ++path_index) {
        gchar *contents = NULL;
        gchar **lines = NULL;
        size_t line_index;

        if (!g_file_get_contents(
                paths[path_index], &contents, NULL, NULL)) {
            continue;
        }

        lines = g_strsplit(contents, "\n", -1);
        for (line_index = 0U;
             lines[line_index] != NULL;
             ++line_index) {
            gchar **fields;
            gchar *zone_copy;

            if (lines[line_index][0] == '\0' ||
                lines[line_index][0] == '#') {
                continue;
            }

            fields = g_strsplit(lines[line_index], "\t", 4);
            if (fields[2] == NULL) {
                g_strfreev(fields);
                continue;
            }
            g_strchomp(fields[2]);
            if (!timezone_id_valid(fields[2]) ||
                g_hash_table_contains(seen, fields[2])) {
                g_strfreev(fields);
                continue;
            }

            zone_copy = g_strdup(fields[2]);
            g_hash_table_add(seen, g_strdup(zone_copy));
            g_ptr_array_add(zones, zone_copy);
            g_strfreev(fields);
        }

        g_strfreev(lines);
        g_free(contents);
        if (zones->len > 1U) {
            break;
        }
    }

    /*
     * timedated may report a valid tzdata alias that is intentionally absent
     * from zone.tab/zone1970.tab. The authoritative current value must still
     * be representable in the selector rather than silently falling back to
     * the first catalogue entry.
     */
    if (current_timezone_id != NULL &&
        timezone_id_valid(current_timezone_id) &&
        !g_hash_table_contains(seen, current_timezone_id)) {
        g_hash_table_add(seen, g_strdup(current_timezone_id));
        g_ptr_array_add(zones, g_strdup(current_timezone_id));
    }

    g_hash_table_unref(seen);
    g_ptr_array_sort(zones, timezone_compare);
    return zones;
}

bool ss_regional_context_detect(SsRegionalContext *context)
{
    static const char *const zone_tables[] = {
        "/usr/share/zoneinfo/zone1970.tab",
        "/usr/share/zoneinfo/zone.tab"
    };
    size_t index;

    if (context == NULL) {
        return false;
    }
    memset(context, 0, sizeof(*context));

    /*
     * Prefer the distribution's explicit /etc/timezone identity, then the
     * canonical /etc/localtime symlink, then GLib's local-zone fallback. This
     * preserves a stable IANA identifier where the platform exposes one.
     */
    if (!read_timezone_file(context->timezone_id,
                            sizeof(context->timezone_id)) &&
        !read_timezone_localtime_link(context->timezone_id,
                                      sizeof(context->timezone_id)) &&
        !read_timezone_glib(context->timezone_id,
                            sizeof(context->timezone_id))) {
        return false;
    }

    (void)ss_regional_context_city_name(
        context->timezone_id,
        context->timezone_city,
        sizeof(context->timezone_city));

    for (index = 0U;
         index < sizeof(zone_tables) / sizeof(zone_tables[0]);
         ++index) {
        if (ss_regional_context_lookup_timezone_reference(
                zone_tables[index],
                context->timezone_id,
                &context->reference_latitude,
                &context->reference_longitude)) {
            context->has_reference_coordinates = true;
            break;
        }
    }

    return true;
}
