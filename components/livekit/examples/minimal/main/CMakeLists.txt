set(minimal_private_requires "")
if(IDF_TARGET STREQUAL "esp32s31")
    # Released av_render exposes an LCD header without propagating its dependency.
    list(APPEND minimal_private_requires s31_board esp_lcd)
endif()

idf_component_register(
    SRCS "main.c" "example.c" "board.c" "media.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${minimal_private_requires}
)
