idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES Wireless ClockTools HttpTools HttpServer OtaUpdate MqttTools LVGL ui freertos esp_timer esp_psram esp_driver_tsens spi_flash
)
