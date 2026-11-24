SET(SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/components/ui_comp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/ui_comp_hook.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui_helpers.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui_comm.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/ui_flow.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_fonts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/animations/ui_animations.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_screen1.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_screen1.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_detection.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_detection.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_result.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_result.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_menu.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_menu.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_menulight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_menulight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_sensordark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_sensordark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_networkdark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_networkdark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_bledark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_bledark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_audiodark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_audiodark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_displaydark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_displaydark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_systemdark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_systemdark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_sensorlight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_sensorlight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_networklight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_networklight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_blelight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_blelight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_audiolight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_audiolight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_displaylight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_displaylight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/screens/ui_systemlight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/events/aido_event_systemlight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_font__aidogui_default_alibaba__16.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_font_ai_ai_font_a2b93e6c2db05d6bbbf6f27d_otf_12.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_font_ai_ai_font_a2efc8b8e51fbd031a7142a1_ttf_20.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_font_ai_ai_font_a2b93e6c2db05d6bbbf6f27d_otf_16.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_font_ai_ai_font_a2b93e6c2db05d6bbbf6f27d_otf_9.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_font_ai_ai_font_600ee36d62d930b6186ed561_ttf_12.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_font_ai_ai_font_a2b93e6c2db05d6bbbf6f27d_otf_20.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_font_ai_ai_font_a2b93e6c2db05d6bbbf6f27d_otf_10.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_font_ai_ai_font_a2efc8b8e51fbd031a7142a1_ttf_24.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ui_font_ai_ai_font_600ee36d62d930b6186ed561_ttf_16.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/custom/custom.c"
)

file(GLOB_RECURSE AIDOGUI_USER_SOURCES CONFIGURE_DEPENDS "custom/*.c")
foreach(USER_SOURCE IN LISTS AIDOGUI_USER_SOURCES)
    string(TOLOWER "${USER_SOURCE}" LOWER_USER_SOURCE)
    # 旧扩展注册文件保留在磁盘供用户迁移，但不能链接进新代码。
    if(NOT LOWER_USER_SOURCE MATCHES "/custom/(user/(aidogui_user_hooks\\.c|simulator/aidogui_user_simulator\\.c)|project/(aidogui_project_hooks\\.c|simulator/aidogui_project_simulator\\.c))$")
        list(APPEND SOURCES "${USER_SOURCE}")
    endif()
endforeach()
list(REMOVE_DUPLICATES SOURCES)
add_library(ui ${SOURCES})
target_include_directories(ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/custom")
if(TARGET lvgl::lvgl)
    target_link_libraries(ui PUBLIC lvgl::lvgl)
elseif(TARGET lvgl)
    target_link_libraries(ui PUBLIC lvgl)
endif()
