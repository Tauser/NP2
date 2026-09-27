param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-update-recovery-policy-test'))

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$executable = Join-Path $OutputDirectory 'update_recovery_policy_host_test.exe'
& gcc -std=c11 -Wall -Wextra -Werror -pedantic -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'update_recovery_policy_host_test.c') `
    (Join-Path $repo 'firmware/main/update_recovery_policy.c') -o $executable
if ($LASTEXITCODE -ne 0) { throw 'Update recovery policy host test compilation failed' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'Update recovery policy host test failed' }
