param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/xiaozhi-host'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$repo=Split-Path $PSScriptRoot -Parent
$main=Join-Path $repo 'firmware/main'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
& gcc -std=c11 -O2 -Wall -Wextra -Werror -D_USE_MATH_DEFINES -I $main `
    (Join-Path $PSScriptRoot 'xiaozhi_host_test.c') (Join-Path $main 'xiaozhi_protocol.c') `
    (Join-Path $main 'voice_resampler.c') (Join-Path $main 'google_agenda_codec.c') `
    -lm -o (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Xiaozhi host compilation failed'}
& (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Xiaozhi host test failed'}
