idf_component_register(
    SRCS "lvgl_v8_port.cpp" "DisplayPanel.cpp"
    INCLUDE_DIRS "."
    REQUIRES espressif__esp32_display_panel lvgl__lvgl freertos esp_timer
)

## LVGL tearing avoidance mode (3 = double-buffer + direct-mode, recommended for RGB LCD)
target_compile_definitions(${COMPONENT_LIB} PUBLIC CONFIG_LVGL_PORT_AVOID_TEARING_MODE=3)

## Point LVGL at our custom lv_conf.h
target_compile_options(${COMPONENT_LIB} PUBLIC
    -DLV_CONF_PATH=\"${CMAKE_CURRENT_LIST_DIR}/lv_conf.h\"
)
