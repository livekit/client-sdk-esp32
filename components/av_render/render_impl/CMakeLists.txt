
list (APPEND COMPONENT_SRCDIRS ./)
set(COMPONENT_PRIV_REQUIRES esp_timer media_lib_sal esp_lcd driver)

register_component()
