cmake_minimum_required(VERSION 3.5)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lk_custom_hardware)

# esp_asrc (a dependency of gmf_audio) has a format-string warning that IDF v5.4
# treats as an error
idf_build_get_property(build_components BUILD_COMPONENTS)
if("espressif__esp_asrc" IN_LIST build_components)
    idf_component_get_property(esp_asrc_lib espressif__esp_asrc COMPONENT_LIB)
    target_compile_options(${esp_asrc_lib} PRIVATE "-Wno-error=format")
endif()
