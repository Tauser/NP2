param([string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/network-buffers-host'))
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$repo = Split-Path $PSScriptRoot -Parent
$source = Get-Content -Raw (Join-Path $repo 'firmware/main/network_validation_service.c')
function SourceRange([string]$Start, [string]$End) {
    $first = $source.IndexOf($Start, [StringComparison]::Ordinal)
    if ($first -lt 0) { throw "Production routine missing: $Start" }
    $last = $source.IndexOf($End, $first + $Start.Length, [StringComparison]::Ordinal)
    if ($last -lt 0) { throw "Production boundary missing: $End" }
    return $source.Substring($first, $last - $first)
}
$parts = @(
    (SourceRange 'static esp_err_t stream_update_image(const' 'static esp_err_t persist_update_journal_and_wait'),
    (SourceRange 'static esp_err_t refresh_product_domain(data_refresh_domain_t' 'static esp_err_t refresh_all_product_domains')
)
$parts -join "`n" | Set-Content -LiteralPath (Join-Path $OutputDirectory 'network_buffers_under_test.inc')
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $OutputDirectory (Join-Path $PSScriptRoot 'network_transient_buffers_host_test.c') -o (Join-Path $OutputDirectory 'network_buffers_host_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Network buffer host compilation failed' }
& (Join-Path $OutputDirectory 'network_buffers_host_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Network buffer host test failed' }
