#include "timezone_catalog.h"

#include <string.h>

/* Source: nayarsystems/posix_tz_db zones.csv, commit
 * 93447c0ddac304ca6672a5fd905261c7e8905159 (MIT; see
 * timezone_catalog.LICENSE). The CSV remains in flash and is parsed only when
 * a selection is applied or a bounded UI list is populated. */
extern const uint8_t timezone_catalog_csv_start[]
    asm("_binary_timezone_catalog_csv_start");
extern const uint8_t timezone_catalog_csv_end[]
    asm("_binary_timezone_catalog_csv_end");

typedef struct {
    const char *iana;
    const char *posix;
} primary_timezone_t;

static const primary_timezone_t s_primary[] = {
    {"America/Sao_Paulo", "<-03>3"},
    {"America/Sao_Paulo", "<-03>3"},
    {"America/Argentina/Buenos_Aires", "<-03>3"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0/2"},
};

static bool starts_with(const uint8_t *cursor, const uint8_t *end,
                        const char *text)
{
    while (*text != '\0') {
        if (cursor >= end || *cursor++ != (uint8_t)*text++) return false;
    }
    return true;
}

static bool find_line_end(const uint8_t *cursor, const uint8_t *end,
                          const uint8_t **out_end)
{
    while (cursor < end && *cursor != '\n') ++cursor;
    if (out_end != NULL) *out_end = cursor;
    return cursor < end;
}

static bool parse_next(const uint8_t **cursor, timezone_catalog_entry_t *out_entry)
{
    const uint8_t *const end = timezone_catalog_csv_end;
    const uint8_t *line = *cursor;
    const uint8_t *line_end = NULL;
    if (line >= end || !find_line_end(line, end, &line_end) ||
        !starts_with(line, line_end, "\"")) {
        return false;
    }

    const uint8_t *iana = line + 1;
    const uint8_t *separator = iana;
    while (separator + 3 < line_end &&
           !(separator[0] == '\"' && separator[1] == ',' && separator[2] == '\"')) {
        ++separator;
    }
    if (separator + 3 >= line_end || line_end <= separator + 3 ||
        line_end[-1] != '\"') {
        return false;
    }
    const uint8_t *const posix = separator + 3;
    out_entry->iana = (const char *)iana;
    out_entry->iana_length = (size_t)(separator - iana);
    out_entry->posix = (const char *)posix;
    out_entry->posix_length = (size_t)((line_end - 1) - posix);
    *cursor = line_end + 1;
    return true;
}

static bool same_iana(const timezone_catalog_entry_t *entry, const char *iana)
{
    const size_t length = strlen(iana);
    return entry->iana_length == length && memcmp(entry->iana, iana, length) == 0;
}

static bool duplicates_primary(const timezone_catalog_entry_t *entry)
{
    /* America/Brasilia is retained as a friendly legacy choice; the other
     * four entries are already present in the upstream IANA catalog. */
    return same_iana(entry, "America/Sao_Paulo") ||
           same_iana(entry, "America/Argentina/Buenos_Aires") ||
           same_iana(entry, "America/New_York") ||
           same_iana(entry, "Europe/London");
}

static bool primary_entry(uint16_t index, timezone_catalog_entry_t *out_entry)
{
    if (index >= sizeof(s_primary) / sizeof(s_primary[0])) return false;
    const primary_timezone_t *const source = &s_primary[index];
    *out_entry = (timezone_catalog_entry_t){
        .iana = source->iana,
        .iana_length = strlen(source->iana),
        .posix = source->posix,
        .posix_length = strlen(source->posix),
    };
    return true;
}

bool timezone_catalog_get(uint16_t index, timezone_catalog_entry_t *out_entry)
{
    if (out_entry == NULL || index >= TIMEZONE_CATALOG_COUNT) return false;
    if (primary_entry(index, out_entry)) return true;

    uint16_t remaining = index - (uint16_t)(sizeof(s_primary) / sizeof(s_primary[0]));
    const uint8_t *cursor = timezone_catalog_csv_start;
    timezone_catalog_entry_t candidate = {0};
    while (parse_next(&cursor, &candidate)) {
        if (duplicates_primary(&candidate)) continue;
        if (remaining-- == 0U) {
            *out_entry = candidate;
            return true;
        }
    }
    return false;
}

bool timezone_catalog_is_valid(uint16_t index)
{
    return index < TIMEZONE_CATALOG_COUNT;
}

size_t timezone_catalog_count(void)
{
    return TIMEZONE_CATALOG_COUNT;
}

static bool copy_entry_value(uint16_t index, bool posix, char *out, size_t out_size)
{
    timezone_catalog_entry_t entry = {0};
    if (out == NULL || !timezone_catalog_get(index, &entry)) return false;
    const char *const source = posix ? entry.posix : entry.iana;
    const size_t length = posix ? entry.posix_length : entry.iana_length;
    if (out_size <= length) return false;
    memcpy(out, source, length);
    out[length] = '\0';
    return true;
}

bool timezone_catalog_copy_iana(uint16_t index, char *out, size_t out_size)
{
    return copy_entry_value(index, false, out, out_size);
}

bool timezone_catalog_copy_posix(uint16_t index, char *out, size_t out_size)
{
    return copy_entry_value(index, true, out, out_size);
}
