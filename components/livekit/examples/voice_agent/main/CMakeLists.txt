if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    set(voice_agent_json_component espressif__cjson)
else()
    set(voice_agent_json_component json)
endif()

idf_component_register(SRCS "main.c" "example.c" "board.c" "media.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_tsens ${voice_agent_json_component})
