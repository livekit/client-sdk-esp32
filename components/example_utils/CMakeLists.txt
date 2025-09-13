idf_component_register(
    SRC_DIRS ./src
    INCLUDE_DIRS ./include
    PRIV_REQUIRES esp_netif esp_wifi nvs_flash
)