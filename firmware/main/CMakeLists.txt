idf_component_register(
    SRCS "app_main.c" "board_bringup.c" "diagnostic_ui.c"
    INCLUDE_DIRS "."
    REQUIRES esp_app_format esp_psram
)
