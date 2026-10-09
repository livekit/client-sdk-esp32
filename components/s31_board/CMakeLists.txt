idf_component_register(
    SRCS
        "s31_board.c"
        "s31_function_core_audio.c"
        "s31_korvo_audio.c"
        "s31_korvo_camera.c"
        "s31_rgb_led.c"
        "s31_audio_render.c"
        "s31_aec.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES
        espressif__esp_codec_dev
        espressif__esp_capture
        tempotian__av_render
        espressif__media_lib_sal
    PRIV_REQUIRES
        espressif__esp-sr
        esp_ringbuf
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_i2s
        espressif__esp_video
        espressif__led_strip
)
