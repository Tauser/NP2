[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$DestinationRoot
)

$ErrorActionPreference = 'Stop'
$sourceDirectory = Join-Path $PSScriptRoot '..\firmware\main\ui\assets\weather'
$targetDirectory = Join-Path $DestinationRoot 'np2\weather'
$expectedBytes = 560 * 376 * 2
$expectedNames = @(
    'np_bg_day_clear.bin', 'np_bg_day_partly_cloudy.bin', 'np_bg_day_fog.bin',
    'np_bg_day_drizzle.bin', 'np_bg_day_rain.bin', 'np_bg_day_snow.bin',
    'np_bg_day_rain_showers.bin', 'np_bg_day_snow_showers.bin',
    'np_bg_day_thunderstorm.bin', 'np_bg_day_variable.bin',
    'np_bg_night_clear.bin', 'np_bg_night_partly_cloudy.bin', 'np_bg_night_fog.bin',
    'np_bg_night_drizzle.bin', 'np_bg_night_rain.bin', 'np_bg_night_snow.bin',
    'np_bg_night_rain_showers.bin', 'np_bg_night_snow_showers.bin',
    'np_bg_night_thunderstorm.bin', 'np_bg_night_variable.bin'
)

foreach ($name in $expectedNames) {
    $source = Join-Path $sourceDirectory $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Asset ausente: $source"
    }
    if ((Get-Item -LiteralPath $source).Length -ne $expectedBytes) {
        throw "Background incompatível com a Home atual: $source tem $((Get-Item -LiteralPath $source).Length) bytes; esperado: $expectedBytes (560 x 376 RGB565). Nada foi copiado para o SD."
    }
}

New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
foreach ($name in $expectedNames) {
    Copy-Item -LiteralPath (Join-Path $sourceDirectory $name) -Destination (Join-Path $targetDirectory $name) -Force
}

Write-Host "20 assets RGB565 copiados para $targetDirectory"
