param(
    [string]$BuildDirectory='D:/Projetos/NP2/firmware/build/spotify-pairing-20261023',
    [string]$InvestigationDirectory='D:/Projetos/NP2/firmware/build/spotify-investigation-20261022',
    [switch]$Flash,
    [switch]$Playback,
    [string]$Port='COM8'
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$build=[IO.Path]::GetFullPath($BuildDirectory).Replace('\','/')
$investigation=[IO.Path]::GetFullPath($InvestigationDirectory).Replace('\','/')
$env:IDF_PATH='C:\esp\v5.5.4\esp-idf'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\tools\python\v5.5.4\venv'
$env:ESP_IDF_VERSION='5.5'
$env:ESP_ROM_ELF_DIR='C:/Espressif/tools/esp-rom-elfs/20241011'
$env:PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION='python'
$env:PATH="$investigation/proto-env/Scripts;C:\Espressif\tools\python\v5.5.4\venv\Scripts;C:\Espressif\tools\cmake\4.0.3\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;"+$env:PATH
$env:CMAKE_BUILD_PARALLEL_LEVEL='8'
$python='C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe'
& $python (Join-Path $PSScriptRoot 'spotify_native_probe/verify_sources.py') "$investigation/cspot"
if($LASTEXITCODE -ne 0){throw 'Spotify source pins rejected'}
New-Item -ItemType Directory -Force -Path $build | Out-Null
if(!(Test-Path -LiteralPath "$build/sdkconfig")) {
    $baseline=Join-Path $repo 'firmware/build/music-owner-bench-20261022/sdkconfig'
    $text=[IO.File]::ReadAllText($baseline)
    if(!$text.Contains('# CONFIG_COMPILER_CXX_EXCEPTIONS is not set')){throw 'Unexpected baseline exception setting'}
    $text=$text.Replace('# CONFIG_COMPILER_CXX_EXCEPTIONS is not set','CONFIG_COMPILER_CXX_EXCEPTIONS=y')
    $text += "`nCONFIG_COMPILER_CXX_EXCEPTIONS_EMG_POOL_SIZE=2048`n"
    [IO.File]::WriteAllText("$build/sdkconfig",$text)
}
$arguments=@('-DIDF_TARGET=esp32p4',"-DSDKCONFIG=$build/sdkconfig",'-DNP2_SPOTIFY_PAIRING_BENCH=ON',
    '-DNP2_MUSIC_AUDIO_BENCH=OFF',"-DNP2_CSPOT_DIR=$investigation/cspot",'-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-B',$build)
$playbackSetting=if($Playback){'ON'}else{'OFF'}
$arguments += "-DNP2_SPOTIFY_PLAYBACK_BENCH=$playbackSetting"
Push-Location (Join-Path $repo 'firmware')
try {
    & $python "$env:IDF_PATH/tools/idf.py" @arguments build *> "$build/build.log"
    if($LASTEXITCODE -ne 0){throw "Pairing bench build failed: $build/build.log"}
    Write-Output "PASS: complete P4 laboratory firmware built; experimental playback=$playbackSetting."
    if($Flash) {
        & $python "$env:IDF_PATH/tools/idf.py" @arguments -p $Port -b 460800 app-flash *> "$build/flash.log"
        if($LASTEXITCODE -ne 0){throw "Pairing bench app-flash failed: $build/flash.log"}
        Write-Output 'PASS: P4 application flashed and verified. C6, NVS and partition table unchanged.'
    }
} finally {Pop-Location}
