param([string]$OutputDirectory=(Join-Path $env:TEMP 'np2-agenda-test'))
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$exe=Join-Path $OutputDirectory 'agenda_host_test.exe'
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'agenda_host_test.c') `
    (Join-Path $repo 'firmware/main/agenda_service.c') `
    (Join-Path $repo 'firmware/main/google_agenda_codec.c') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Agenda host compilation failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Agenda host test failed' }
