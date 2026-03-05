cmake_minimum_required(VERSION 3.16.0)

# Add Espressif libs (ESP32_Display_Panel, esp-lib-utils, ESP32_IO_Expander) as
# proper IDF components. PlatformIO downloads them to .pio/libdeps/<env>/ but
# won't compile them via LDF (they have idf_component.yml and their own
# CMakeLists.txt with idf_component_register). We glob across all envs so this
# works for both 'debug' and 'release' without hardcoding the env name.
file(GLOB _IDF_EXTRA_COMPONENTS
    "${CMAKE_CURRENT_LIST_DIR}/.pio/libdeps/*/ESP32_Display_Panel"
    "${CMAKE_CURRENT_LIST_DIR}/.pio/libdeps/*/esp-lib-utils"
    "${CMAKE_CURRENT_LIST_DIR}/.pio/libdeps/*/ESP32_IO_Expander"
)
list(APPEND EXTRA_COMPONENT_DIRS ${_IDF_EXTRA_COMPONENTS})

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(platformio)
