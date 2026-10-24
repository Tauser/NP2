#include "agenda_storage.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "cache_record.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"

#define AGENDA_MAGIC UINT32_C(0x4e503241)
/* Read the installed 12-item record before writing the expanded v2 format. */
typedef struct {
    uint32_t instance_id, next_id;
    uint8_t count, reserved[3];
    agenda_item_t items[12];
    agenda_remote_t remote[12];
} legacy_data_t;
_Static_assert(sizeof(legacy_data_t) == 2868U, "legacy agenda layout changed");
typedef struct { cache_record_header_t header; agenda_data_t data; } record_t;
typedef struct { record_t scratch; record_t committed; } workspace_t;
static workspace_t *s_workspace;
static esp_err_t s_result = ESP_ERR_INVALID_STATE;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static const char *const s_slots[] = {"/lfsdiag/agenda.0", "/lfsdiag/agenda.1"};

bool agenda_storage_owner_format_allowed(void)
{
    return s_result == ESP_ERR_NOT_FOUND;
}

static bool valid(const record_t *r)
{
    return r->header.magic == AGENDA_MAGIC && r->header.schema_version == 2U &&
        r->header.header_size == sizeof(r->header) && r->header.generation != 0U &&
        r->header.payload_size == sizeof(r->data) &&
        r->header.header_crc32 == cache_record_crc32((const uint8_t *)&r->header,
            offsetof(cache_record_header_t, header_crc32)) &&
        r->header.payload_crc32 == cache_record_crc32((const uint8_t *)&r->data, sizeof(r->data)) &&
        agenda_data_valid(&r->data);
}

static bool read_record(FILE *file, record_t *r)
{
    memset(r, 0, sizeof(*r));
    if (fread(&r->header, 1U, sizeof(r->header), file) != sizeof(r->header)) return false;
    const bool legacy = r->header.schema_version == 1U;
    const size_t bytes = legacy ? sizeof(legacy_data_t) : sizeof(r->data);
    if (r->header.magic != AGENDA_MAGIC || (!legacy && r->header.schema_version != 2U) ||
        r->header.header_size != sizeof(r->header) || r->header.generation == 0U ||
        r->header.payload_size != bytes ||
        r->header.header_crc32 != cache_record_crc32((const uint8_t *)&r->header,
            offsetof(cache_record_header_t, header_crc32))) return false;
    if (fread(&r->data, 1U, bytes, file) != bytes || fgetc(file) != EOF || ferror(file) ||
        r->header.payload_crc32 != cache_record_crc32((const uint8_t *)&r->data, bytes)) return false;
    if (legacy) {
        if (r->data.count > 12U) return false;
        memmove(r->data.remote, (uint8_t *)&r->data + offsetof(legacy_data_t, remote),
                sizeof(((legacy_data_t *)0)->remote));
        memset(&r->data.items[12], 0, (AGENDA_MAX_ITEMS-12U)*sizeof(r->data.items[0]));
        memset(&r->data.remote[12], 0, (AGENDA_MAX_ITEMS-12U)*sizeof(r->data.remote[0]));
        r->header.schema_version = 2U;
        r->header.payload_size = sizeof(r->data);
        r->header.payload_crc32 = cache_record_crc32((const uint8_t *)&r->data, sizeof(r->data));
        r->header.header_crc32 = cache_record_crc32((const uint8_t *)&r->header,
            offsetof(cache_record_header_t, header_crc32));
    }
    return valid(r);
}

esp_err_t agenda_storage_owner_load(void)
{
    if (s_workspace == NULL) {
        workspace_t *allocated = heap_caps_calloc(1U, sizeof(*allocated), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (allocated == NULL) return ESP_ERR_NO_MEM;
        portENTER_CRITICAL(&s_lock);
        s_workspace = allocated;
        portEXIT_CRITICAL(&s_lock);
    }
    bool found = false, damaged = false;
    for (unsigned slot = 0U; slot < 2U; ++slot) {
        FILE *f = fopen(s_slots[slot], "rb");
        if (f == NULL) { if (errno != ENOENT) damaged = true; continue; }
        record_t *r = &s_workspace->scratch;
        const bool complete = read_record(f, r);
        fclose(f);
        if (!complete || !valid(r)) { damaged = true; continue; }
        if (!found || r->header.generation > s_workspace->committed.header.generation) {
            portENTER_CRITICAL(&s_lock);
            s_workspace->committed = *r;
            portEXIT_CRITICAL(&s_lock);
            found = true;
        }
    }
    portENTER_CRITICAL(&s_lock);
    s_result = found ? ESP_OK : damaged ? ESP_ERR_INVALID_CRC : ESP_ERR_NOT_FOUND;
    portEXIT_CRITICAL(&s_lock);
    return s_result;
}

esp_err_t agenda_storage_copy(agenda_data_t *out)
{
    if (out == NULL) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    esp_err_t result = s_result;
    if (result == ESP_OK && s_workspace != NULL) *out = s_workspace->committed.data;
    portEXIT_CRITICAL(&s_lock);
    return result;
}

esp_err_t agenda_storage_owner_write(const agenda_data_t *data)
{
    if (!agenda_data_valid(data) || s_workspace == NULL) return ESP_ERR_INVALID_ARG;
    record_t *r = &s_workspace->scratch;
    const uint32_t prior = s_result == ESP_OK ? s_workspace->committed.header.generation : 0U;
    if (prior == UINT32_MAX) return ESP_ERR_INVALID_STATE;
    memset(r, 0, sizeof(*r));
    r->data = *data;
    r->header = (cache_record_header_t){.magic = AGENDA_MAGIC, .schema_version = 2U,
        .header_size = sizeof(r->header), .generation = prior + 1U, .payload_size = sizeof(r->data)};
    r->header.payload_crc32 = cache_record_crc32((const uint8_t *)&r->data, sizeof(r->data));
    r->header.header_crc32 = cache_record_crc32((const uint8_t *)&r->header,
        offsetof(cache_record_header_t, header_crc32));
    const char *tmp = "/lfsdiag/agenda.tmp";
    FILE *f = fopen(tmp, "wb");
    if (f == NULL) return ESP_FAIL;
    bool ok = fwrite(r, 1U, sizeof(*r), f) == sizeof(*r);
    if (ok) ok = fflush(f) == 0 && fsync(fileno(f)) == 0;
    if (fclose(f) != 0) ok = false;
    if (!ok) return ESP_FAIL;
    f = fopen(tmp, "rb");
    if (f == NULL) return ESP_FAIL;
    ok = fread(r, 1U, sizeof(*r), f) == sizeof(*r) && fgetc(f) == EOF && !ferror(f);
    fclose(f);
    if (!ok || !valid(r) || r->header.generation != prior + 1U ||
        memcmp(&r->data, data, sizeof(*data)) != 0) return ESP_ERR_INVALID_CRC;
    if (rename(tmp, s_slots[r->header.generation & 1U]) != 0) return ESP_FAIL;
    portENTER_CRITICAL(&s_lock);
    s_workspace->committed = *r;
    s_result = ESP_OK;
    portEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}
