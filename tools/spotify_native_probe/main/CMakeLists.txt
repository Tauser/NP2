idf_component_register(SRCS "probe_main.c" INCLUDE_DIRS "."
    REQUIRES mbedtls pthread lwip json espressif__mdns esp_driver_i2s)
include("${CMAKE_CURRENT_LIST_DIR}/../native_libraries.cmake")
target_link_libraries(${COMPONENT_LIB} PRIVATE cspot)
idf_component_get_property(np2_http_parser_lib http_parser COMPONENT_LIB)
target_compile_definitions(${np2_http_parser_lib} PRIVATE HTTP_MAX_HEADER_SIZE=8192)
