param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/google-agenda-host'))
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$shim=Join-Path $OutputDirectory 'shim'
New-Item -ItemType Directory -Force -Path $shim,(Join-Path $shim 'freertos') | Out-Null
$headers=@{
'esp_err.h'=@'
#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_NVS_NOT_FOUND 0x1102
'@
'esp_crt_bundle.h'=@'
#pragma once
void esp_crt_bundle_attach(void);
'@
'esp_partition.h'=@'
#pragma once
typedef struct esp_partition_t esp_partition_t;
'@
'esp_flash_encrypt.h'=@'
#pragma once
'@
'nvs.h'=@'
#pragma once
#include <stddef.h>
#include "esp_err.h"
typedef int nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
esp_err_t nvs_open(const char *,int,nvs_handle_t *);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
esp_err_t nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
esp_err_t nvs_commit(nvs_handle_t);
'@
'esp_heap_caps.h'=@'
#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1U
#define MALLOC_CAP_INTERNAL 2U
#define MALLOC_CAP_8BIT 4U
void *heap_caps_calloc(size_t,size_t,unsigned);
void heap_caps_free(void *);
size_t heap_caps_get_free_size(unsigned);
size_t heap_caps_get_largest_free_block(unsigned);
'@
'esp_mac.h'=@'
#pragma once
#include <stdint.h>
#include "esp_err.h"
esp_err_t esp_efuse_mac_get_default(uint8_t *);
'@
'esp_timer.h'=@'
#pragma once
#include <stdint.h>
int64_t esp_timer_get_time(void);
'@
'freertos/FreeRTOS.h'=@'
#pragma once
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
'@
'esp_http_client.h'=@'
#pragma once
#include <stdbool.h>
#include "esp_err.h"
typedef enum { HTTP_METHOD_GET, HTTP_METHOD_POST, HTTP_METHOD_PATCH, HTTP_METHOD_DELETE } esp_http_client_method_t;
enum { HTTP_EVENT_ON_DATA=1 };
typedef struct { int event_id; void *data; int data_len; } esp_http_client_event_t;
typedef struct {
    const char *url; esp_http_client_method_t method;
    void (*crt_bundle_attach)(void); bool disable_auto_redirect;
    int max_redirection_count; esp_err_t (*event_handler)(esp_http_client_event_t *);
    int timeout_ms,buffer_size,buffer_size_tx;
} esp_http_client_config_t;
typedef void *esp_http_client_handle_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t,const char *,const char *);
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t,const char *,int);
esp_err_t esp_http_client_perform(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
void esp_http_client_cleanup(esp_http_client_handle_t);
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t,int);
esp_err_t esp_http_client_open(esp_http_client_handle_t,int);
int esp_http_client_write(esp_http_client_handle_t,const char *,int);
long long esp_http_client_fetch_headers(esp_http_client_handle_t);
int esp_http_client_read(esp_http_client_handle_t,char *,int);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t);
esp_err_t esp_http_client_close(esp_http_client_handle_t);
'@
}
foreach ($entry in $headers.GetEnumerator()) { [IO.File]::WriteAllText((Join-Path $shim $entry.Key),$entry.Value) }
$exe=Join-Path $OutputDirectory 'google_sync_test.exe'
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $shim -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'google_agenda_sync_host_test.c') `
    (Join-Path $repo 'firmware/main/agenda_service.c') (Join-Path $repo 'firmware/main/google_agenda_codec.c') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Google sync host compilation failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Google sync host test failed' }
$storageExe=Join-Path $OutputDirectory 'agenda_storage_test.exe'
& gcc -std=gnu11 -O2 -Wall -Wextra -Werror -I $shim -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'agenda_storage_host_test.c') `
    (Join-Path $repo 'firmware/main/agenda_service.c') (Join-Path $repo 'firmware/main/cache_record.c') -o $storageExe
if ($LASTEXITCODE -ne 0) { throw 'Agenda storage host compilation failed' }
$store=Join-Path $OutputDirectory 'testfs'
New-Item -ItemType Directory -Force -Path $store | Out-Null
& $storageExe $store
if ($LASTEXITCODE -ne 0) { throw 'Agenda storage host test failed' }
$vaultExe=Join-Path $OutputDirectory 'google_credentials_test.exe'
$json='C:/esp/v5.5.4/esp-idf/components/json/cJSON'
& gcc -std=gnu11 -DNP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=1 -O2 -Wall -Wextra -Werror `
    -I $shim -I (Join-Path $repo 'firmware/main') -I $json `
    (Join-Path $PSScriptRoot 'google_agenda_credentials_host_test.c') `
    (Join-Path $repo 'firmware/main/cache_record.c') (Join-Path $json 'cJSON.c') -o $vaultExe
if ($LASTEXITCODE -ne 0) { throw 'Google vault host compilation failed' }
& $vaultExe
if ($LASTEXITCODE -ne 0) { throw 'Google vault host test failed' }
$flashSource=Get-Content -Raw (Join-Path $repo 'firmware/main/flash_coordinator.c')
$first=$flashSource.IndexOf('esp_err_t flash_coordinator_request_agenda_write(')
$last=$flashSource.IndexOf('esp_err_t flash_coordinator_request_notification_profile_write(', $first)
$producer=$flashSource.Substring($first,$last-$first)
$first=$flashSource.IndexOf('        case FLASH_REQUEST_AGENDA_WRITE:')
$last=$flashSource.IndexOf('        case FLASH_REQUEST_ONBOARDING_PROFILE_WRITE:', $first)
$consumer=$flashSource.Substring($first,$last-$first)
$testSource=$producer+"`nstatic void consume_agenda_request(void) { flash_request_t request=queued; esp_err_t result; switch(request.kind) {`n"+$consumer+"`n} }`n"
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'agenda_flash_under_test.inc'),$testSource)
$flashExe=Join-Path $OutputDirectory 'agenda_flash_test.exe'
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $shim -I $OutputDirectory -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'agenda_flash_host_test.c') (Join-Path $repo 'firmware/main/agenda_service.c') -o $flashExe
if ($LASTEXITCODE -ne 0) { throw 'Agenda flash mailbox compilation failed' }
& $flashExe
if ($LASTEXITCODE -ne 0) { throw 'Agenda flash mailbox test failed' }
