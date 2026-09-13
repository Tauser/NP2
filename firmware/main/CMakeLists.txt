idf_component_register(
    SRCS "cache_record.c" "offline_data_codec.c" "offline_data_provider.c" "offline_value_format.c" "offline_dashboard.c" "app_state.c" "app_event_bus.c" "time_service.c" "network_validation_service.c" "connectivity_diagnostic.c" "provisioning_service.c" "wifi_setup_view.c" "flash_coordinator.c" "app_main.c" "board_bringup.c" "diagnostic_ui.c"
    INCLUDE_DIRS "."
    REQUIRES esp_app_format esp_event esp_netif esp_psram esp_wifi espressif__esp_hosted
             espressif__esp_wifi_remote joltwallet__littlefs nvs_flash esp_driver_usb_serial_jtag
             esp_http_client mbedtls
)
