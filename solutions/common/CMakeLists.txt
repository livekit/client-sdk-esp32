
set(component_srcdirs "./")

idf_component_register(
    SRC_DIRS ./
    INCLUDE_DIRS ./
    REQUIRES esp_netif nvs_flash esp_wifi
)
