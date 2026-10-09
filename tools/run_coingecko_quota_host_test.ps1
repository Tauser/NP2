$ErrorActionPreference = 'Stop'
$np2Root = Split-Path $PSScriptRoot -Parent
$np2TestDir = Join-Path $np2Root 'firmware/build/quota-host-tests'
New-Item -ItemType Directory -Path $np2TestDir -Force | Out-Null
foreach ($np2Test in @('coingecko_quota', 'data_refresh_scheduler')) {
    $np2Exe = Join-Path $np2TestDir "$np2Test.exe"
    $np2Sources = @((Join-Path $PSScriptRoot "${np2Test}_host_test.c"), (Join-Path $np2Root "firmware/main/$np2Test.c"))
    if ($np2Test -eq 'coingecko_quota') { $np2Sources += Join-Path $np2Root 'firmware/main/cache_record.c' }
    & gcc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -I (Join-Path $np2Root 'firmware/main') @np2Sources -o $np2Exe
    if ($LASTEXITCODE -ne 0) { throw "$np2Test compile failed" }
    & $np2Exe
    if ($LASTEXITCODE -ne 0) { throw "$np2Test failed" }
}
