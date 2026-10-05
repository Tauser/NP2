param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-event-bus-test'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$shim = Join-Path $OutputDirectory 'shim'
New-Item -ItemType Directory -Force -Path $shim, (Join-Path $shim 'freertos'), (Join-Path $shim 'lwip') | Out-Null
$headers = @{
    'esp_err.h' = @'
#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_NO_MEM 0x101
'@
    'esp_log.h' = @'
#pragma once
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
'@
    'freertos/FreeRTOS.h' = @'
#pragma once
#include <windows.h>
#include <stdint.h>
typedef SRWLOCK portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED SRWLOCK_INIT
#define portENTER_CRITICAL(p) AcquireSRWLockExclusive(p)
#define portEXIT_CRITICAL(p) ReleaseSRWLockExclusive(p)
#define pdMS_TO_TICKS(ms) (ms)
#define pdPASS 1
'@
    'freertos/queue.h' = @'
#pragma once
#include <stddef.h>
typedef struct test_queue *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned count, size_t item_size);
int xQueueSend(QueueHandle_t queue, const void *item, unsigned ticks);
int xQueueReceive(QueueHandle_t queue, void *item, unsigned ticks);
'@
    'lwip/sockets.h' = @'
#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
'@
}
foreach ($entry in $headers.GetEnumerator()) {
    [IO.File]::WriteAllText((Join-Path $shim $entry.Key), $entry.Value)
}
# Compile the actual delivery functions, with only their I/O mocked. This
# preserves the production implementation rather than duplicating its policy.
$network = [IO.File]::ReadAllText((Join-Path $repo 'firmware/main/network_validation_service.c'))
$delivery = [regex]::Match($network, '(?s)static void publish_product_snapshot\(void\).*?(?=/\* This runs only in the sole HTTPS worker)')
if (!$delivery.Success) { throw 'Production delivery functions not found' }
if ($network -notmatch 'for \(;;\) \{\s*retry_pending_product_delivery\(\);') {
    throw 'Delivery retry is not wired into the worker loop'
}
[IO.File]::WriteAllText((Join-Path $shim 'product_delivery_under_test.inc'), $delivery.Value)
$executable = Join-Path $OutputDirectory 'app_event_bus_host_test.exe'
& gcc -std=c11 -D_WIN32_WINNT=0x0600 -DWIN32_LEAN_AND_MEAN -O2 -Wall -Wextra -Werror `
    -I $shim -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'app_event_bus_host_test.c') `
    (Join-Path $repo 'firmware/main/offline_data_codec.c') `
    -lws2_32 -o $executable
if ($LASTEXITCODE -ne 0) { throw 'Event bus host compilation failed' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'Event bus host test failed' }
