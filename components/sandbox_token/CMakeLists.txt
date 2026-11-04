# cJSON moved from IDF to the component registry (espressif/cjson) in IDF v6.0
if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    set(json_component espressif__cjson)
else()
    set(json_component json)
endif()

idf_component_register(
    SRC_DIRS ./src
    INCLUDE_DIRS ./include
    PRIV_REQUIRES ${json_component} mbedtls esp_http_client
)