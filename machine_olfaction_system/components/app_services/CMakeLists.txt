idf_component_register(
    SRCS
        "src/settings_store.c"
        "src/services.c"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        nvs_flash
        esp_timer
        esp_hw_support
        bme690
)
