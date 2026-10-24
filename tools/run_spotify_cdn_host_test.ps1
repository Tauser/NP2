param([string]$InvestigationDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/spotify-investigation-20261022'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$repo=Split-Path $PSScriptRoot -Parent
$main=Join-Path $repo 'firmware/main'
$adapter=Join-Path $PSScriptRoot 'spotify_native_probe/adapter'
$shim=Join-Path $PSScriptRoot 'spotify_native_probe/host_shim'
$output=Join-Path $InvestigationDirectory 'cdn-host'
$source=Join-Path $InvestigationDirectory 'cspot/cspot'
New-Item -ItemType Directory -Force -Path $output | Out-Null
& 'C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe' (Join-Path $PSScriptRoot 'spotify_native_probe/prepare_compat.py') $source $output
if($LASTEXITCODE -ne 0){throw 'Spotify compatibility generation failed'}
& 'C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe' (Join-Path $PSScriptRoot 'spotify_native_probe/prepare_runtime.py') $source $output
if($LASTEXITCODE -ne 0){throw 'Spotify runtime compatibility generation failed'}
# Copy only the header for a host identity-cipher fixture. Runtime candidate
# keeps the original Crypto header and mbedTLS; no source checkout is edited.
$header=Get-Content -Raw (Join-Path $output 'CDNAudioFile.h')
if(!$header.Contains('#include "Crypto.h"')){throw 'CDNAudioFile header anchor changed'}
$header.Replace('#include "Crypto.h"','#include "fake_crypto.h"') | Set-Content -LiteralPath (Join-Path $output 'CDNAudioFile.h')
'#include "fake_crypto.h"' | Set-Content -LiteralPath (Join-Path $output 'Crypto.h')
foreach($fixture in @('Utils.h','Logger.h')) {
    Copy-Item -LiteralPath (Join-Path $shim $fixture) -Destination (Join-Path $output $fixture) -Force
}
foreach($unused in @('AccessKeyFetcher.h','BellLogger.h','Packet.h','SocketStream.h','WrappedSemaphore.h','cJSON.h')){
    '#pragma once' | Set-Content -LiteralPath (Join-Path $output $unused)
}
& gcc -std=c11 -Wall -Wextra -Werror -I $main -c (Join-Path $main 'spotify_http_policy.c') -o (Join-Path $output 'policy.o')
if($LASTEXITCODE -ne 0){throw 'Spotify policy compilation failed'}
& g++ -std=c++20 -DBELL_ONLY_CJSON -O2 -Wall -Wextra -Werror -I $output -I $main -I $adapter -I $shim `
    (Join-Path $PSScriptRoot 'spotify_cdn_host_test.cpp') (Join-Path $output 'CDNAudioFile.cpp') `
    (Join-Path $adapter 'HTTPClient.cpp') (Join-Path $output 'policy.o') -o (Join-Path $output 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify CDN host compilation failed'}
& (Join-Path $output 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify CDN host test failed'}
