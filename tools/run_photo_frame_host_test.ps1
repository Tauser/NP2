param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/photo-host-20261022'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$repo=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path $OutputDirectory,(Join-Path $OutputDirectory 'freertos'),(Join-Path $OutputDirectory 'bsp') | Out-Null
$headers=@{
'esp_err.h'=@'
#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC 0x109
'@
'esp_heap_caps.h'=@'
#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1U
#define MALLOC_CAP_INTERNAL 2U
#define MALLOC_CAP_8BIT 4U
void *heap_caps_aligned_alloc(size_t,size_t,unsigned);
size_t heap_caps_get_free_size(unsigned);
'@
'esp_log.h'=@'
#pragma once
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
'@
'bsp/esp32_p4_wifi6_touch_lcd_7b.h'="#pragma once`n#define BSP_SD_MOUNT_POINT `".`"`n"
'freertos/FreeRTOS.h'=@'
#pragma once
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(ms) (ms)
'@
'freertos/task.h'="#pragma once`n#include `"FreeRTOS.h`"`nTickType_t xTaskGetTickCount(void);`n"
'lvgl.h'=@'
#pragma once
#include <stdint.h>
#define LV_DRAW_BUF_ALIGN 64
#define LV_IMAGE_HEADER_MAGIC 0x19
#define LV_COLOR_FORMAT_RGB565 1
typedef struct { uint32_t magic,cf,w,h,stride; } lv_image_header_t;
typedef struct { lv_image_header_t header;uint32_t data_size;const uint8_t *data; } lv_image_dsc_t;
'@
}
foreach($key in $headers.Keys){[IO.File]::WriteAllText((Join-Path $OutputDirectory $key),$headers[$key])}
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $OutputDirectory -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'photo_frame_host_test.c') (Join-Path $repo 'firmware/main/photo_frame_model.c') `
    -o (Join-Path $OutputDirectory 'photo_test.exe')
if($LASTEXITCODE -ne 0){throw 'Photo service compilation failed'}
Push-Location $OutputDirectory
try { & ./photo_test.exe; if($LASTEXITCODE -ne 0){throw 'Photo service test failed'} } finally { Pop-Location }

$source=Get-Content -Raw (Join-Path $repo 'firmware/main/flash_coordinator.c')
$a=$source.IndexOf('} device_control_profile_record_t;')
$first=$source.LastIndexOf('typedef struct {',$a)
$last=$source.IndexOf('} credential_vault_record_t;',$a)
$last=$source.LastIndexOf('typedef struct {',$last)
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'photo_storage_under_test.inc'),$source.Substring($first,$last-$first))
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $OutputDirectory -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'photo_storage_host_test.c') (Join-Path $repo 'firmware/main/cache_record.c') `
    -o (Join-Path $OutputDirectory 'photo_storage_test.exe')
if($LASTEXITCODE -ne 0){throw 'Photo storage compilation failed'}
& (Join-Path $OutputDirectory 'photo_storage_test.exe')
if($LASTEXITCODE -ne 0){throw 'Photo storage test failed'}
$ownerSource=Get-Content -Raw (Join-Path $repo 'firmware/main/device_control_service.c')
$first=$ownerSource.IndexOf('static void finish_save_if_complete(')
$last=$ownerSource.IndexOf('static void device_control_task(',$first)
$settingsFirst=$ownerSource.IndexOf('esp_err_t device_control_set_photo_settings(')
$settingsLast=$ownerSource.IndexOf('esp_err_t device_control_set_photo_config(',$settingsFirst)
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'photo_controls_under_test.inc'),$ownerSource.Substring($first,$last-$first)+$ownerSource.Substring($settingsFirst,$settingsLast-$settingsFirst))
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $OutputDirectory -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'photo_controls_host_test.c') -o (Join-Path $OutputDirectory 'photo_controls_test.exe')
if($LASTEXITCODE -ne 0){throw 'Photo control owner compilation failed'}
& (Join-Path $OutputDirectory 'photo_controls_test.exe')
if($LASTEXITCODE -ne 0){throw 'Photo control owner test failed'}
