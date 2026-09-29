param([string]$OutputDirectory = (Join-Path $env:TEMP 'np2-settings-wifi-ui-test'))
$ErrorActionPreference = 'Stop'
function Write-ChangedText([string]$Path, [string]$Text) {
    if (!(Test-Path -LiteralPath $Path) -or [IO.File]::ReadAllText($Path) -ne $Text) {
        [IO.File]::WriteAllText($Path, $Text)
    }
}
$repo = (Split-Path $PSScriptRoot -Parent).Replace('\','/')
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$dir = $OutputDirectory.Replace('\','/')
Write-ChangedText (Join-Path $OutputDirectory 'esp_err.h') "#pragma once`ntypedef int esp_err_t;"
Write-ChangedText (Join-Path $OutputDirectory 'lv_conf.h') @'
#pragma once
#define LV_CONF_H
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE
#define LV_MEM_SIZE (2U * 1024U * 1024U)
#define LV_USE_LOG 0
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_THORVG_EXTERNAL 0
'@
$project = @'
cmake_minimum_required(VERSION 3.20)
project(np2_wifi_ui_test C CXX)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_PATH "@DIR@/lv_conf.h" CACHE PATH "" FORCE)
add_subdirectory("@REPO@/firmware/managed_components/lvgl__lvgl" lvgl)
file(GLOB fonts "@REPO@/firmware/main/ui/fonts/ui_font_*.c")
add_executable(wifi_ui_test "@REPO@/tools/settings_wifi_ui_host_test.c"
    "@REPO@/firmware/main/ui/screens/settings/np_settings_wifi.c"
    "@REPO@/firmware/main/ui/screens/settings/np_wifi_password.c"
    "@REPO@/firmware/main/ui/core/np_components.c"
    "@REPO@/firmware/main/ui/core/np_styles.c"
    "@REPO@/firmware/main/ui/core/np_form.c"
    "@REPO@/firmware/main/ui/core/np_modal.c"
    "@REPO@/firmware/main/ui/core/np_keyboard.c" ${fonts})
target_include_directories(wifi_ui_test PRIVATE "@DIR@" "@REPO@/firmware/main"
    "@REPO@/firmware/main/ui/core")
target_compile_definitions(wifi_ui_test PRIVATE NP2_PRODUCT_FONTS)
target_compile_options(wifi_ui_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(wifi_ui_test PRIVATE lvgl m)
'@
$project = $project.Replace('@REPO@', $repo).Replace('@DIR@', $dir)
Write-ChangedText (Join-Path $OutputDirectory 'CMakeLists.txt') $project
$cmake = 'C:/Espressif/tools/cmake/4.0.3/bin/cmake.exe'
& $cmake -S $OutputDirectory -B (Join-Path $OutputDirectory 'build') -G Ninja `
    -DCMAKE_MAKE_PROGRAM=C:/Espressif/tools/ninja/1.12.1/ninja.exe `
    -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
if ($LASTEXITCODE -ne 0) { throw 'Wi-Fi UI host configuration failed' }
& $cmake --build (Join-Path $OutputDirectory 'build') --parallel 8
if ($LASTEXITCODE -ne 0) { throw 'Wi-Fi UI host build failed' }
& (Join-Path $OutputDirectory 'build/wifi_ui_test.exe') (Join-Path $OutputDirectory 'wifi.rgb565')
if ($LASTEXITCODE -ne 0) { throw 'Wi-Fi UI host test failed' }
