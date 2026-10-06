cmake_minimum_required(VERSION 3.16)
set(COMPONENTS main)
# The unit-test-app components were removed from IDF in v6.0 (unity_test_utils
# now lives in the unity component itself)
if(EXISTS "$ENV{IDF_PATH}/tools/unit-test-app/components")
    set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_app)

# esp_asrc (a dependency of gmf_audio) has a format-string warning that IDF v5.4
# treats as an error
idf_build_get_property(build_components BUILD_COMPONENTS)
if("espressif__esp_asrc" IN_LIST build_components)
    idf_component_get_property(esp_asrc_lib espressif__esp_asrc COMPONENT_LIB)
    target_compile_options(${esp_asrc_lib} PRIVATE "-Wno-error=format")
endif()
