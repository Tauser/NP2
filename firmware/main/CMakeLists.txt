file(GLOB NP2_FONT_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/ui/fonts/*.c")

# This repository's normal build/flash/monitor path targets the unlocked
# development P4. Production pipelines must explicitly set this OFF and use
# NVS Encryption protected by Flash Encryption instead.
option(NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION
       "Retain Wi-Fi credentials in development NVS" ON)

# A UI compilada pertence ao firmware. O controlador projeta AppState para os
# widgets já criados, sem duplicar layout ou iniciar I/O.

idf_component_register(
    # G5 policies and opt-in physical-maintenance OTA adapter.
    SRCS "update_replay_policy.c" "update_journal.c" "update_recovery_policy.c" "update_https_policy.c" "update_policy.c" "update_manifest.c" "update_signature.c" "update_image_hash.c" "update_keyring.c" "update_admission.c" "update_p4_writer.c" "update_boot_supervisor.c"
         "update_development.c" "cache_record.c" "offline_data_codec.c" "offline_data_provider.c" "offline_value_format.c" "data_refresh_scheduler.c" "weather_condition.c" "weather_asset_service.c"
         "ui/core/np_styles.c" "ui/core/np_form.c" "ui/core/np_components.c" "ui/core/np_feedback.c" "ui/core/np_modal.c" "ui/core/np_keyboard.c" "ui/core/np_confirm.c"
         "ui/assets/np_weather_background_assets.c" "ui/assets/np_weather_static_icons.c" "ui/assets/np_btc_tilted_icon.c"
         "ui/screens/np_boot.c" "ui/screens/np_home.c" "ui/screens/np_weather.c" "ui/screens/np_market.c" "ui/screens/np_devices.c"
         "ui/screens/np_profile.c" "ui/screens/np_preferences.c"
         "ui/screens/settings/np_settings_display_sound.c" "ui/screens/settings/np_settings_system.c" "ui/screens/settings/np_settings_notifications.c" "ui/screens/settings/np_settings_timezone.c" "ui/screens/settings/np_settings_wifi.c" "ui/screens/settings/np_wifi_password.c"
         "ui/screens/np_navigation_manager.c" "ui/screens/np_pomodoro.c" "ui/screens/np_notification_center.c" "ui/screens/product_ui.c" "app_state.c" "app_event_bus.c" "pomodoro_service.c" "camera_stream_service.c" "user_profile.c" "timezone_catalog.c" "time_service.c" "network_validation_service.c" "connectivity_diagnostic.c" "sonoff_lan_service.c" "np_ewelink.c" "np_ewelink_auth.c" "np_ewelink_cloud.c" "np_ewelink_storage.c" "onvif_discovery_service.c" "notification_service.c" "provisioning_service.c" "onboarding_service.c" "wifi_setup_view.c" "flash_coordinator.c" "device_control_service.c" "app_main.c" "board_bringup.c" "diagnostic_ui.c"
         ${NP2_FONT_SRCS}
    INCLUDE_DIRS "." "ui/core" "ui/screens"
    EMBED_TXTFILES "timezone_catalog.csv"
    REQUIRES esp_app_format esp_event esp_netif esp_psram esp_wifi espressif__esp_hosted
             espressif__esp_wifi_remote joltwallet__littlefs nvs_flash esp_driver_usb_serial_jtag
             espressif__mdns esp_http_client mbedtls json app_update bootloader_support esp_driver_tsens
             esp_driver_sdmmc fatfs sdmmc lwip esp_rom espressif__esp_h264 edge264
)

# sdkconfig.defaults does not override a generated sdkconfig from an older build.
# ADR-076 keeps the P4 AES-DMA path disabled until its memory gate is qualified.
if(CONFIG_MBEDTLS_HARDWARE_AES)
    message(FATAL_ERROR
        "NP2 ADR-076: CONFIG_MBEDTLS_HARDWARE_AES must be OFF. "
        "Disable hardware AES in the effective sdkconfig or use a fresh SDKCONFIG; "
        "changing sdkconfig.defaults alone does not replace a cached value.")
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE NP2_PRODUCT_FONTS=1)

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
