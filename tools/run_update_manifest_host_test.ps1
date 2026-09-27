param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-update-manifest-test'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$executable = Join-Path $OutputDirectory 'update_manifest_host_test.exe'
& gcc -std=c11 -O2 -Wall -Wextra -Werror -pedantic `
    -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'update_manifest_host_test.c') `
    (Join-Path $repo 'firmware/main/update_manifest.c') `
    (Join-Path $repo 'firmware/main/update_policy.c') -o $executable
if ($LASTEXITCODE -ne 0) { throw 'Update manifest host test compilation failed' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'Update manifest host test failed' }
