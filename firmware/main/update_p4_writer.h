#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_partition.h"
#include "update_image_hash.h"
#include "update_journal.h"
typedef struct { const esp_partition_t *partition; bool active; bool finished; } update_p4_writer_t;
typedef enum { UPDATE_P4_WRITER_OK=0, UPDATE_P4_WRITER_INVALID_ARGUMENT, UPDATE_P4_WRITER_INVALID_STATE, UPDATE_P4_WRITER_HASH, UPDATE_P4_WRITER_FLASH } update_p4_writer_result_t;
update_p4_writer_result_t update_p4_writer_begin(update_p4_writer_t *writer, update_image_hash_session_t *hash);
update_p4_writer_result_t update_p4_writer_append(update_p4_writer_t *writer, update_image_hash_session_t *hash, const uint8_t *bytes, size_t bytes_count);
update_p4_writer_result_t update_p4_writer_finish(update_p4_writer_t *writer, update_image_hash_session_t *hash);
update_p4_writer_result_t update_p4_writer_request_activation(
    update_p4_writer_t *writer, const update_journal_record_t *journal);
void update_p4_writer_abort(update_p4_writer_t *writer, update_image_hash_session_t *hash);
