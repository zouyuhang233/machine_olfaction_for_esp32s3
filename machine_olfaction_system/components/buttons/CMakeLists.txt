idf_component_register(
    SRCS "src/buttons.c"
    INCLUDE_DIRS "include"
    REQUIRES
        bsp
        driver
        esp_timer
)
