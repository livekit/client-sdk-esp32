
list (APPEND COMPONENT_SRCDIRS ./src)

set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_PRIV_REQUIRES media_lib_sal esp_timer esp_audio_codec esp_video_codec)

register_component()
