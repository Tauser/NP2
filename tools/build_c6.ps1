param(
    [Parameter(Mandatory = $true)][string]$ReferenceSource,
    [string]$BuildName = ('c6-' + (Get-Date -Format 'yyyyMMdd-HHmmss')),
    [string]$HostedPath
)
$ErrorActionPreference = 'Stop'
if ($BuildName -notmatch '^[a-zA-Z0-9_-]+$') { throw 'BuildName must be a simple directory name' }
if (-not $env:IDF_PATH) { throw 'Run in an ESP-IDF 5.5.4 terminal' }
$repo = Split-Path $PSScriptRoot -Parent
$project = Join-Path $repo 'coprocessor'
if (-not $HostedPath) {
    $HostedPath = Join-Path $repo 'firmware/managed_components/espressif__esp_hosted'
}
$build = Join-Path $project "build/$BuildName"
if (Test-Path -LiteralPath $build) { throw 'Use a new BuildName for a clean build; existing evidence is preserved' }
$reference = (Resolve-Path -LiteralPath $ReferenceSource).Path
$idfVersion = & python (Join-Path $env:IDF_PATH 'tools/idf.py') --version
if ($LASTEXITCODE -ne 0) { throw 'IDF Python unavailable' }
if ($idfVersion.Trim() -notmatch '^ESP-IDF v5\.5\.4(-dirty)?$') { throw 'Expected exact ESP-IDF 5.5.4' }
Write-Output $idfVersion
& python (Join-Path $PSScriptRoot 'audit_c6_idf_patch.py') `
    --idf-path $env:IDF_PATH --hosted-path $HostedPath --reference $reference `
    --output (Join-Path $build 'audit')
if ($LASTEXITCODE -ne 0) { throw 'C6 SDIO patch gate failed' }
Push-Location $project
try {
    & python (Join-Path $env:IDF_PATH 'tools/idf.py') -B $build `
        -D "SDKCONFIG=$build/sdkconfig" -D IDF_TARGET=esp32c6 build *> "$build/build.log"
    if ($LASTEXITCODE -ne 0) {
        Get-Content "$build/build.log" -Tail 45
        throw 'C6 build failed; inspect preserved build.log'
    }
    & python (Join-Path $env:IDF_PATH 'tools/idf.py') -B $build size *> "$build/size.log"
    if ($LASTEXITCODE -ne 0) { throw 'C6 size report failed' }
    & python (Join-Path $PSScriptRoot 'verify_c6_build.py') --build $build
    if ($LASTEXITCODE -ne 0) { throw 'C6 effective configuration/artifact gate failed' }
    Get-Content "$build/build.log" -Tail 15
} finally { Pop-Location }
# Deliberately no serial flash command: this USB belongs to the P4.
