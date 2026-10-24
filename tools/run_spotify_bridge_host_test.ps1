param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/spotify-bridge-host'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$repo=Split-Path $PSScriptRoot -Parent
$main=Join-Path $repo 'firmware/main'
$shim=Join-Path $repo 'tools/spotify_native_probe/host_shim'
New-Item -ItemType Directory -Force -Path (Join-Path $OutputDirectory 'freertos') | Out-Null
foreach($header in @('esp_err.h','esp_crt_bundle.h','esp_heap_caps.h','esp_http_client.h','esp_timer.h','freertos/FreeRTOS.h','freertos/semphr.h','freertos/task.h')) {
    '#include "platform.h"' | Set-Content -LiteralPath (Join-Path $OutputDirectory $header)
}
& gcc -std=c11 -D_POSIX_C_SOURCE=200809L -pthread -O2 -Wall -Wextra -Werror -I $OutputDirectory -I $shim -I $main `
    (Join-Path $PSScriptRoot 'spotify_bridge_host_test.c') (Join-Path $main 'spotify_http_bridge.c') `
    (Join-Path $main 'spotify_http_policy.c') -o (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify bridge compilation failed'}
& (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify bridge test failed'}
