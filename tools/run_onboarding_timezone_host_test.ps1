param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-onboarding-timezone-test'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$shim = Join-Path $OutputDirectory 'shim'
New-Item -ItemType Directory -Force -Path $shim, (Join-Path $shim 'freertos') | Out-Null
$headers = @{
    'esp_err.h' = @'
#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_TIMEOUT 0x107
'@
    'esp_partition.h' = "#pragma once`ntypedef struct esp_partition_t esp_partition_t;"
    'freertos/FreeRTOS.h' = @'
#pragma once
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define taskENTER_CRITICAL(p) ((void)(p))
#define taskEXIT_CRITICAL(p) ((void)(p))
'@
}
foreach ($entry in $headers.GetEnumerator()) {
    [IO.File]::WriteAllText((Join-Path $shim $entry.Key), $entry.Value)
}
$executable = Join-Path $OutputDirectory 'onboarding_timezone_host_test.exe'
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $shim -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'onboarding_timezone_host_test.c') -o $executable
if ($LASTEXITCODE -ne 0) { throw 'Timezone host compilation failed' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'Timezone host test failed' }
