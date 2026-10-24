if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    set(LK_JSON_COMPONENT espressif__cjson)
else()
    set(LK_JSON_COMPONENT json)
endif()

idf_component_register(
    SRC_DIRS ./src
    INCLUDE_DIRS ./include
    PRIV_REQUIRES ${LK_JSON_COMPONENT} mbedtls esp_http_client
)
