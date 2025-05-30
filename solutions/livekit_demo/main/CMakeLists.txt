idf_component_register(SRCS "main.c" "board.c" "media_sys.c" "livekit_demo.c"
                       INCLUDE_DIRS ".")

add_compile_definitions(PB_BUFFER_ONLY=1)
add_compile_definitions(PB_VALIDATE_UTF8=1)