#pragma once

#include <stdbool.h>
#include <stdint.h>

#define AGENDA_MAX_ITEMS 64U
#define AGENDA_TITLE_BYTES 49U
#define AGENDA_MIN_EPOCH UINT32_C(1735689600)
#define AGENDA_REMINDER_NONE UINT16_MAX
#define AGENDA_CATCHUP_SECONDS 900U
/* Sanitized sync outcome; no provider payload or credential crosses to UI. */
#define AGENDA_SYNC_AUTH_REQUIRED (-2001)

typedef enum { AGENDA_EVENT = 0, AGENDA_TASK = 1 } agenda_kind_t;
typedef enum {
    AGENDA_OK = 0, AGENDA_INVALID, AGENDA_FULL, AGENDA_NOT_FOUND,
    AGENDA_CONFLICT, AGENDA_NOT_READY
} agenda_result_t;

/* UTC seconds. A task may have no due date (start=0); events require an end.
 * Fixed-width flags keep the durable v1 record independent of enum/bool ABI. */
typedef struct {
    uint32_t id;
    uint32_t revision;
    uint32_t start_unix_s;
    uint32_t end_unix_s;
    uint16_t reminder_minutes;
    uint8_t kind;
    uint8_t completed;
    uint8_t reminded;
    char title[AGENDA_TITLE_BYTES];
    uint8_t reserved[2];
} agenda_item_t;

typedef struct {
    char id[97];
    char etag[65];
    uint8_t dirty;
    uint8_t deleted;
    uint8_t reserved[2];
} agenda_remote_t;

typedef struct {
    uint32_t instance_id;
    uint32_t next_id;
    uint8_t count;
    uint8_t reserved[3];
    agenda_item_t items[AGENDA_MAX_ITEMS];
    agenda_remote_t remote[AGENDA_MAX_ITEMS];
} agenda_data_t;

typedef struct {
    bool ready;
    bool persistence_pending;
    bool sync_busy;
    bool google_linked;
    bool google_auth_required;
    uint32_t generation;
    uint32_t completion_id;
    agenda_result_t command_result;
    int32_t storage_result;
    int32_t sync_result;
    uint32_t last_sync_unix_s;
    uint8_t count;
    agenda_item_t items[AGENDA_MAX_ITEMS];
} agenda_projection_t;

typedef enum { AGENDA_UPSERT = 0, AGENDA_DELETE, AGENDA_SET_COMPLETED,
    AGENDA_USE_GOOGLE, AGENDA_KEEP_LOCAL } agenda_action_t;
typedef struct {
    uint32_t request_id;
    agenda_action_t action;
    /* id=0 creates; edits/deletes require the revision last seen by the UI. */
    agenda_item_t item;
} agenda_command_t;

void agenda_init(agenda_data_t *data);
bool agenda_item_valid(const agenda_item_t *item);
bool agenda_data_valid(const agenda_data_t *data);
agenda_result_t agenda_apply(agenda_data_t *data, const agenda_command_t *command);
/* Marks one reminder consumed, even if notifications are muted. Old reminders
 * beyond the catch-up window expire silently. No wall clock or I/O in domain. */
bool agenda_take_reminder(agenda_data_t *data, bool time_trusted, uint32_t now,
                          agenda_item_t *out_item, bool *out_changed);
