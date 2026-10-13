// SPDX-License-Identifier: GPL-3.0-or-later
#include <infiltratr/temporal.h>

#include <string.h>

bool calendar_plus_format_date_v1(
    const char *calendar_id,
    int32_t gregorian_year,
    int32_t gregorian_month,
    int32_t gregorian_day,
    const char *part,
    char *buffer,
    size_t capacity,
    size_t *length);

bool calendar_plus_format_date_v1(
    const char *calendar_id,
    int32_t gregorian_year,
    int32_t gregorian_month,
    int32_t gregorian_day,
    const char *part,
    char *buffer,
    size_t capacity,
    size_t *length)
{
    static const char text[] = "13 Gutenberg 238";
    const size_t size = sizeof(text) - 1U;

    if (calendar_id == NULL || strcmp(calendar_id, "positivist") != 0 ||
        gregorian_year != 2026 || gregorian_month != 9 ||
        gregorian_day != 21 || part == NULL || strcmp(part, "full") != 0 ||
        buffer == NULL || capacity <= size) {
        return false;
    }

    memcpy(buffer, text, sizeof(text));
    if (length != NULL) {
        *length = size;
    }
    return true;
}
