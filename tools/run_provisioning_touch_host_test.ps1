param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-provisioning-touch-test'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$shim = Join-Path $OutputDirectory 'shim'
New-Item -ItemType Directory -Force -Path $shim, (Join-Path $shim 'freertos'), (Join-Path $shim 'driver') | Out-Null
$headers = @{
    'esp_err.h' = @'
#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_NO_MEM 0x101
const char *esp_err_to_name(esp_err_t);
'@
    'esp_partition.h' = "#pragma once`ntypedef struct esp_partition_t esp_partition_t;"
    'esp_timer.h' = "#pragma once`n#include <stdint.h>`nstatic inline int64_t esp_timer_get_time(void) { return 0; }"
    'esp_log.h' = @'
#pragma once
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
'@
    'freertos/FreeRTOS.h' = @'
#pragma once
#include <stddef.h>
typedef int portMUX_TYPE;
typedef int BaseType_t;
#define portMUX_INITIALIZER_UNLOCKED 0
#define taskENTER_CRITICAL(p) ((void)(p))
#define taskEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(ms) (ms)
#define pdPASS 1
'@
    'freertos/task.h' = @'
#pragma once
void vTaskDelay(unsigned);
BaseType_t xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, void *);
'@
    'driver/usb_serial_jtag.h' = @'
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
typedef struct { int unused; } usb_serial_jtag_driver_config_t;
#define USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT() ((usb_serial_jtag_driver_config_t){0})
int usb_serial_jtag_read_bytes(void *, size_t, unsigned);
int usb_serial_jtag_write_bytes(const void *, size_t, unsigned);
esp_err_t usb_serial_jtag_wait_tx_done(unsigned);
bool usb_serial_jtag_is_driver_installed(void);
esp_err_t usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t *);
esp_err_t usb_serial_jtag_driver_uninstall(void);
'@
}
foreach ($entry in $headers.GetEnumerator()) {
    [IO.File]::WriteAllText((Join-Path $shim $entry.Key), $entry.Value)
}
$executable = Join-Path $OutputDirectory 'provisioning_touch_host_test.exe'
& gcc -std=c11 -O2 -Wall -Wextra -Werror -fwhole-program -ffunction-sections -fdata-sections `
    -I $shim -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'provisioning_touch_host_test.c') `
    '-Wl,--gc-sections' -o $executable
if ($LASTEXITCODE -ne 0) { throw 'Provisioning touch host compilation failed' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'Provisioning touch host test failed' }
