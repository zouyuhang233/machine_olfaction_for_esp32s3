idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS ""
    REQUIRES
        bsp
        bme690
        lcd
        lvgl__lvgl
        lvgl_port
        ui
        app_services
)
