idf_component_register(
    SRCS "http_data_handlers.cpp" "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES Wireless ClockTools HttpServer OtaUpdate MqttTools LVGL ui freertos esp_timer esp_psram esp_driver_tsens spi_flash app_update
)
