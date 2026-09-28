param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-settings-policy-test'))
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$executable = Join-Path $OutputDirectory 'settings_policy_host_test.exe'
& gcc -std=c11 -O2 -Wall -Wextra -Werror (Join-Path $PSScriptRoot 'settings_policy_host_test.c') -o $executable
if ($LASTEXITCODE -ne 0) { throw 'Settings policy compilation failed' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'Settings policy test failed' }
