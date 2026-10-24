param([string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/sounds-host-20261021'))
$ErrorActionPreference='Stop'
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
$repo=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'sound_melody_host_test.c') (Join-Path $repo 'firmware/main/sound_melody.c') `
    -o (Join-Path $OutputDirectory 'sound_melody_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Sound synthesis compilation failed' }
& (Join-Path $OutputDirectory 'sound_melody_test.exe') $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw 'Sound synthesis test failed' }
$source=Get-Content -Raw (Join-Path $repo 'firmware/main/device_control_service.c')
function Part([string]$Start,[string]$End) {
    $a=$source.IndexOf($Start);$b=$source.IndexOf($End,$a)
    if ($a -lt 0 -or $b -le $a) { throw "Missing production section: $Start" }
    $source.Substring($a,$b-$a)
}
$sections=@((Part 'static esp_err_t codec_result(' 'static esp_err_t apply_brightness('),
    (Part 'static bool play_pending_sound_chunk(' 'static void finish_save_if_complete('),
    (Part 'esp_err_t device_control_play_melody(' 'void device_control_get_status('))
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'sound_queue_under_test.inc'),($sections -join "`n"))
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'esp_err.h'),"#pragma once`ntypedef int esp_err_t;`n#define ESP_OK 0`n#define ESP_FAIL -1`n#define ESP_ERR_INVALID_STATE 0x103`n#define ESP_ERR_INVALID_ARG 0x102`n")
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $OutputDirectory -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'sound_queue_host_test.c') (Join-Path $repo 'firmware/main/sound_melody.c') `
    -o (Join-Path $OutputDirectory 'sound_queue_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Sound owner compilation failed' }
& (Join-Path $OutputDirectory 'sound_queue_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Sound owner test failed' }
$source=Get-Content -Raw (Join-Path $repo 'firmware/main/flash_coordinator.c')
$a=$source.IndexOf('} notification_profile_record_t;')
$first=$source.LastIndexOf('typedef struct {',$a)
$last=$source.IndexOf('} user_profile_record_t;',$a)
$last=$source.LastIndexOf('typedef struct {',$last)
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'sound_storage_under_test.inc'),$source.Substring($first,$last-$first))
& gcc -std=c11 -O2 -Wall -Wextra -Werror -I $OutputDirectory -I (Join-Path $repo 'firmware/main') `
    (Join-Path $PSScriptRoot 'sound_storage_host_test.c') (Join-Path $repo 'firmware/main/sound_melody.c') `
    (Join-Path $repo 'firmware/main/cache_record.c') -o (Join-Path $OutputDirectory 'sound_storage_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Sound storage compilation failed' }
& (Join-Path $OutputDirectory 'sound_storage_test.exe')
if ($LASTEXITCODE -ne 0) { throw 'Sound storage test failed' }
