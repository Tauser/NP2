#include <assert.h>
#include <stdio.h>

#include "update_journal.h"

int main(void)
{
    update_journal_record_t record = {0};
    update_manifest_t manifest = {.manifest_id = {1}, .app_version = 2};
    update_replay_record_t replay = {0};

    update_journal_init(&record);
    assert(update_journal_prepare(&record, &manifest));
    assert(update_journal_transition(&record, UPDATE_JOURNAL_P4_STAGED));
    assert(record.generation == 0U);

    update_journal_record_t staged = record;
    assert(update_journal_set_generation(&staged, 1U));
    assert(update_journal_transition(&record, UPDATE_JOURNAL_IDLE));
    assert(update_journal_can_follow(&staged, &record));
    update_journal_record_t successor = {0};
    assert(update_journal_next(&staged, UPDATE_JOURNAL_IDLE, &successor));
    assert(successor.generation == 0U);
    assert(update_journal_can_follow(&staged, &successor));

    update_journal_init(&record);
    assert(update_journal_prepare(&record, &manifest));
    assert(update_journal_transition(&record, UPDATE_JOURNAL_P4_STAGED));

    update_journal_record_t persisted = record;
    assert(update_journal_set_generation(&persisted, 1U));
    assert(update_journal_transition(&record, UPDATE_JOURNAL_P4_PENDING));
    assert(update_journal_can_follow(&persisted, &record));
    assert(!update_journal_to_replay_record(&record, &replay));
    assert(!update_journal_transition(&record, UPDATE_JOURNAL_IDLE));

    persisted = record;
    assert(update_journal_set_generation(&persisted, 2U));
    /* The executor must reseal the generation returned by NVS before select. */
    update_journal_record_t stale_crc = record;
    stale_crc.generation = 2U;
    assert(!update_journal_is_valid(&stale_crc));
    update_journal_record_t activation = record;
    assert(update_journal_set_generation(&activation, 2U));
    assert(update_journal_is_valid(&activation));
    assert(activation.crc32 == persisted.crc32);
    assert(update_journal_transition(&record, UPDATE_JOURNAL_ACCEPTED));
    assert(update_journal_can_follow(&persisted, &record));
    assert(update_journal_to_replay_record(&record, &replay));
    assert(replay.highest_app_version == 2U);

    persisted = record;
    assert(update_journal_set_generation(&persisted, 3U));
    assert(update_journal_transition(&record, UPDATE_JOURNAL_IDLE));
    assert(update_journal_can_follow(&persisted, &record));
    assert(update_journal_to_replay_record(&record, &replay));

    persisted = record;
    assert(update_journal_set_generation(&persisted, 4U));

    update_journal_init(&record);
    manifest.app_version = 3U;
    assert(update_journal_prepare(&record, &manifest));
    assert(update_journal_transition(&record, UPDATE_JOURNAL_P4_STAGED));
    assert(update_journal_can_follow(&persisted, &record));

    assert(update_journal_set_generation(&persisted, 10U));
    assert(!update_journal_set_generation(&persisted, 10U));
    persisted.app_version ^= 1U;
    assert(!update_journal_is_valid(&persisted));

    puts("update_journal_host_test: all checks passed");
    return 0;
}
