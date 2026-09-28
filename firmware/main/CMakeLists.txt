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
         "ui/core/np_styles.c" "ui/core/np_components.c" "ui/core/np_feedback.c" "ui/core/np_modal.c" "ui/core/np_keyboard.c" "ui/core/np_confirm.c"
         "ui/assets/np_weather_background_assets.c"
         "ui/screens/np_boot.c" "ui/screens/np_home.c"
         "ui/screens/np_settings.c" "ui/screens/settings/np_settings_system.c" "ui/screens/settings/np_settings_notifications.c" "ui/screens/settings/np_settings_timezone.c" "ui/screens/settings/np_settings_wifi.c" "ui/screens/settings/np_wifi_password.c"
         "ui/screens/product_ui.c" "app_state.c" "app_event_bus.c" "timezone_catalog.c" "time_service.c" "network_validation_service.c" "connectivity_diagnostic.c" "notification_service.c" "provisioning_service.c" "onboarding_service.c" "wifi_setup_view.c" "flash_coordinator.c" "device_control_service.c" "app_main.c" "board_bringup.c" "diagnostic_ui.c"
         ${NP2_FONT_SRCS}
    INCLUDE_DIRS "." "ui/core" "ui/screens"
    EMBED_TXTFILES "timezone_catalog.csv"
    REQUIRES esp_app_format esp_event esp_netif esp_psram esp_wifi espressif__esp_hosted
             espressif__esp_wifi_remote joltwallet__littlefs nvs_flash esp_driver_usb_serial_jtag
             esp_http_client mbedtls app_update bootloader_support
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE NP2_PRODUCT_FONTS=1)

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
