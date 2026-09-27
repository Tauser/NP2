$ErrorActionPreference = 'Stop'; $repo = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $env:TEMP 'np2-update-replay-policy-test.exe'
& gcc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -I (Join-Path $repo 'firmware/main') (Join-Path $PSScriptRoot 'update_replay_policy_host_test.c') (Join-Path $repo 'firmware/main/update_replay_policy.c') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Replay policy host test compilation failed' }; & $exe
if ($LASTEXITCODE -ne 0) { throw 'Replay policy host test failed' }
