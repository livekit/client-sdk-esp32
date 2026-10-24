set(voice_agent_sources
    "main.c"
    "example.c"
    "board.c"
    "media.c"
)

set(voice_agent_private_requires
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_tsens
    esp_wifi
    espressif__esp_codec_dev
)

if(CONFIG_LK_EXAMPLE_ENABLE_INTEROP_TEST)
    list(APPEND voice_agent_sources "interop_test.c")
endif()

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND voice_agent_private_requires espressif__cjson)
else()
    list(APPEND voice_agent_private_requires json)
endif()

if(IDF_TARGET STREQUAL "esp32s31")
    # Released av_render exposes an LCD header without propagating its dependency.
    list(APPEND voice_agent_private_requires s31_board esp_lcd)
endif()

idf_component_register(
    SRCS ${voice_agent_sources}
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${voice_agent_private_requires}
)
