#include "timezone_catalog.h"

#include <string.h>

/* Source: nayarsystems/posix_tz_db zones.csv, commit
 * 93447c0ddac304ca6672a5fd905261c7e8905159 (MIT; see
 * timezone_catalog.LICENSE). The CSV remains in flash and is parsed only when
 * a selection is applied or a bounded UI list is populated. The generated
 * flash index avoids rescanning the CSV from its beginning for every row. */
extern const uint8_t timezone_catalog_csv_start[]
    asm("_binary_timezone_catalog_csv_start");
extern const uint8_t timezone_catalog_csv_end[]
    asm("_binary_timezone_catalog_csv_end");

#include "timezone_catalog_index.inc"
_Static_assert(sizeof(s_catalog_offsets) / sizeof(s_catalog_offsets[0]) ==
               TIMEZONE_CATALOG_COUNT - 5U, "Timezone catalog index count mismatch");

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

    const size_t offset = s_catalog_offsets[index - 5U];
    if (offset >= (size_t)(timezone_catalog_csv_end - timezone_catalog_csv_start)) return false;
    const uint8_t *cursor = timezone_catalog_csv_start + offset;
    return parse_next(&cursor, out_entry);
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

const char *timezone_catalog_country(uint16_t index)
{
    return timezone_catalog_is_valid(index) ? s_country_names[s_country_ids[index]] : "";
}

bool timezone_catalog_standard_offset(uint16_t index, int16_t *minutes)
{
    timezone_catalog_entry_t entry = {0};
    if (minutes == NULL || !timezone_catalog_get(index, &entry)) return false;
    const char *p = entry.posix, *end = p + entry.posix_length;
    if (p < end && *p == '<') {
        while (p < end && *p != '>') ++p;
        if (p == end) return false;
        ++p;
    } else {
        while (p < end && ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) ++p;
    }
    int sign = 1;
    if (p < end && (*p == '-' || *p == '+')) { if (*p == '-') sign = -1; ++p; }
    if (p == end || *p < '0' || *p > '9') return false;
    unsigned hour = 0, minute = 0;
    while (p < end && *p >= '0' && *p <= '9') hour = hour * 10U + (unsigned)(*p++ - '0');
    if (p < end && *p == ':') {
        ++p;
        if (p == end || *p < '0' || *p > '9') return false;
        while (p < end && *p >= '0' && *p <= '9') minute = minute * 10U + (unsigned)(*p++ - '0');
    }
    if (hour > 24U || minute > 59U) return false;
    /* POSIX signs are opposite to geographical UTC offsets. DST is not
     * guessed here: this is explicitly the standard offset for the zone. */
    *minutes = (int16_t)(-sign * (int)(hour * 60U + minute));
    return true;
}
