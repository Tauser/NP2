#include "agenda_service.h"

#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(agenda_item_t) == 72U, "agenda v1 item layout changed");
_Static_assert(sizeof(agenda_data_t) == 15244U, "agenda v2 record layout changed");

static bool title_valid(const char *text)
{
    bool visible = false;
    size_t i = 0U;
    while (i < AGENDA_TITLE_BYTES && text[i] != '\0') {
        const uint8_t c = (uint8_t)text[i++];
        if (c < 32U || c == 127U) return false;
        if (c != ' ') visible = true;
        if (c < 128U) continue;
        unsigned extra;
        uint32_t code, minimum;
        if (c >= 0xc2U && c <= 0xdfU) { extra = 1U; code = c & 31U; minimum = 128U; }
        else if (c >= 0xe0U && c <= 0xefU) { extra = 2U; code = c & 15U; minimum = 2048U; }
        else if (c >= 0xf0U && c <= 0xf4U) { extra = 3U; code = c & 7U; minimum = 65536U; }
        else return false;
        while (extra-- > 0U) {
            if (i >= AGENDA_TITLE_BYTES) return false;
            const uint8_t next = (uint8_t)text[i++];
            if ((next & 0xc0U) != 0x80U) return false;
            code = (code << 6U) | (next & 63U);
        }
        if (code < minimum || code > 0x10ffffU || (code >= 0xd800U && code <= 0xdfffU))
            return false;
    }
    return visible && i < AGENDA_TITLE_BYTES;
}

void agenda_init(agenda_data_t *data)
{
    if (data == NULL) return;
    memset(data, 0, sizeof(*data));
    data->next_id = 1U;
    data->instance_id = 1U;
}

bool agenda_item_valid(const agenda_item_t *item)
{
    if (item == NULL || !title_valid(item->title) || item->kind > AGENDA_TASK ||
        item->completed > 1U || item->reminded > 1U ||
        (item->reminder_minutes != AGENDA_REMINDER_NONE && item->reminder_minutes > 10080U))
        return false;
    if (item->kind == AGENDA_EVENT &&
        (item->start_unix_s < AGENDA_MIN_EPOCH || item->end_unix_s <= item->start_unix_s ||
         item->completed != 0U)) return false;
    if (item->kind == AGENDA_TASK &&
        ((item->start_unix_s != 0U && item->start_unix_s < AGENDA_MIN_EPOCH) ||
         item->end_unix_s != 0U)) return false;
    return item->start_unix_s != 0U || item->reminder_minutes == AGENDA_REMINDER_NONE;
}

bool agenda_data_valid(const agenda_data_t *data)
{
    if (data == NULL || data->instance_id == 0U || data->next_id == 0U || data->count > AGENDA_MAX_ITEMS) return false;
    for (uint8_t i = 0U; i < data->count; ++i) {
        const agenda_item_t *item = &data->items[i];
        if (!agenda_item_valid(item) || item->id == 0U || item->id >= data->next_id ||
            item->revision == 0U || data->remote[i].dirty > 1U || data->remote[i].deleted > 1U ||
            memchr(data->remote[i].id, 0, sizeof(data->remote[i].id)) == NULL ||
            memchr(data->remote[i].etag, 0, sizeof(data->remote[i].etag)) == NULL) return false;
        for (uint8_t j = 0U; j < i; ++j)
            if (data->items[j].id == item->id) return false;
    }
    return true;
}

agenda_result_t agenda_apply(agenda_data_t *data, const agenda_command_t *command)
{
    if (data == NULL || command == NULL || command->request_id == 0U ||
        command->action > AGENDA_KEEP_LOCAL) return AGENDA_INVALID;
    uint8_t index = data->count;
    for (uint8_t i = 0U; i < data->count; ++i)
        if (data->items[i].id == command->item.id) { index = i; break; }
    const bool create = command->action == AGENDA_UPSERT && command->item.id == 0U;
    if (!create && index == data->count) return AGENDA_NOT_FOUND;
    if (!create && data->remote[index].deleted && command->action != AGENDA_USE_GOOGLE &&
        command->action != AGENDA_KEEP_LOCAL) return AGENDA_NOT_FOUND;
    if (!create && data->items[index].revision != command->item.revision) return AGENDA_CONFLICT;
    if (!create && data->items[index].revision == UINT32_MAX) return AGENDA_CONFLICT;
    if (!create && command->action == AGENDA_UPSERT && data->items[index].kind != command->item.kind)
        return AGENDA_INVALID;
    if (command->action == AGENDA_USE_GOOGLE || command->action == AGENDA_KEEP_LOCAL) {
        if (!data->remote[index].reserved[0] ||
            (command->action == AGENDA_KEEP_LOCAL && data->remote[index].reserved[1])) return AGENDA_INVALID;
        data->remote[index].dirty = command->action == AGENDA_KEEP_LOCAL ? 1U : 0U;
        if (command->action == AGENDA_USE_GOOGLE) data->remote[index].deleted = 0U;
        memset(data->remote[index].reserved, 0, sizeof(data->remote[index].reserved));
        ++data->items[index].revision;
        return AGENDA_OK;
    }
    if (!create && data->items[index].reserved[0]) return AGENDA_INVALID;
    if (command->action == AGENDA_DELETE) {
        data->remote[index].deleted = 1U;
        data->remote[index].dirty = 1U;
        ++data->items[index].revision;
        return AGENDA_OK;
    }
    if (command->action == AGENDA_SET_COMPLETED) {
        if (data->items[index].kind != AGENDA_TASK || command->item.completed > 1U) return AGENDA_INVALID;
        data->items[index].completed = command->item.completed;
        /* Reopening a task never replays an already delivered reminder. */
        ++data->items[index].revision;
        data->remote[index].dirty = 1U;
        return AGENDA_OK;
    }
    if (!agenda_item_valid(&command->item)) return AGENDA_INVALID;
    if (create && (data->count == AGENDA_MAX_ITEMS || data->next_id == UINT32_MAX)) return AGENDA_FULL;
    agenda_item_t item = command->item;
    item.reminded = !create && data->items[index].start_unix_s == item.start_unix_s &&
        data->items[index].reminder_minutes == item.reminder_minutes
            ? data->items[index].reminded : 0U;
    item.id = create ? data->next_id++ : item.id;
    item.revision = create ? 1U : data->items[index].revision + 1U;
    memset(item.reserved, 0, sizeof(item.reserved));
    if (create) ++data->count;
    data->items[index] = item;
    data->remote[index].dirty = 1U;
    return AGENDA_OK;
}

bool agenda_take_reminder(agenda_data_t *data, bool time_trusted, uint32_t now,
                          agenda_item_t *out_item, bool *out_changed)
{
    if (out_changed != NULL) *out_changed = false;
    if (data == NULL || !time_trusted || now < AGENDA_MIN_EPOCH) return false;
    for (uint8_t i = 0U; i < data->count; ++i) {
        agenda_item_t *item = &data->items[i];
        if (data->remote[i].deleted || item->reminded || item->completed || item->start_unix_s == 0U ||
            item->reminder_minutes == AGENDA_REMINDER_NONE) continue;
        const uint32_t due = item->start_unix_s - (uint32_t)item->reminder_minutes * 60U;
        if (now < due) continue;
        item->reminded = 1U;
        if (out_changed != NULL) *out_changed = true;
        if (now - due > AGENDA_CATCHUP_SECONDS ||
            (item->kind == AGENDA_EVENT && now >= item->end_unix_s)) continue;
        if (out_item != NULL) *out_item = *item;
        return true;
    }
    return false;
}
