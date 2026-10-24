# cJSON moved from IDF to the component registry (espressif/cjson) in IDF v6.0
if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    set(json_component espressif__cjson)
else()
    set(json_component json)
endif()

idf_component_register(SRCS "main.c" "example.c" "board.c" "media.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_tsens ${json_component})
