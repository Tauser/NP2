param([string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'firmware/build/assistant-ui-host-20261026'))
$ErrorActionPreference = 'Stop'
$env:PATH = 'C:/msys64/ucrt64/bin;' + $env:PATH
$repo = (Split-Path $PSScriptRoot -Parent).Replace('\','/')
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$dir = $OutputDirectory.Replace('\','/')
function Write-ChangedText([string]$Path, [string]$Text) {
    if (!(Test-Path -LiteralPath $Path) -or [IO.File]::ReadAllText($Path) -ne $Text) {
        [IO.File]::WriteAllText($Path, $Text)
    }
}
Write-ChangedText (Join-Path $OutputDirectory 'lv_conf.h') @'
#pragma once
#define LV_CONF_H
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE
#define LV_MEM_SIZE (192U * 1024U)
#define LV_USE_LOG 0
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_THORVG_EXTERNAL 0
'@
Write-ChangedText (Join-Path $OutputDirectory 'esp_err.h') @'
#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
'@
$project = @'
cmake_minimum_required(VERSION 3.20)
project(np2_assistant_ui_test C CXX)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_PATH "@DIR@/lv_conf.h" CACHE PATH "" FORCE)
add_subdirectory("@REPO@/firmware/managed_components/lvgl__lvgl" lvgl)
file(GLOB fonts "@REPO@/firmware/main/ui/fonts/ui_font_*.c")
add_executable(assistant_ui_test "@REPO@/tools/assistant_ui_host_test.c"
    "@REPO@/firmware/main/ui/screens/np_assistant.c"
    "@REPO@/firmware/main/ui/assets/np_btc_tilted_icon.c"
    "@REPO@/firmware/main/ui/core/np_components.c"
    "@REPO@/firmware/main/ui/core/np_form.c"
    "@REPO@/firmware/main/ui/core/np_styles.c" ${fonts})
target_include_directories(assistant_ui_test PRIVATE "@DIR@" "@REPO@/firmware/main"
    "@REPO@/firmware/main/ui/core")
target_compile_definitions(assistant_ui_test PRIVATE NP2_PRODUCT_FONTS)
target_compile_options(assistant_ui_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(assistant_ui_test PRIVATE lvgl m)
'@
Write-ChangedText (Join-Path $OutputDirectory 'CMakeLists.txt') ($project.Replace('@REPO@',$repo).Replace('@DIR@',$dir))
$cmake = 'C:/Espressif/tools/cmake/4.0.3/bin/cmake.exe'
& $cmake -S $OutputDirectory -B (Join-Path $OutputDirectory 'build') -G Ninja `
    -DCMAKE_MAKE_PROGRAM=C:/Espressif/tools/ninja/1.12.1/ninja.exe `
    -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
if ($LASTEXITCODE -ne 0) { throw 'Assistant UI host configuration failed' }
& $cmake --build (Join-Path $OutputDirectory 'build') --parallel 8
if ($LASTEXITCODE -ne 0) { throw 'Assistant UI host build failed' }
& (Join-Path $OutputDirectory 'build/assistant_ui_test.exe') (Join-Path $OutputDirectory 'assistant.rgb565')
if ($LASTEXITCODE -ne 0) { throw 'Assistant UI host test failed' }
