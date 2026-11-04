# cJSON moved from IDF to the component registry (espressif/cjson) in IDF v6.0
if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    set(json_component espressif__cjson)
else()
    set(json_component json)
endif()

idf_component_register(
    SRC_DIRS ./core ./protocol
    PRIV_INCLUDE_DIRS ./core ./protocol
    INCLUDE_DIRS ./include
    REQUIRES
        av_render
        esp_capture
        media_lib_sal
    PRIV_REQUIRES
        esp_netif
        esp_peer
        esp_websocket_client
        ${json_component}
        khash
        mbedtls
        nanopb
)

idf_component_get_property(LIVEKIT_SDK_VERSION ${COMPONENT_NAME} COMPONENT_VERSION)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "LIVEKIT_SDK_VERSION=\"${LIVEKIT_SDK_VERSION}\"")

# IDF v6 treats all warnings as errors (-Werror), which would turn conversion
# warnings from IDF's own headers into build failures.
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wconversion" "-Wno-error=conversion" "-Wno-error=sign-conversion")