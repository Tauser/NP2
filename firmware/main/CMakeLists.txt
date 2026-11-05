file(GLOB NP2_FONT_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/ui/fonts/*.c")

# This repository's normal build/flash/monitor path targets the unlocked
# development P4. Production pipelines must explicitly set this OFF and use
# NVS Encryption protected by Flash Encryption instead.
option(NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION
       "Retain Wi-Fi credentials in development NVS" ON)
option(NP2_MUSIC_AUDIO_BENCH "Run one bounded PCM probe after boot; laboratory only" OFF)
option(NP2_H264_DECODE_BENCH "Compare H.264 decoders once after boot; laboratory only (ADR-121)" OFF)
set(NP2_H264_BENCH_SAMPLE "" CACHE FILEPATH "Annex B sample embedded by the decode bench; never versioned")
option(NP2_SPOTIFY_PAIRING_BENCH "Private bounded Connect pairing experiment; no playback" OFF)
option(NP2_SPOTIFY_PLAYBACK_BENCH "Enable experimental native Spotify playback after decoder gate" OFF)
if(NP2_SPOTIFY_PLAYBACK_BENCH AND NOT NP2_SPOTIFY_PAIRING_BENCH)
    message(FATAL_ERROR "Spotify playback experiment requires its private pairing build")
endif()
if(NP2_SPOTIFY_PAIRING_BENCH)
    if(NOT CONFIG_COMPILER_CXX_EXCEPTIONS)
        message(FATAL_ERROR "Pairing bench requires its exception-enabled sdkconfig; compiler flags alone are unsafe")
    endif()
    set(NP2_SPOTIFY_REQUIRES esp_http_server pthread)
endif()

# A UI compilada pertence ao firmware. O controlador projeta AppState para os
# widgets já criados, sem duplicar layout ou iniciar I/O.

idf_component_register(
    # G5 policies and opt-in physical-maintenance OTA adapter.
    SRCS "home_widget_preferences.c" "spotify_cover.c" "spotify_remote.c" "spotify_remote_credentials.c" "ui/screens/np_spotify.c" "ui/screens/np_home_camera.c""spotify_http_policy.c" "update_replay_policy.c" "update_journal.c" "update_recovery_policy.c" "update_https_policy.c" "update_policy.c" "update_manifest.c" "update_signature.c" "update_image_hash.c" "update_keyring.c" "update_admission.c" "update_p4_writer.c" "update_boot_supervisor.c"
         "update_development.c" "cache_record.c" "coingecko_quota.c" "offline_data_codec.c" "offline_data_provider.c" "offline_value_format.c" "data_refresh_scheduler.c" "weather_condition.c" "weather_asset_service.c"
         "ui/core/np_styles.c" "ui/core/np_form.c" "ui/core/np_components.c" "ui/core/np_feedback.c" "ui/core/np_modal.c" "ui/core/np_keyboard.c" "ui/core/np_confirm.c"
         "ui/assets/np_weather_background_assets.c" "ui/assets/np_weather_static_icons.c" "ui/assets/np_btc_tilted_icon.c"
         "ui/screens/np_boot.c" "ui/screens/np_home.c" "ui/screens/np_weather.c" "ui/screens/np_market.c" "ui/screens/np_devices.c"
         "ui/screens/np_profile.c" "ui/screens/np_preferences.c"
         "ui/screens/settings/np_settings_display_sound.c" "ui/screens/settings/np_settings_photo_frame.c" "ui/screens/settings/np_settings_system.c" "ui/screens/settings/np_settings_notifications.c" "ui/screens/settings/np_settings_timezone.c" "ui/screens/settings/np_settings_wifi.c" "ui/screens/settings/np_wifi_password.c"
         "ui/screens/np_navigation_manager.c" "ui/screens/np_pomodoro.c" "ui/screens/np_notification_center.c" "ui/screens/product_ui.c" "app_state.c" "app_event_bus.c" "agenda_service.c" "agenda_storage.c" "google_agenda_codec.c" "google_agenda_credentials.c" "google_agenda_sync.c" "ui/screens/np_agenda.c" "pomodoro_service.c" "camera_stream_service.c" "camera_credentials.c" "camera_inventory.c" "i420_rgb565.c" "user_profile.c" "timezone_catalog.c" "time_service.c" "network_validation_service.c" "connectivity_diagnostic.c" "sonoff_lan_service.c" "np_ewelink.c" "np_ewelink_auth.c" "np_ewelink_cloud.c" "np_ewelink_storage.c" "onvif_discovery_service.c" "notification_service.c" "provisioning_service.c" "onboarding_service.c" "wifi_setup_view.c" "flash_coordinator.c" "device_control_service.c" "app_main.c" "board_bringup.c" "diagnostic_ui.c"
         "sound_melody.c" "photo_frame_model.c" "photo_frame_service.c" "photo_thumbnail_service.c" "spotify_http_bridge.c" "music_pcm.c" "music_audio_service.c"
         "voice_capture.c" "voice_resampler.c" "xiaozhi_protocol.c" "xiaozhi_service.c" "audio_capture_io.c" "ui/screens/np_assistant.c"
         "ui/screens/np_photo_frame.c" ${NP2_FONT_SRCS}
    INCLUDE_DIRS "." "ui/core" "ui/screens"
    EMBED_TXTFILES "timezone_catalog.csv"
    LDFRAGMENTS "memory_layout.lf"
    REQUIRES esp_app_format esp_event esp_netif esp_psram esp_wifi espressif__esp_hosted
             espressif__esp_wifi_remote joltwallet__littlefs nvs_flash esp_driver_usb_serial_jtag
             espressif__mdns esp_http_client mbedtls json app_update bootloader_support esp_driver_tsens
             esp_driver_sdmmc fatfs sdmmc lwip esp_rom espressif__esp_h264 edge264 sub0h264
             espressif__esp_websocket_client espressif__esp_audio_codec spi_flash
             ${NP2_SPOTIFY_REQUIRES}
)

# Per-pixel path at video rate; the application default is -Og.
set_source_files_properties("i420_rgb565.c" PROPERTIES COMPILE_OPTIONS "-O2")

# sdkconfig.defaults does not override a generated sdkconfig from an older build.
# ADR-076 keeps the P4 AES-DMA path disabled until its memory gate is qualified.
if(CONFIG_MBEDTLS_HARDWARE_AES)
    message(FATAL_ERROR
        "NP2 ADR-076: CONFIG_MBEDTLS_HARDWARE_AES must be OFF. "
        "Disable hardware AES in the effective sdkconfig or use a fresh SDKCONFIG; "
        "changing sdkconfig.defaults alone does not replace a cached value.")
endif()

if(NOT CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY)
    message(FATAL_ERROR
        "NP2 ADR-095 requires CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y. "
        "Use a fresh SDKCONFIG or update the effective configuration.")
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE NP2_PRODUCT_FONTS=1)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=i2s_channel_read")
if(NP2_MUSIC_AUDIO_BENCH)
    target_sources(${COMPONENT_LIB} PRIVATE "music_audio_probe.c")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE NP2_MUSIC_AUDIO_BENCH=1)
endif()

if(NP2_H264_DECODE_BENCH)
    if(NOT EXISTS "${NP2_H264_BENCH_SAMPLE}")
        message(FATAL_ERROR "Decode bench requires NP2_H264_BENCH_SAMPLE")
    endif()
    file(SIZE "${NP2_H264_BENCH_SAMPLE}" NP2_H264_BENCH_SAMPLE_BYTES)
    if(NP2_H264_BENCH_SAMPLE_BYTES GREATER 4194304)
        message(FATAL_ERROR "Decode bench sample exceeds 4 MiB; capture a shorter one")
    endif()
    file(SHA256 "${NP2_H264_BENCH_SAMPLE}" NP2_H264_BENCH_SAMPLE_SHA256)
    # A fixed name gives the bench fixed symbols for any sample.
    configure_file("${NP2_H264_BENCH_SAMPLE}" "${CMAKE_CURRENT_BINARY_DIR}/h264_bench_sample.h264" COPYONLY)
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_BINARY_DIR}/h264_bench_sample.h264" BINARY)
    target_sources(${COMPONENT_LIB} PRIVATE "h264_decode_bench.c")
    # The edge264 component is INTERFACE, so its external object never reaches the
    # linker by itself. Only this build links it; the product image is unchanged.
    add_dependencies(${COMPONENT_LIB} edge264_build)
    target_link_libraries(${COMPONENT_LIB} PRIVATE "${CMAKE_BINARY_DIR}/esp-idf/edge264/edge264.o")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE NP2_H264_DECODE_BENCH=1
        NP2_H264_BENCH_SAMPLE_SHA256="${NP2_H264_BENCH_SAMPLE_SHA256}")
endif()

if(NP2_SPOTIFY_PAIRING_BENCH)
    if(NOT EXISTS "${NP2_CSPOT_DIR}/cspot/CMakeLists.txt")
        message(FATAL_ERROR "Pairing bench requires the pinned NP2_CSPOT_DIR checkout")
    endif()
    get_filename_component(NP2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    include("${NP2_ROOT}/tools/spotify_native_probe/native_libraries.cmake")
    target_sources(${COMPONENT_LIB} PRIVATE "spotify_pairing_service.cpp"
        "${NP2_ROOT}/tools/spotify_native_probe/adapter/spotify_pairing_policy.c")
    if(NP2_SPOTIFY_PLAYBACK_BENCH)
        if(NOT CONFIG_SPIRAM_XIP_FROM_PSRAM OR NOT CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY)
            message(FATAL_ERROR "Spotify PSRAM workers require the qualified XiP/stack profile")
        endif()
        target_sources(${COMPONENT_LIB} PRIVATE "spotify_decoder_fixture.c" "spotify_pcm_sink.c"
            "spotify_playback_runtime.cpp")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE NP2_SPOTIFY_PLAYBACK_BENCH=1)
    endif()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE NP2_SPOTIFY_PAIRING_BENCH=1)
    target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
    target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
    target_link_libraries(${COMPONENT_LIB} PRIVATE cspot idf::esp_http_server)
endif()

# ADR-104: the IDF HTTP parser otherwise permits 80 KiB of response headers,
# before the client event can reject them. Bound all shared-worker responses
# at the parser, including a single fragmented header. No IDF source patch.
idf_component_get_property(np2_http_parser_lib http_parser COMPONENT_LIB)
target_compile_definitions(${np2_http_parser_lib} PRIVATE HTTP_MAX_HEADER_SIZE=8192)

# Reject stale lookup data while keeping the persisted catalog order.
idf_build_get_property(np2_catalog_python PYTHON)
execute_process(COMMAND "${np2_catalog_python}"
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/generate_timezone_index.py" --check
    RESULT_VARIABLE np2_catalog_check OUTPUT_QUIET)
if(NOT np2_catalog_check EQUAL 0)
    message(FATAL_ERROR "Timezone index stale: run tools/generate_timezone_index.py")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/timezone_catalog.csv"
    "${CMAKE_CURRENT_LIST_DIR}/timezone_catalog_index.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/generate_timezone_index.py")

target_compile_definitions(${COMPONENT_LIB} PRIVATE NP2_WEATHER_BACKGROUNDS_EMBEDDED=0)

if(NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
                               NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION=1)
endif()

# Generated public-only laboratory configuration. Absent in normal builds.
if(NP2_OTA_DEVELOPMENT_HEADER)
    if(NOT EXISTS "${NP2_OTA_DEVELOPMENT_HEADER}")
        message(FATAL_ERROR "NP2_OTA_DEVELOPMENT_HEADER does not exist")
    endif()
    configure_file("${NP2_OTA_DEVELOPMENT_HEADER}"
                   "${CMAKE_CURRENT_BINARY_DIR}/np2_ota_development_config.h" COPYONLY)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE NP2_OTA_DEVELOPMENT_CONFIGURED=1)
endif()

# Fixed-pool JPEG decoder, from the already pinned LVGL 9.5.0 source. Keep a
# private namespace/config: no change to LVGL's registered image decoders.
idf_component_get_property(np2_lvgl_dir lvgl__lvgl COMPONENT_DIR)
execute_process(COMMAND "${np2_catalog_python}"
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/prepare_spotify_cover_decoder.py"
    "${np2_lvgl_dir}/src/libs/tjpgd" "${CMAKE_CURRENT_BINARY_DIR}/spotify_cover_decoder"
    RESULT_VARIABLE np2_cover_prepare)
if(NOT np2_cover_prepare EQUAL 0)
    message(FATAL_ERROR "Private cover decoder preparation failed")
endif()
target_sources(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/spotify_cover_decoder/np2_tjpgd.c")
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/spotify_cover_decoder")
set_source_files_properties("spotify_cover.c" "${CMAKE_CURRENT_BINARY_DIR}/spotify_cover_decoder/np2_tjpgd.c"
    PROPERTIES COMPILE_DEFINITIONS "jd_prepare=np2_cover_jd_prepare;jd_decomp=np2_cover_jd_decomp;jd_mcu_load=np2_cover_jd_mcu_load;jd_mcu_output=np2_cover_jd_mcu_output;jd_restart=np2_cover_jd_restart")
