# Builds the laboratory decode bench (ADR-121) in its own ignored build directory.
param(
    [Parameter(Mandatory)][string]$Sample,
    [string]$BuildDirectory='D:/Projetos/NP2/firmware/build/h264-decode-bench'
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$build=[IO.Path]::GetFullPath($BuildDirectory).Replace('\','/')
$samplePath=(Resolve-Path -LiteralPath $Sample).Path.Replace('\','/')
Remove-Item Env:MSYSTEM -ErrorAction SilentlyContinue
$env:IDF_PATH='C:\esp\v5.5.4\esp-idf'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\tools\python\v5.5.4\venv'
$env:ESP_IDF_VERSION='5.5'
$env:ESP_ROM_ELF_DIR='C:/Espressif/tools/esp-rom-elfs/20241011'
$env:PATH="C:\Espressif\tools\python\v5.5.4\venv\Scripts;C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;C:\Espressif\tools\esp-clang\esp-20.1.1_20250829\esp-clang\bin;"+$env:PATH
$env:CMAKE_BUILD_PARALLEL_LEVEL='8'
$python='C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$arguments=@('-DIDF_TARGET=esp32p4',"-DSDKCONFIG=$build/sdkconfig",'-DNP2_H264_DECODE_BENCH=ON',
    "-DNP2_H264_BENCH_SAMPLE=$samplePath",'-DNP2_SPOTIFY_PAIRING_BENCH=OFF','-DNP2_SPOTIFY_PLAYBACK_BENCH=OFF',
    '-DNP2_MUSIC_AUDIO_BENCH=OFF','-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-B',$build)
Push-Location (Join-Path $repo 'firmware')
try {
    & $python "$env:IDF_PATH/tools/idf.py" @arguments build *> "$build/build.log"
    # idf.py refuses an MSYS environment with exit code 0, so require the image too.
    if($LASTEXITCODE -ne 0 -or -not (Test-Path "$build/np2_p4.bin")){throw "Decode bench build failed: $build/build.log"}
    Write-Output "PASS: decode bench built in $build"
} finally {Pop-Location}
