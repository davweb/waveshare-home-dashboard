idf_component_register(
    SRCS "HttpTools.cpp"
    INCLUDE_DIRS "."
    REQUIRES Wireless esp_http_client bblanchon__arduinojson
)
