param([string]$OutputDirectory='D:/Projetos/NP2/firmware/build/spotify-pcm-sink-host')
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$main=Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/main'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $main (Join-Path $PSScriptRoot 'spotify_pcm_sink_host_test.c') `
    (Join-Path $main 'spotify_pcm_sink.c') (Join-Path $main 'music_pcm.c') -o (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify PCM sink compile failed'}
& (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify PCM sink test failed'}
