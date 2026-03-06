idf_component_register(
    SRCS "Wireless.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event nvs_flash
)
