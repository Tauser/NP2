#include "update_p4_writer.h"

#include <string.h>

#include "flash_coordinator.h"

update_p4_writer_result_t update_p4_writer_begin(update_p4_writer_t *writer,
                                                  update_image_hash_session_t *hash)
{
    if (writer == NULL || hash == NULL || writer->active || !hash->active) {
        return UPDATE_P4_WRITER_INVALID_STATE;
    }

    const esp_partition_t *partition = NULL;
    if (flash_coordinator_p4_ota_begin(hash->expected_bytes, &partition) != ESP_OK ||
        partition == NULL) {
        return UPDATE_P4_WRITER_FLASH;
    }
    *writer = (update_p4_writer_t){.partition = partition, .active = true};
    return UPDATE_P4_WRITER_OK;
}

update_p4_writer_result_t update_p4_writer_append(update_p4_writer_t *writer,
                                                   update_image_hash_session_t *hash,
                                                   const uint8_t *bytes,
                                                   size_t bytes_count)
{
    if (writer == NULL || hash == NULL || !writer->active) {
        return UPDATE_P4_WRITER_INVALID_STATE;
    }
    if (update_image_hash_append(hash, bytes, bytes_count) != UPDATE_IMAGE_HASH_OK) {
        update_p4_writer_abort(writer, hash);
        return UPDATE_P4_WRITER_HASH;
    }
    if (flash_coordinator_p4_ota_append(bytes, (uint32_t)bytes_count) != ESP_OK) {
        update_p4_writer_abort(writer, hash);
        return UPDATE_P4_WRITER_FLASH;
    }
    return UPDATE_P4_WRITER_OK;
}

update_p4_writer_result_t update_p4_writer_finish(update_p4_writer_t *writer,
                                                   update_image_hash_session_t *hash)
{
    if (writer == NULL || hash == NULL || !writer->active) {
        return UPDATE_P4_WRITER_INVALID_STATE;
    }
    if (update_image_hash_finish(hash) != UPDATE_IMAGE_HASH_OK) {
        update_p4_writer_abort(writer, hash);
        return UPDATE_P4_WRITER_HASH;
    }

    if (flash_coordinator_p4_ota_finish() != ESP_OK) {
        memset(writer, 0, sizeof(*writer));
        return UPDATE_P4_WRITER_FLASH;
    }
    writer->active = false;
    writer->finished = true;
    return UPDATE_P4_WRITER_OK;
}

update_p4_writer_result_t update_p4_writer_request_activation(
    update_p4_writer_t *writer, const update_journal_record_t *journal)
{
    if (writer == NULL || journal == NULL || !writer->finished || writer->partition == NULL ||
        !update_journal_is_valid(journal) || journal->state != UPDATE_JOURNAL_P4_PENDING ||
        journal->generation == 0U) {
        return UPDATE_P4_WRITER_INVALID_STATE;
    }

    if (flash_coordinator_p4_ota_activate(journal) != ESP_OK) {
        return UPDATE_P4_WRITER_FLASH;
    }

    memset(writer, 0, sizeof(*writer));
    return UPDATE_P4_WRITER_OK;
}

void update_p4_writer_abort(update_p4_writer_t *writer, update_image_hash_session_t *hash)
{
    if (writer != NULL && (writer->active || writer->finished)) {
        (void)flash_coordinator_p4_ota_abort();
        memset(writer, 0, sizeof(*writer));
    }
    if (hash != NULL) {
        update_image_hash_abort(hash);
    }
}
