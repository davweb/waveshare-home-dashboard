idf_component_register(
    SRCS "OtaUpdate.cpp"
    INCLUDE_DIRS "."
    REQUIRES Wireless esp_http_client app_update json
)
