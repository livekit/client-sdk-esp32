idf_component_register(
    SRC_DIRS ./core ./protocol
    PRIV_INCLUDE_DIRS ./core ./protocol
    INCLUDE_DIRS ./include
    REQUIRES
        av_render
        esp_capture
    PRIV_REQUIRES
        esp_codec_dev
        esp_netif
        esp_websocket_client
        esp_peer
        json
        mbedtls
        media_lib_sal
        nanopb
        khash
)

idf_component_get_property(LIVEKIT_SDK_VERSION ${COMPONENT_NAME} COMPONENT_VERSION)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "LIVEKIT_SDK_VERSION=\"${LIVEKIT_SDK_VERSION}\"")