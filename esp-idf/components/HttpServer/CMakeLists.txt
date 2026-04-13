idf_component_register(
    SRCS "HtmlUtils.cpp" "LogBuffer.cpp" "HttpServer.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server LVGL lvgl__lvgl freertos
)
