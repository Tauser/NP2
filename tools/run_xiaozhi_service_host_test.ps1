param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/xiaozhi-service-host'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$repo=Split-Path $PSScriptRoot -Parent
$main=Join-Path $repo 'firmware/main'
$shim=Join-Path $repo 'tools/spotify_native_probe/host_shim'
New-Item -ItemType Directory -Force -Path (Join-Path $OutputDirectory 'freertos') | Out-Null
foreach($header in @('esp_err.h','esp_heap_caps.h','freertos/FreeRTOS.h')) {
    '#include "platform.h"' | Set-Content -LiteralPath (Join-Path $OutputDirectory $header)
}
# Compile the actual executor reservation/completion functions, not a copy of
# the scheduling policy. The surrounding transport/platform is provided by fakes.
$network=[IO.File]::ReadAllText((Join-Path $main 'network_validation_service.c'))
$completion=[regex]::Match($network,'(?s)static bool s_xiaozhi_active;.*?(?=static void network_validation_task)')
$reservation=[regex]::Match($network,'(?s)static esp_err_t request_spotify_remote\(bool interactive\).*?(?=esp_err_t network_validation_service_request_xiaozhi)')
if(!$completion.Success -or !$reservation.Success){throw 'Executor test seams not found'}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'xiaozhi_executor_under_test.inc'),$completion.Value+$reservation.Value)
$controls=[IO.File]::ReadAllText((Join-Path $main 'device_control_service.c'))
$capture=[regex]::Match($controls,'(?s)static bool capture_microphone_chunk\(void\).*?(?=static void wake_audio_owner)')
if(!$capture.Success){throw 'Microphone owner seam not found'}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'capture_microphone_under_test.inc'),$capture.Value)
& gcc -std=gnu11 -pthread -O1 -g -Wall -Wextra -Werror -I $OutputDirectory -I $shim -I $PSScriptRoot -I $main `
    (Join-Path $PSScriptRoot 'xiaozhi_service_host_test.c') (Join-Path $main 'xiaozhi_protocol.c') `
    (Join-Path $main 'voice_resampler.c') (Join-Path $main 'voice_capture.c') `
    (Join-Path $main 'music_audio_service.c') (Join-Path $main 'music_pcm.c') (Join-Path $main 'google_agenda_codec.c') `
    -lm -o (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Xiaozhi service host compilation failed'}
& (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Xiaozhi service host test failed'}
@'
#pragma once
#include "platform.h"
typedef void *esp_codec_dev_handle_t;
#define ESP_CODEC_DEV_OK 0
int esp_codec_dev_read(esp_codec_dev_handle_t,void *,int);
'@ | Set-Content -LiteralPath (Join-Path $OutputDirectory 'esp_codec_dev.h')
& gcc -std=gnu11 -pthread -O1 -g -Wall -Wextra -Werror -I $OutputDirectory -I $shim -I $PSScriptRoot -I $main `
    (Join-Path $PSScriptRoot 'audio_capture_io_host_test.c') -o (Join-Path $OutputDirectory 'capture.exe')
if($LASTEXITCODE -ne 0){throw 'Microphone adapter compilation failed'}
& (Join-Path $OutputDirectory 'capture.exe')
if($LASTEXITCODE -ne 0){throw 'Microphone adapter test failed'}
