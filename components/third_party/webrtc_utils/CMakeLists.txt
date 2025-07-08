
set(COMPONENT_ADD_INCLUDEDIRS .)

# Edit following two lines to set component requirements (see docs)

list (APPEND COMPONENT_SRCDIRS .)

list(APPEND COMPONENT_REQUIRES esp-tls mbedtls esp_netif esp_ringbuf)

register_component()
