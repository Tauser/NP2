[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$DestinationRoot
)

$ErrorActionPreference = 'Stop'
$sourceDirectory = Join-Path $PSScriptRoot '..\design\clima\generated-160'
$targetRoot = (Resolve-Path -LiteralPath $DestinationRoot).Path
$volumeRoot = [System.IO.Path]::GetPathRoot($targetRoot)
if ($targetRoot.TrimEnd('\') -ne $volumeRoot.TrimEnd('\') -or
    [System.IO.DriveInfo]::new($volumeRoot).DriveType -ne [System.IO.DriveType]::Removable) {
    throw 'DestinationRoot deve ser a raiz de uma unidade removível (por exemplo, F:\). Nada foi copiado.'
}
$weatherDirectory = Join-Path $targetRoot 'np2\weather'
$targetDirectory = Join-Path $weatherDirectory 'icons'
$stagingDirectory = Join-Path $weatherDirectory 'icons-160-staging'
$backupDirectory = Join-Path $weatherDirectory 'icons-96-backup'
$volumePrefix = [System.IO.Path]::GetFullPath($volumeRoot).TrimEnd('\') + '\'
foreach ($path in @($weatherDirectory, $targetDirectory, $stagingDirectory, $backupDirectory)) {
    $resolvedPath = [System.IO.Path]::GetFullPath($path)
    if (-not $resolvedPath.StartsWith($volumePrefix,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Caminho fora da unidade removível: $resolvedPath. Nada foi copiado."
    }
}

$names = @(
    'clear', 'partly_cloudy', 'fog', 'drizzle', 'rain', 'snow',
    'rain_showers', 'snow_showers', 'thunderstorm', 'variable'
)
$expected = foreach ($period in @('day', 'night')) {
    foreach ($name in $names) { "np_icon_${period}_${name}.bin" }
}
$totalBytes = 0L
foreach ($name in $expected) {
    $source = Join-Path $sourceDirectory $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Pacote ausente: $source. Execute tools/build_weather_icons.py primeiro."
    }
    $stream = [System.IO.File]::OpenRead($source)
    try {
        $header = New-Object byte[] 24
        if ($stream.Read($header, 0, 24) -ne 24 -or
            [System.Text.Encoding]::ASCII.GetString($header, 0, 4) -ne 'NPWI' -or
            [BitConverter]::ToUInt16($header, 4) -ne 1 -or
            [BitConverter]::ToUInt16($header, 6) -ne 160 -or
            [BitConverter]::ToUInt16($header, 8) -ne 160) {
            throw "Cabeçalho inválido: $source"
        }
        $frames = [BitConverter]::ToUInt16($header, 10)
        $bytes = [BitConverter]::ToUInt32($header, 16)
        if ($frames -lt 1 -or $frames -gt 48 -or
            $bytes -ne ($frames * 160 * 160 * 4) -or
            $stream.Length -ne (24 + $bytes)) {
            throw "Tamanho inválido: $source"
        }
        $totalBytes += $stream.Length
    } finally {
        $stream.Dispose()
    }
}

if (Test-Path -LiteralPath $stagingDirectory) {
    throw "A pasta de staging já existe: $stagingDirectory. Nada foi copiado."
}
if (Test-Path -LiteralPath $backupDirectory) {
    throw "A pasta de backup já existe: $backupDirectory. Nada foi copiado."
}
if ([System.IO.DriveInfo]::new($volumeRoot).AvailableFreeSpace -lt ($totalBytes + 1048576L)) {
    throw "Espaço insuficiente no cartão para staging de $totalBytes bytes. Nada foi copiado."
}

New-Item -ItemType Directory -Path $weatherDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $stagingDirectory | Out-Null
foreach ($name in $expected) {
    $source = Join-Path $sourceDirectory $name
    $staged = Join-Path $stagingDirectory $name
    Copy-Item -LiteralPath $source -Destination $staged
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $staged -Algorithm SHA256).Hash) {
        throw "Cópia divergente: $name. Os ícones antigos continuam em $targetDirectory."
    }
}

if (Test-Path -LiteralPath $targetDirectory) {
    Move-Item -LiteralPath $targetDirectory -Destination $backupDirectory
}
try {
    Move-Item -LiteralPath $stagingDirectory -Destination $targetDirectory
} catch {
    if ((Test-Path -LiteralPath $backupDirectory) -and
        -not (Test-Path -LiteralPath $targetDirectory)) {
        Move-Item -LiteralPath $backupDirectory -Destination $targetDirectory
    }
    throw
}
Write-Host "20 pacotes nativos de 160 px copiados e verificados em $targetDirectory"
if (Test-Path -LiteralPath $backupDirectory) {
    Write-Host "Pacotes anteriores preservados em $backupDirectory"
}
