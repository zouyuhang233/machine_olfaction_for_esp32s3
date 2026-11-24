file(GLOB_RECURSE AIDOGUI_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.c")
list(REMOVE_ITEM AIDOGUI_SOURCES "${CMAKE_CURRENT_LIST_DIR}/app_ui.c")

idf_component_register(
    SRCS
        ${AIDOGUI_SOURCES}
        "app_ui.c"
    INCLUDE_DIRS
        "."
        "custom"
    REQUIRES
        lvgl__lvgl
        app_services
        buttons
        bsp
        lcd
        esp_timer
        esp_system
)

# AiDoGUI 生成代码包含大量单行事件流；保留警告，但不让生成器的排版风格中断构建。
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error
    -Wno-misleading-indentation
    -Wno-error=switch
)

