cmake_minimum_required(VERSION 3.16)

# Preserve the pinned example's common defaults as well as its C6 profile.
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/upstream/sdkconfig.defaults;${CMAKE_CURRENT_LIST_DIR}/upstream/sdkconfig.defaults.esp32c6;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(eh_cp_ota_coprocessor_ota)

if(NOT IDF_TARGET STREQUAL "esp32c6")
    message(FATAL_ERROR "NP2 coprocessor requires explicit IDF_TARGET=esp32c6")
endif()
