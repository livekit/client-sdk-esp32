if (${IDF_TARGET} STREQUAL "esp32s3")
list(APPEND COMPONENT_SRCS capture_video_dvp_src.c)
endif()

if (${IDF_TARGET} STREQUAL "esp32p4")
list(APPEND COMPONENT_SRCS capture_video_v4l2_src.c)
list(APPEND COMPONENT_REQUIRES esp_mm) 
endif()

idf_component_register(SRCS "${COMPONENT_SRCS}"
                       INCLUDE_DIRS "${COMPONENT_INCLUDES}"
                       REQUIRES ${COMPONENT_REQUIRES})
