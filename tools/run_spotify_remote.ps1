param([string]$BuildDirectory='D:/Projetos/NP2/firmware/build/spotify-remote-20261024', [switch]$Flash, [string]$Port='COM8')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$build=[IO.Path]::GetFullPath($BuildDirectory).Replace('\','/')
$env:IDF_PATH='C:\esp\v5.5.4\esp-idf'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\tools\python\v5.5.4\venv'
$env:ESP_IDF_VERSION='5.5'
$env:ESP_ROM_ELF_DIR='C:/Espressif/tools/esp-rom-elfs/20241011'
$env:PATH="C:\Espressif\tools\python\v5.5.4\venv\Scripts;C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;"+$env:PATH
$env:CMAKE_BUILD_PARALLEL_LEVEL='8'
$python='C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$arguments=@('-DIDF_TARGET=esp32p4',"-DSDKCONFIG=$build/sdkconfig",'-DNP2_SPOTIFY_PAIRING_BENCH=OFF',
    '-DNP2_SPOTIFY_PLAYBACK_BENCH=OFF','-DNP2_MUSIC_AUDIO_BENCH=OFF','-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-B',$build)
Push-Location (Join-Path $repo 'firmware')
try {
    & $python "$env:IDF_PATH/tools/idf.py" @arguments build *> "$build/build.log"
    if($LASTEXITCODE -ne 0){throw "Remote build failed: $build/build.log"}
    Write-Output 'PASS: P4 remote firmware built with native playback/pairing OFF.'
    if($Flash) {
        & $python "$env:IDF_PATH/tools/idf.py" @arguments -p $Port -b 460800 app-flash *> "$build/flash.log"
        if($LASTEXITCODE -ne 0){throw "Remote app-flash failed: $build/flash.log"}
        Write-Output 'PASS: P4 application flashed; C6, NVS and partition table preserved.'
    }
} finally {Pop-Location}
