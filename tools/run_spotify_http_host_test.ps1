param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/spotify-http-host'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$repo=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$adapter=Join-Path $repo 'tools/spotify_native_probe/adapter'
$main=Join-Path $repo 'firmware/main'
& gcc -std=c11 -Wall -Wextra -Werror -I $main -c (Join-Path $main 'spotify_http_policy.c') -o (Join-Path $OutputDirectory 'policy.o')
if($LASTEXITCODE -ne 0){throw 'Spotify policy compilation failed'}
& g++ -std=c++20 -Wall -Wextra -Werror -I $main -I $adapter (Join-Path $PSScriptRoot 'spotify_http_host_test.cpp') `
    (Join-Path $adapter 'HTTPClient.cpp') (Join-Path $OutputDirectory 'policy.o') -o (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify HTTP adapter compilation failed'}
& (Join-Path $OutputDirectory 'test.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify HTTP adapter test failed'}
$parser='C:/esp/v5.5.4/esp-idf/components/http_parser'
& gcc -std=c11 -DHTTP_MAX_HEADER_SIZE=8192 -Wall -Wextra -Werror -I $parser `
    (Join-Path $PSScriptRoot 'spotify_http_parser_host_test.c') (Join-Path $parser 'http_parser.c') `
    -o (Join-Path $OutputDirectory 'parser.exe')
if($LASTEXITCODE -ne 0){throw 'IDF HTTP parser compilation failed'}
& (Join-Path $OutputDirectory 'parser.exe')
if($LASTEXITCODE -ne 0){throw 'IDF HTTP parser limit test failed'}
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $main -I $adapter `
    (Join-Path $PSScriptRoot 'spotify_response_host_test.c') (Join-Path $adapter 'spotify_response_codec.c') `
    (Join-Path $main 'google_agenda_codec.c') (Join-Path $main 'spotify_http_policy.c') `
    -o (Join-Path $OutputDirectory 'response.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify response codec compilation failed'}
& (Join-Path $OutputDirectory 'response.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify response codec test failed'}
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $adapter `
    (Join-Path $PSScriptRoot 'spotify_login_host_test.c') (Join-Path $adapter 'spotify_login_policy.c') `
    -o (Join-Path $OutputDirectory 'login.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify login policy compilation failed'}
& (Join-Path $OutputDirectory 'login.exe')
if($LASTEXITCODE -ne 0){throw 'Spotify login policy test failed'}
