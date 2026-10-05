idf_component_register(INCLUDE_DIRS "." REQUIRES pthread)

# edge264's portable vector backend uses Clang builtins unsupported by GCC.
# Compile only this third-party translation unit with the pinned ESP Clang tool.
find_program(EDGE264_CLANG NAMES clang clang.exe REQUIRED)
get_filename_component(EDGE264_GCC_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
get_filename_component(EDGE264_GCC_ROOT "${EDGE264_GCC_BIN}" DIRECTORY)
set(EDGE264_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/edge264.o")
file(GLOB EDGE264_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/*.c")
add_custom_command(
    OUTPUT "${EDGE264_OBJECT}"
    COMMAND "${EDGE264_CLANG}" --target=riscv32-esp-elf
            "--gcc-toolchain=${EDGE264_GCC_ROOT}"
            -march=rv32imafc_zicsr_zifencei_xesppie -mabi=ilp32f
            -O2 -std=gnu17 -ffunction-sections -fdata-sections
            -Wno-initializer-overrides
            -DCLOCK_PROCESS_CPUTIME_ID=CLOCK_MONOTONIC
            "-I${IDF_PATH}/components/pthread/include"
            -c "${CMAKE_CURRENT_LIST_DIR}/src/edge264.c"
            -o "${EDGE264_OBJECT}"
    DEPENDS ${EDGE264_SOURCES} "${CMAKE_CURRENT_LIST_DIR}/src/edge264_internal.h"
            "${CMAKE_CURRENT_LIST_DIR}/edge264.h"
    VERBATIM
)
add_custom_target(edge264_build DEPENDS "${EDGE264_OBJECT}")
set_source_files_properties("${EDGE264_OBJECT}" PROPERTIES
                            GENERATED TRUE EXTERNAL_OBJECT TRUE)
target_sources(${COMPONENT_LIB} PRIVATE "${EDGE264_OBJECT}")
add_dependencies(${COMPONENT_LIB} edge264_build)
