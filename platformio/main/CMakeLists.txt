idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES Board Wireless ClockTools HttpTools LVGL ui freertos esp_timer
)
