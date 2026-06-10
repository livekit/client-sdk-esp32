idf_component_register(SRCS "main.c" "example.c" "board.c" "media.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_usb_serial_jtag)