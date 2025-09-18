idf_component_register(
    SRC_DIRS ./core ./protocol
    PRIV_INCLUDE_DIRS ./core ./protocol
    INCLUDE_DIRS ./include
    REQUIRES
        av_render
        esp_capture
    PRIV_REQUIRES
        esp_netif
        esp_peer
        esp_websocket_client
        json
        khash
        mbedtls
        media_lib_sal
        nanopb
)

idf_component_get_property(LIVEKIT_SDK_VERSION ${COMPONENT_NAME} COMPONENT_VERSION)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "LIVEKIT_SDK_VERSION=\"${LIVEKIT_SDK_VERSION}\"")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wconversion")