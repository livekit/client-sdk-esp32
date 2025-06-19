idf_component_register(
    SRC_DIRS ./src
    INCLUDE_DIRS ./include
    REQUIRES
        av_render
        esp_capture
        esp_codec_dev
        esp_netif
        esp_timer
        esp_websocket_client
        esp_webrtc
        json
        livekit_protocol
        mbedtls
        media_lib_sal
        peer_default
        webrtc_utils
)

idf_component_get_property(LIVEKIT_SDK_VERSION ${COMPONENT_NAME} COMPONENT_VERSION)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "LIVEKIT_SDK_VERSION=\"${LIVEKIT_SDK_VERSION}\"")