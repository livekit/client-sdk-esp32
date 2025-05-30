idf_component_register(
    SRC_DIRS ./src
    INCLUDE_DIRS ./include
    REQUIRES livekit_protocol esp_webrtc esp_websocket_client esp_netif mbedtls
)