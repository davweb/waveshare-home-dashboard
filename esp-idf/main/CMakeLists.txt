idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES Wireless ClockTools HttpTools LVGL ui freertos esp_timer
)
