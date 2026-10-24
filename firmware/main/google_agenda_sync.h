#pragma once
#include <stddef.h>
#include "agenda_service.h"
#include "esp_err.h"

/* Private OAuth handoff. Never placed in events/projections or logged. */
typedef struct {
    char client_id[193];
    char client_secret[97];
    char refresh_token[513];
    char subject[65];
    uint8_t reserved;
} google_agenda_credentials_t;

bool google_agenda_linked(void);
esp_err_t google_agenda_request_sync(const agenda_data_t *data);
bool google_agenda_take_result(agenda_data_t *out_data, esp_err_t *out_result, bool *out_changed);
/* Called exclusively on the existing HTTPS worker, after trusted time. */
esp_err_t google_agenda_process_sync(void);
void google_agenda_fail_sync(esp_err_t result);
/* Called by the physically armed USB provisioner, with bounded JSON. */
esp_err_t google_agenda_provision_json(const char *json, size_t length);
/* Vault I/O called exclusively by FlashCoordinator. */
esp_err_t google_agenda_credentials_owner_load(void);
esp_err_t google_agenda_credentials_owner_write(const google_agenda_credentials_t *credentials);
