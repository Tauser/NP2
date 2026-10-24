# Shared, pinned foreign-library compatibility recipe. No credentials here.
set(NP2_SPOTIFY_ADAPTER_ROOT "${CMAKE_CURRENT_LIST_DIR}")
idf_build_get_property(np2_python PYTHON)
execute_process(COMMAND "${np2_python}" "${NP2_SPOTIFY_ADAPTER_ROOT}/verify_sources.py"
    "${NP2_CSPOT_DIR}" RESULT_VARIABLE np2_pin_result)
if(NOT np2_pin_result EQUAL 0)
    message(FATAL_ERROR "Spotify pinned sources validation failed")
endif()
set(BELL_DISABLE_SINKS ON CACHE BOOL "" FORCE)
set(BELL_DISABLE_MQTT ON CACHE BOOL "" FORCE)
set(BELL_DISABLE_FMT ON CACHE BOOL "" FORCE)
set(BELL_DISABLE_REGEX ON CACHE BOOL "" FORCE)
set(BELL_ONLY_CJSON ON CACHE BOOL "" FORCE)
set(BELL_EXTERNAL_CJSON idf::json CACHE STRING "" FORCE)
set(BELL_CODEC_AAC OFF CACHE BOOL "" FORCE)
set(BELL_CODEC_MP3 ON CACHE BOOL "" FORCE)
set(BELL_CODEC_ALAC OFF CACHE BOOL "" FORCE)
set(BELL_CODEC_OPUS OFF CACHE BOOL "" FORCE)
set(BELL_CODEC_VORBIS ON CACHE BOOL "" FORCE)
set(BELL_EXTERNAL_VORBIS "" CACHE STRING "Private bounded Tremor only" FORCE)
add_subdirectory("${NP2_CSPOT_DIR}/cspot" "${CMAKE_CURRENT_BINARY_DIR}/cspot")
# bell's ESP selection includes Xtensa assembly. P4 uses RISC-V; cspot does
# not require that DSP routine. No upstream files or product flags change.
get_target_property(bell_sources bell SOURCES)
list(FILTER bell_sources EXCLUDE REGEX "biquad_f32_ae32\\.S$")
# Upstream globs decoder files even when the corresponding codec is disabled.
list(FILTER bell_sources EXCLUDE REGEX "/(AAC|OPUS)Decoder\\.cpp$")
list(FILTER bell_sources EXCLUDE REGEX "/(HTTPClient|TLSSocket)\\.cpp$")
list(APPEND bell_sources
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/HTTPClient.cpp"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/TLSSocket.cpp"
    "${NP2_ROOT}/firmware/main/spotify_http_policy.c")
target_include_directories(bell BEFORE PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/compat"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter" "${NP2_ROOT}/firmware/main")
# Compile private copies with two header/signature fixes. The pinned checkout
# remains immutable; compatibility changes are reviewable in this harness.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/compat")
foreach(source URLParser BellHTTPServer)
    set(original "${NP2_CSPOT_DIR}/cspot/bell/main/io/${source}.cpp")
    file(READ "${original}" body)
    if(source STREQUAL "URLParser")
        string(PREPEND body "#include <cstring>\n")
    else()
        string(REPLACE "handleClose(CivetServer* server, struct mg_connection* conn)"
            "handleClose(CivetServer* server, const struct mg_connection* conn)" body "${body}")
        # Civet's close callback is const; bell's legacy callback typedef is
        # not. This only bridges that typedef, never writes to the connection.
        string(REPLACE "stateHandler(conn, BellHTTPServer::WSState::CLOSED);"
            "stateHandler(const_cast<struct mg_connection*>(conn), BellHTTPServer::WSState::CLOSED);"
            body "${body}")
    endif()
    set(patched "${CMAKE_CURRENT_BINARY_DIR}/compat/${source}.cpp")
    file(WRITE "${patched}" "${body}")
    list(REMOVE_ITEM bell_sources "${original}")
    list(APPEND bell_sources "${patched}")
endforeach()
set_property(TARGET bell PROPERTY SOURCES "${bell_sources}")
idf_build_get_property(np2_python PYTHON)
execute_process(COMMAND "${np2_python}"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/prepare_compat.py" "${NP2_CSPOT_DIR}/cspot"
    "${CMAKE_CURRENT_BINARY_DIR}/compat" RESULT_VARIABLE np2_patch_result)
if(NOT np2_patch_result EQUAL 0)
    message(FATAL_ERROR "Spotify bounded compatibility copy generation failed")
endif()
execute_process(COMMAND "${np2_python}" "${NP2_SPOTIFY_ADAPTER_ROOT}/prepare_runtime.py"
    "${NP2_CSPOT_DIR}/cspot" "${CMAKE_CURRENT_BINARY_DIR}/compat" RESULT_VARIABLE np2_runtime_result)
if(NOT np2_runtime_result EQUAL 0)
    message(FATAL_ERROR "Spotify lifecycle compatibility generation failed")
endif()
list(FILTER bell_sources EXCLUDE REGEX "/(NanoPBHelper|Crypto)\\.cpp$")
list(APPEND bell_sources "${CMAKE_CURRENT_BINARY_DIR}/compat/NanoPBHelper.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/Crypto.cpp"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/spotify_vorbis_guard.c")
execute_process(COMMAND "${np2_python}"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/prepare_vorbis.py"
    "${NP2_CSPOT_DIR}/cspot/bell/external/tremor"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/tremor" RESULT_VARIABLE np2_vorbis_result)
if(NOT np2_vorbis_result EQUAL 0)
    message(FATAL_ERROR "Private bounded Tremor copy generation failed")
endif()
set(np2_guarded_bell_sources "")
foreach(source IN LISTS bell_sources)
    if(source MATCHES "/external/tremor/([^/]+\\.c)$")
        list(APPEND np2_guarded_bell_sources "${CMAKE_CURRENT_BINARY_DIR}/compat/tremor/${CMAKE_MATCH_1}")
    else()
        list(APPEND np2_guarded_bell_sources "${source}")
    endif()
endforeach()
set(bell_sources "${np2_guarded_bell_sources}")
target_include_directories(bell BEFORE PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/compat/tremor")
set_property(TARGET bell PROPERTY SOURCES "${bell_sources}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NP2_SPOTIFY_ADAPTER_ROOT}/prepare_compat.py"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/prepare_vorbis.py"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/prepare_runtime.py"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/source-lock.json"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/verify_sources.py")
get_target_property(np2_cspot_sources cspot SOURCES)
target_include_directories(cspot BEFORE PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/compat")
list(FILTER np2_cspot_sources EXCLUDE REGEX "/(ApResolve|AccessKeyFetcher|TrackQueue|CDNAudioFile|TrackPlayer|LoginBlob|PlainConnection|ShannonConnection|MercurySession|SpircHandler|TimeProvider|AuthChallenges|Session|TrackReference|PlaybackState)\\.cpp$")
list(APPEND np2_cspot_sources
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/PlainConnection.cpp"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/spotify_packet_policy.c"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/spotify_ap_parse.c"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/spotify_ap_verify.c"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/ApResolve.cpp"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/AccessKeyFetcher.cpp"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/SpotifyResponse.cpp"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/spotify_response_codec.c"
    "${NP2_SPOTIFY_ADAPTER_ROOT}/adapter/spotify_login_policy.c"
    "${NP2_ROOT}/firmware/main/google_agenda_codec.c"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/AuthChallenges.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/Session.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/TrackReference.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/PlaybackState.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/ShannonConnection.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/MercurySession.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/SpircHandler.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/TimeProvider.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/TrackQueue.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/CDNAudioFile.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/LoginBlob.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/compat/TrackPlayer.cpp")
set_property(TARGET cspot PROPERTY SOURCES "${np2_cspot_sources}")
foreach(lib cspot bell)
    target_compile_options(${lib} PRIVATE -fexceptions)
    target_compile_definitions(${lib} PRIVATE
        CONFIG_ESP32_PTHREAD_TASK_PRIO_DEFAULT=CONFIG_PTHREAD_TASK_PRIO_DEFAULT)
endforeach()
