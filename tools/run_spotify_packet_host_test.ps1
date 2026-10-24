param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/spotify-packet-host'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$adapter=Join-Path $PSScriptRoot 'spotify_native_probe/adapter'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
foreach($source in @('spotify_packet_policy','spotify_ap_parse')) {
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -I $adapter -c (Join-Path $adapter "$source.c") -o (Join-Path $OutputDirectory "$source.o")
    if($LASTEXITCODE -ne 0){throw 'Spotify packet policy compile failed'}
}
& g++ -std=c++20 -O2 -pthread -Wall -Wextra -Werror -I $adapter (Join-Path $PSScriptRoot 'spotify_packet_host_test.cpp') `
    (Join-Path $OutputDirectory 'spotify_packet_policy.o') (Join-Path $OutputDirectory 'spotify_ap_parse.o') -o (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify packet test compile failed'}
& (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify packet test failed'}
