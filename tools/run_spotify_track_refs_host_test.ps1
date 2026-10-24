param([string]$InvestigationDirectory='D:/Projetos/NP2/firmware/build/spotify-investigation-20261022',
      [string]$PlaybackBuild='D:/Projetos/NP2/firmware/build/spotify-playback-20261023')
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$adapter=Join-Path $PSScriptRoot 'spotify_native_probe/adapter'
$source=Join-Path $InvestigationDirectory 'cspot/cspot'
$output=Join-Path $InvestigationDirectory 'track-refs-host'
$nanopb=Join-Path $source 'bell/external/nanopb'
$proto=Join-Path $PlaybackBuild 'esp-idf/main/cspot'
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach($generator in @('prepare_compat.py','prepare_runtime.py')){
    & 'C:/Espressif/tools/python/v5.5.4/venv/Scripts/python.exe' (Join-Path $PSScriptRoot "spotify_native_probe/$generator") $source $output
    if($LASTEXITCODE -ne 0){throw 'Spotify compatibility generation failed'}
}
# Keep a distinct allocator type on host too, so a wrong std::string/vector
# callback cast cannot pass just because the default host alias is identical.
$allocator=[IO.File]::ReadAllText((Join-Path $adapter 'SpotifyAudioAllocator.h'))
$anchor='template<class T> using SpotifyAudioAllocator = std::allocator<T>;'
if(!$allocator.Contains($anchor)){throw 'Spotify allocator fixture anchor changed'}
$replacement=@'
template<class T> struct SpotifyAudioAllocator : std::allocator<T> {
    using value_type=T;
    template<class U> struct rebind { using other=SpotifyAudioAllocator<U>; };
    SpotifyAudioAllocator()=default;
    template<class U> SpotifyAudioAllocator(const SpotifyAudioAllocator<U>&) {}
    template<class U> bool operator==(const SpotifyAudioAllocator<U>&) const { return true; }
};
'@
[IO.File]::WriteAllText((Join-Path $output 'SpotifyAudioAllocator.h'),$allocator.Replace($anchor,$replacement))
$objects=@()
foreach($file in @("$nanopb/pb_common.c","$nanopb/pb_encode.c","$nanopb/pb_decode.c","$proto/protobuf/spirc.pb.c")){
    $object=Join-Path $output ((Split-Path $file -Leaf)+'.o');$objects+=$object
    & gcc -std=c11 -O2 -DPB_ENABLE_MALLOC -I $nanopb -I $proto -c $file -o $object
    if($LASTEXITCODE -ne 0){throw 'Spotify protobuf fixture compile failed'}
}
& g++ -std=c++20 -O2 -Wall -Wextra -Werror -Wno-sign-compare -Wno-unused-parameter -DPB_ENABLE_MALLOC `
    -I $output -I $adapter -I $proto -I $nanopb -I "$source/bell/main/utilities/include" -I "$source/bell/main/platform/win32" `
    (Join-Path $PSScriptRoot 'spotify_track_refs_host_test.cpp') "$output/TrackReference.cpp" "$source/src/Utils.cpp" `
    "$source/bell/main/utilities/NanoPBExtensions.cpp" @objects -lws2_32 -o "$output/test.exe"
if($LASTEXITCODE -ne 0){throw 'Spotify media references test compile failed'}
& "$output/test.exe"
if($LASTEXITCODE -ne 0){throw 'Spotify media references test failed'}
