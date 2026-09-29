param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-notification-preferences-test'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$shim = Join-Path $OutputDirectory 'shim'
New-Item -ItemType Directory -Force -Path $shim, (Join-Path $shim 'freertos') | Out-Null
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
#define ESP_ERR_NOT_FOUND 0x105
'@
    'esp_partition.h' = "#pragma once`ntypedef struct esp_partition_t esp_partition_t;"
    'freertos/FreeRTOS.h' = @'
#pragma once
#include <stdint.h>
typedef int portMUX_TYPE;
typedef int BaseType_t;
typedef void *TaskHandle_t;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(ms) (ms)
#define pdTRUE 1
#define pdPASS 1
'@
    'freertos/task.h' = @'
#pragma once
#include "FreeRTOS.h"
static inline BaseType_t xTaskCreate(void (*fn)(void *), const char *name,
    unsigned stack, void *arg, unsigned priority, TaskHandle_t *task) {
    (void)fn; (void)name; (void)stack; (void)arg; (void)priority;
    *task = (void *)1; return pdPASS;
}
static inline void xTaskNotifyGive(TaskHandle_t task) { (void)task; }
static inline uint32_t ulTaskNotifyTake(BaseType_t clear, unsigned ticks) {
    (void)clear; (void)ticks; return 0;
}
'@
}
foreach ($entry in $headers.GetEnumerator()) {
    [IO.File]::WriteAllText((Join-Path $shim $entry.Key), $entry.Value)
}
$executable = Join-Path $OutputDirectory 'notification_preferences_host_test.exe'
& gcc -std=c11 -O2 -Wall -Wextra -Werror -fwhole-program -ffunction-sections -fdata-sections `
    -I $shim -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'notification_preferences_host_test.c') `
    '-Wl,--gc-sections' -o $executable
if ($LASTEXITCODE -ne 0) { throw 'Notification preferences host compilation failed' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'Notification preferences host test failed' }
