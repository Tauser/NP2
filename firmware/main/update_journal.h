#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "update_manifest.h"
#include "update_replay_policy.h"
#define UPDATE_JOURNAL_MAGIC UINT32_C(0x4e50324a)
#define UPDATE_JOURNAL_VERSION UINT16_C(1)
typedef enum { UPDATE_JOURNAL_IDLE, UPDATE_JOURNAL_P4_STAGED, UPDATE_JOURNAL_P4_PENDING, UPDATE_JOURNAL_ACCEPTED } update_journal_state_t;
typedef struct { uint32_t magic; uint16_t version; uint16_t state; uint32_t generation; uint8_t manifest_id[16]; uint32_t app_version; uint32_t crc32; } update_journal_record_t;
void update_journal_init(update_journal_record_t *record);
bool update_journal_prepare(update_journal_record_t *record, const update_manifest_t *manifest);
bool update_journal_is_valid(const update_journal_record_t *record);
bool update_journal_transition(update_journal_record_t *record, update_journal_state_t next);
/* Produces a sealed, generation-zero successor for FlashCoordinator to persist. */
bool update_journal_next(const update_journal_record_t *previous,
                         update_journal_state_t next,
                         update_journal_record_t *out_record);
bool update_journal_can_follow(const update_journal_record_t *previous,
                               const update_journal_record_t *candidate);
bool update_journal_set_generation(update_journal_record_t *record, uint32_t generation);
bool update_journal_to_replay_record(const update_journal_record_t *journal,
                                     update_replay_record_t *out_record);
