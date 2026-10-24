param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/spotify-remote-host'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$repo=Split-Path $PSScriptRoot -Parent
$main=Join-Path $repo 'firmware/main'
$shim=Join-Path $repo 'tools/spotify_native_probe/host_shim'
New-Item -ItemType Directory -Force -Path (Join-Path $OutputDirectory 'freertos') | Out-Null
foreach($header in @('esp_err.h','esp_crt_bundle.h','esp_heap_caps.h','esp_http_client.h','esp_timer.h','esp_log.h','freertos/FreeRTOS.h')) {
    '#include "platform.h"' | Set-Content -LiteralPath (Join-Path $OutputDirectory $header)
}
@'
#include "platform.h"
typedef int esp_http_client_method_t;
#define HTTP_METHOD_PUT 2
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_LOGI(tag,...) do { (void)(tag); if(0) printf(__VA_ARGS__); } while(0)
'@ | Set-Content -LiteralPath (Join-Path $OutputDirectory 'extra.h')
& 'C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe' (Join-Path $PSScriptRoot 'prepare_spotify_cover_decoder.py') `
    (Join-Path $repo 'firmware/managed_components/lvgl__lvgl/src/libs/tjpgd') (Join-Path $OutputDirectory 'decoder')
if($LASTEXITCODE -ne 0){throw 'Cover decoder preparation failed'}
& gcc -std=gnu11 -pthread -O1 -g -Wall -Wextra -Werror -include (Join-Path $OutputDirectory 'extra.h') -I $OutputDirectory -I $shim -I $main -I (Join-Path $OutputDirectory 'decoder') `
    (Join-Path $PSScriptRoot 'spotify_remote_host_test.c') (Join-Path $main 'google_agenda_codec.c') (Join-Path $main 'spotify_cover.c') (Join-Path $OutputDirectory 'decoder/np2_tjpgd.c') -o (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify remote compilation failed'}
& (Join-Path $OutputDirectory 'test.exe') (Join-Path $PSScriptRoot 'data/spotify-cover-red64.jpg') (Join-Path $PSScriptRoot 'data/spotify-cover-red300.jpg')
if($LASTEXITCODE -ne 0){throw 'Spotify remote test failed'}

$vaultShim=Join-Path $repo 'firmware/build/google-agenda-host/shim'
if(!(Test-Path (Join-Path $vaultShim 'nvs.h'))){throw 'Run tools/run_google_agenda_sync_host_test.ps1 once to prepare its NVS shim'}
& gcc -std=gnu11 -DNP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=1 -O1 -g -Wall -Wextra -Werror -I $vaultShim -I $main `
    (Join-Path $PSScriptRoot 'spotify_remote_credentials_host_test.c') (Join-Path $main 'google_agenda_codec.c') `
    (Join-Path $main 'cache_record.c') -o (Join-Path $OutputDirectory 'vault.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify vault compilation failed'}
& (Join-Path $OutputDirectory 'vault.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify vault test failed'}
