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