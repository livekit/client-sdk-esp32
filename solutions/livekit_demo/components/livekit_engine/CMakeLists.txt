idf_component_register(
    SRC_DIRS ./src
    INCLUDE_DIRS ./include
    REQUIRES esp_webrtc esp_capture esp_codec_dev av_render media_lib_sal webrtc_utils
    PRIV_REQUIRES livekit_signaling
)