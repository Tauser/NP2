/* Fixed-width encoding for the offline snapshot stored inside cache_record. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "offline_data_model.h"

#define OFFLINE_DATA_V1_ENCODED_SIZE 25U
#define OFFLINE_DATA_V2_ENCODED_SIZE 34U
#define OFFLINE_DATA_V3_ENCODED_SIZE 34U
#define OFFLINE_DATA_V4_ENCODED_SIZE 169U
#define OFFLINE_DATA_V5_ENCODED_SIZE 275U
#define OFFLINE_DATA_V6_ENCODED_SIZE 341U
#define OFFLINE_DATA_V6_FIELDS_SIZE 342U
#define OFFLINE_DATA_ENCODED_SIZE 364U

bool offline_data_snapshot_is_valid(const offline_data_snapshot_t *snapshot);
bool offline_data_snapshot_encode(const offline_data_snapshot_t *snapshot, uint8_t *out_bytes,
                                  size_t out_size);
bool offline_data_snapshot_decode(const uint8_t *bytes, size_t size,
                                  offline_data_snapshot_t *out_snapshot);
