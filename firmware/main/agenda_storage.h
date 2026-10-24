#pragma once
#include "agenda_service.h"
#include "esp_err.h"

/* All disk methods are called exclusively by FlashCoordinator. */
esp_err_t agenda_storage_owner_load(void);
esp_err_t agenda_storage_owner_write(const agenda_data_t *data);
/* Cache-only format is allowed only after proving agenda records absent. */
bool agenda_storage_owner_format_allowed(void);
/* Nonblocking RAM copy; no filesystem access. */
esp_err_t agenda_storage_copy(agenda_data_t *out);
