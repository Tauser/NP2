param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-update-https-policy-test'))
$ErrorActionPreference = 'Stop'; $repo = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$exe = Join-Path $OutputDirectory 'update_https_policy_host_test.exe'
& gcc -std=c11 -Wall -Wextra -Werror -pedantic -I (Join-Path $repo 'firmware/main') (Join-Path $PSScriptRoot 'update_https_policy_host_test.c') (Join-Path $repo 'firmware/main/update_https_policy.c') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'HTTPS policy host test compilation failed' }; & $exe
if ($LASTEXITCODE -ne 0) { throw 'HTTPS policy host test failed' }
