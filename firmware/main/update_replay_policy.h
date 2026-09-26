#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "update_manifest.h"

typedef struct {
    bool valid;
    uint8_t manifest_id[UPDATE_MANIFEST_ID_BYTES];
    uint32_t highest_app_version;
} update_replay_record_t;

typedef enum {
    UPDATE_REPLAY_CANDIDATE = 0,
    UPDATE_REPLAY_INVALID,
    UPDATE_REPLAY_DUPLICATE,
    UPDATE_REPLAY_NOT_NEWER,
} update_replay_result_t;

/* Compares an authenticated candidate with the last persisted acceptance.
 * The FlashCoordinator owns durable read/write of this record. */
update_replay_result_t update_replay_check(const update_manifest_t *candidate,
                                           const update_replay_record_t *accepted);
