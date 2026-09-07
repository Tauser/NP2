idf_component_register(
    SRCS "flash_coordinator.c" "app_main.c" "board_bringup.c" "diagnostic_ui.c"
    INCLUDE_DIRS "."
    REQUIRES esp_app_format esp_psram nvs_flash
)
