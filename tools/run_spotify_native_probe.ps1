param(
    [string]$InvestigationDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/spotify-investigation-20261022'),
    [string]$BuildDirectory = '',
    [string]$IDFPath = 'C:/esp/v5.5.4/esp-idf',
    [string]$IDFPython = 'C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe'
)
$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot 'spotify_native_probe'
$investigation = [IO.Path]::GetFullPath($InvestigationDirectory).Replace('\','/')
$lock = Get-Content -Raw (Join-Path $project 'source-lock.json') | ConvertFrom-Json
foreach ($repository in $lock.repositories) {
    $checkout = Join-Path $investigation $repository.path
    if (!(Test-Path -LiteralPath $checkout)) { throw "Missing pinned checkout: $checkout" }
    $revision = & git -C $checkout rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $revision.Trim() -ne $repository.commit) {
        throw "Unexpected source revision: $checkout"
    }
    & git -C $checkout diff --quiet HEAD --
    if ($LASTEXITCODE -ne 0) { throw "Pinned source has local modifications: $checkout" }
}
$generator = Join-Path $investigation 'proto-env/Scripts'
if (!(Test-Path -LiteralPath (Join-Path $generator 'python.exe'))) {
    throw 'Prepare the isolated protobuf generator described in spotify_native_probe/README.md'
}
$env:IDF_PATH = $IDFPath.Replace('/','\')
$env:IDF_TOOLS_PATH = 'C:\Espressif\tools'
$env:IDF_PYTHON_ENV_PATH = (Split-Path (Split-Path $IDFPython -Parent) -Parent).Replace('/','\')
$env:ESP_IDF_VERSION = '5.5'
$env:ESP_ROM_ELF_DIR = 'C:/Espressif/tools/esp-rom-elfs/20241011'
$env:PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION = 'python'
$env:PATH = "$generator;C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;" + $env:PATH
$build = if ($BuildDirectory) { [IO.Path]::GetFullPath($BuildDirectory).Replace('\','/') } else { "$investigation/build" }
$arguments = @('-DIDF_TARGET=esp32p4', "-DNP2_CSPOT_DIR=$investigation/cspot",
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.5', "-DSDKCONFIG=$build/sdkconfig", '-B', $build, 'reconfigure')
Push-Location $project
try {
    & $IDFPython "$IDFPath/tools/idf.py" @arguments *> "$investigation/configure.log"
    if ($LASTEXITCODE -ne 0) { throw "Probe configure failed: $investigation/configure.log" }
    & 'C:/Espressif/tools/cmake/4.0.3/bin/cmake.exe' --build $build --target cspot *> "$investigation/compile.log"
    if ($LASTEXITCODE -ne 0) { throw "Probe compile failed: $investigation/compile.log" }
    Write-Output 'PASS: cspot/Bell libraries compile for ESP32-P4. No player image was flashed; playback/authentication are untested.'
} finally { Pop-Location }
