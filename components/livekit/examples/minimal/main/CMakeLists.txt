idf_component_register(
    SRCS "main.c" "example.c" "board.c" "media.c"
    INCLUDE_DIRS "."
    # To show an embedded PNG on boot, drop `boot.png` into this directory and uncomment:
    EMBED_FILES "boot.png"
)