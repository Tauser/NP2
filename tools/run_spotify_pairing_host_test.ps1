param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/spotify-pairing-host'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$adapter=Join-Path $PSScriptRoot 'spotify_native_probe/adapter'
$main=Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/main'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $adapter -I $main `
    (Join-Path $PSScriptRoot 'spotify_pairing_host_test.c') (Join-Path $adapter 'spotify_pairing_policy.c') `
    (Join-Path $main 'google_agenda_codec.c') -o (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify pairing policy compile failed'}
& (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify pairing policy test failed'}
