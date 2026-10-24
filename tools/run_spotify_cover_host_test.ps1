param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/spotify-cover-host'),
      [string]$FixturePython='python')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$main=Join-Path $repo 'firmware/main'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
New-Item -ItemType Directory -Force -Path (Join-Path $OutputDirectory 'freertos') | Out-Null
'#include "platform.h"' | Set-Content (Join-Path $OutputDirectory 'freertos/FreeRTOS.h')
& $FixturePython (Join-Path $PSScriptRoot 'spotify_cover_fixtures.py') $OutputDirectory
if($LASTEXITCODE -ne 0){throw 'JPEG fixtures require Pillow'}
& $FixturePython (Join-Path $PSScriptRoot 'prepare_spotify_cover_decoder.py') `
    (Join-Path $repo 'firmware/managed_components/lvgl__lvgl/src/libs/tjpgd') (Join-Path $OutputDirectory 'decoder')
if($LASTEXITCODE -ne 0){throw 'Pinned JPEG decoder preparation failed'}
& gcc -std=gnu11 -pthread -O1 -g -Wall -Wextra -Werror -I $OutputDirectory `
    -I (Join-Path $repo 'tools/spotify_native_probe/host_shim') -I $main -I (Join-Path $OutputDirectory 'decoder') `
    (Join-Path $PSScriptRoot 'spotify_cover_host_test.c') (Join-Path $main 'spotify_cover.c') `
    (Join-Path $OutputDirectory 'decoder/np2_tjpgd.c') -o (Join-Path $OutputDirectory 'cover.exe')
if($LASTEXITCODE -ne 0){throw 'Cover host compilation failed'}
& (Join-Path $OutputDirectory 'cover.exe') (Join-Path $OutputDirectory 'cover64.jpg') `
    (Join-Path $OutputDirectory 'cover301.jpg') (Join-Path $OutputDirectory 'cover-progressive.jpg') (Join-Path $OutputDirectory 'cover300.jpg') (Join-Path $OutputDirectory 'cover300-gradient.jpg')
if($LASTEXITCODE -ne 0){throw 'Cover host test failed'}
