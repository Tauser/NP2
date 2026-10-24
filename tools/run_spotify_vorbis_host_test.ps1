param(
    [string]$OutputDirectory='D:/Projetos/NP2/firmware/build/spotify-vorbis-host',
    [string]$Ffmpeg='C:/Users/Tauser/AppData/Local/Microsoft/WinGet/Packages/Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe/ffmpeg-8.1.1-full_build/bin/ffmpeg.exe'
)
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$repo=(Split-Path $PSScriptRoot -Parent).Replace('\','/')
$output=[IO.Path]::GetFullPath($OutputDirectory).Replace('\','/')
$adapter="$repo/tools/spotify_native_probe/adapter"
$source="$repo/firmware/build/spotify-investigation-20261022/cspot/cspot/bell/external/tremor"
$python='C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe'
New-Item -ItemType Directory -Force -Path "$output/tremor" | Out-Null
& $python "$repo/tools/spotify_native_probe/prepare_vorbis.py" $source "$output/tremor"
if($LASTEXITCODE -ne 0){throw 'Private Tremor copy failed'}
& $Ffmpeg -hide_banner -loglevel error -y -fflags +bitexact -f lavfi -i 'sine=frequency=997:sample_rate=44100:duration=3' -ac 2 -af 'volume=0.2' -c:a libvorbis -q:a 3 -flags +bitexact -map_metadata -1 "$output/tone.ogg"
if($LASTEXITCODE -ne 0){throw 'Local stereo fixture generation failed'}
& $Ffmpeg -hide_banner -loglevel error -y -fflags +bitexact -f lavfi -i 'sine=frequency=997:sample_rate=48000:duration=1' -ac 1 -c:a libvorbis -q:a 3 -flags +bitexact -map_metadata -1 "$output/wrong-format.ogg"
if($LASTEXITCODE -ne 0){throw 'Local wrong-format fixture generation failed'}
$cmake=@'
cmake_minimum_required(VERSION 3.20)
project(np_spotify_vorbis_test C)
file(GLOB codec "@OUTPUT@/tremor/*.c")
list(FILTER codec EXCLUDE REGEX "ivorbisfile_example\\.c$")
add_library(codec STATIC ${codec})
target_include_directories(codec PUBLIC "@OUTPUT@/tremor" "@ADAPTER@")
target_compile_options(codec PRIVATE -O2 -Wno-unused-parameter)
add_executable(test "@REPO@/tools/spotify_vorbis_host_test.c" "@ADAPTER@/spotify_vorbis_guard.c")
target_compile_definitions(test PRIVATE NP_SPOTIFY_VORBIS_TEST=1)
target_compile_options(test PRIVATE -O2 -Wall -Wextra -Werror)
target_link_libraries(test PRIVATE codec m)
'@
[IO.File]::WriteAllText("$output/CMakeLists.txt",$cmake.Replace('@OUTPUT@',$output).Replace('@ADAPTER@',$adapter).Replace('@REPO@',$repo))
$cmakeExe='C:/Espressif/tools/cmake/4.0.3/bin/cmake.exe'
& $cmakeExe -S $output -B "$output/build" -G Ninja -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_MAKE_PROGRAM=C:/Espressif/tools/ninja/1.12.1/ninja.exe *> "$output/build.log"
if($LASTEXITCODE -ne 0){throw "Vorbis host configure failed: $output/build.log"}
& $cmakeExe --build "$output/build" --parallel 8 *> "$output/build.log"
if($LASTEXITCODE -ne 0){throw "Vorbis host compilation failed: $output/build.log"}
& "$output/build/test.exe" "$output/tone.ogg" "$output/wrong-format.ogg" "$output/decoded.pcm" "$output/metrics.json"
if($LASTEXITCODE -ne 0){throw "Vorbis host test failed: $LASTEXITCODE"}
& $Ffmpeg -hide_banner -loglevel error -y -i "$output/tone.ogg" -f s16le -acodec pcm_s16le "$output/reference.pcm"
if($LASTEXITCODE -ne 0){throw 'Reference PCM generation failed'}
& $python "$repo/tools/compare_spotify_pcm.py" $output
if($LASTEXITCODE -ne 0){throw 'PCM reference comparison failed'}
