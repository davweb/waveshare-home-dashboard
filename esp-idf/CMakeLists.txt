cmake_minimum_required(VERSION 3.16.0)

## Point LVGL at our custom lv_conf.h before any components are processed
set(LV_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/components/LVGL/lv_conf.h" CACHE STRING "" FORCE)

## Project-wide compile definitions (board identity, framework modes)
add_compile_definitions(
    BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_4_3_B
    ESP_PANEL_BOARD_DEFAULT_USE_SUPPORTED
    ESP_PANEL_DRIVERS_BUS_ENABLE_RGB
    LV_LVGL_H_INCLUDE_SIMPLE
    EEZ_FOR_LVGL
    EEZ_PLATFORM_ESP32
)

## Firmware version — embedded in the binary via esp_app_desc_t.
## Override from release.sh via: idf.py -DPROJECT_VER="..."
if(NOT DEFINED PROJECT_VER)
    set(PROJECT_VER "dev")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(waveshare_dashboard)
