# Reconfigure when the EEZ project file changes
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../eez-studio/waveshare-dashboard.eez-project
)

# Compile and copy UI files to the build directory
execute_process(
    COMMAND zsh ${CMAKE_CURRENT_SOURCE_DIR}/copy-ui.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE COPY_UI_RESULT
)
if(NOT COPY_UI_RESULT EQUAL 0)
    message(WARNING "copy-ui.sh failed with code ${COPY_UI_RESULT}")
endif()

file(GLOB UI_SRCS "ui_image_weather_partly_cloudy_night.c" "images.c" "*.c")

idf_component_register(
    SRCS ${UI_SRCS}
    INCLUDE_DIRS "."
    REQUIRES LVGL eez-framework lvgl__lvgl
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    LV_LVGL_H_INCLUDE_SIMPLE
    EEZ_FOR_LVGL
    EEZ_PLATFORM_ESP32
)
