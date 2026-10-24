param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/music-audio-host'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$repo=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
& 'C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe' (Join-Path $PSScriptRoot 'generate_music_fir.py') --check
if($LASTEXITCODE -ne 0){throw 'Music FIR stale'}
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'music_pcm_host_test.c') (Join-Path $repo 'firmware/main/music_pcm.c') `
    -lm -o (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Music PCM host compilation failed'}
& (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Music PCM host test failed'}
$shim=Join-Path $PSScriptRoot 'spotify_native_probe/host_shim'
New-Item -ItemType Directory -Force -Path (Join-Path $OutputDirectory 'freertos') | Out-Null
foreach($header in @('esp_err.h','esp_heap_caps.h','freertos/FreeRTOS.h')) {
    '#include "platform.h"' | Set-Content -LiteralPath (Join-Path $OutputDirectory $header)
}
& gcc -std=c11 -pthread -O2 -Wall -Wextra -Werror -I $OutputDirectory -I $shim -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'music_audio_service_host_test.c') (Join-Path $repo 'firmware/main/music_audio_service.c') `
    (Join-Path $repo 'firmware/main/music_pcm.c') -o (Join-Path $OutputDirectory 'service.exe')
if($LASTEXITCODE -ne 0){throw 'Music service host compilation failed'}
& (Join-Path $OutputDirectory 'service.exe')
if($LASTEXITCODE -ne 0){throw 'Music service host test failed'}
