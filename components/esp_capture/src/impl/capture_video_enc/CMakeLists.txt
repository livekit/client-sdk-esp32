
set(component_srcdirs "./")

idf_component_register(
    SRC_DIRS ${component_srcdirs}
    INCLUDE_DIRS ./
    REQUIRES esp_driver_jpeg
)
