
# Gather common sources
set(component_srcdirs "src" 
    "src/impl/capture_simple_path"
    "src/impl/capture_file_src"
)

idf_component_register(
    SRC_DIRS ${component_srcdirs}
    INCLUDE_DIRS ./include ./interface
    REQUIRES media_lib_sal esp_timer
)
