param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-cache-full-probe-test'))
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$source = Get-Content -Raw (Join-Path $repo 'firmware/main/flash_coordinator.c')
function SourceRange([string]$Start, [string]$End) {
    $first = $source.IndexOf($Start, [StringComparison]::Ordinal)
    $last = $source.IndexOf($End, $first + $Start.Length, [StringComparison]::Ordinal)
    if ($first -lt 0 -or $last -lt 0) { throw "Production section missing: $Start" }
    return $source.Substring($first, $last - $first)
}
# Compile the actual production routines against the pinned LittleFS core.
# Only ESP-IDF status/clock and POSIX-to-LittleFS adapters are replaced.
$parts = @(
    (SourceRange '#define FLASH_COORDINATOR_QUEUE_LENGTH' 'typedef struct {'),
    (SourceRange 'static bool make_full_probe_proposal_path' 'static esp_err_t read_selected_offline_data'),
    (SourceRange 'static esp_err_t write_all' 'static esp_err_t select_latest_cache'),
    (SourceRange 'static esp_err_t write_cache_record_at_path' 'static esp_err_t write_cache_temp_record'),
    (SourceRange 'static esp_err_t fill_full_probe_file' 'static esp_err_t run_cache_power_cut_probe')
)
$parts -join "`n" | Set-Content -LiteralPath (Join-Path $OutputDirectory 'probe_under_test.inc')
$lfs = Join-Path $repo 'firmware/managed_components/joltwallet__littlefs/src/littlefs'
& gcc -std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function `
    -DLFS_NO_DEBUG -DLFS_NO_WARN -DLFS_NO_ERROR -I $OutputDirectory -I $lfs `
    -I (Join-Path $repo 'firmware/main') (Join-Path $PSScriptRoot 'cache_full_probe_host_test.c') `
    (Join-Path $lfs 'lfs.c') (Join-Path $lfs 'lfs_util.c') `
    (Join-Path $repo 'firmware/main/cache_record.c') `
    (Join-Path $repo 'firmware/main/offline_data_codec.c') `
    -o (Join-Path $OutputDirectory 'cache_full_probe_host_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed' }
& (Join-Path $OutputDirectory 'cache_full_probe_host_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Host test failed' }
