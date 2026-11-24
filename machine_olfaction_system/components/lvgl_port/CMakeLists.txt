idf_component_register(
    SRCS "src/lvgl_port.c"
    INCLUDE_DIRS "include"
    REQUIRES
        lcd
        lvgl__lvgl
        esp_timer
        freertos
)
