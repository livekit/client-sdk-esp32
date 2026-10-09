# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
if(WIN32)
    # ESP-IDF's media dependency graph exceeds Windows' process command-line limit.
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON CACHE BOOL "Use response files for long ESP-IDF commands" FORCE)
endif()
set(COMPONENTS main) # Trim build
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# These mbedTLS options were introduced in ESP-IDF 6.
if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    if(NOT SDKCONFIG_DEFAULTS)
        if(DEFINED ENV{SDKCONFIG_DEFAULTS} AND NOT "$ENV{SDKCONFIG_DEFAULTS}" STREQUAL "")
            set(SDKCONFIG_DEFAULTS "$ENV{SDKCONFIG_DEFAULTS}")
        else()
            set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
        endif()
    endif()
    list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.idf6")
endif()
project(minimal_video)

# esp_asrc 1.0.x logs a uint32_t with %d, which ESP-IDF 5.4 rejects under
# -Werror. Keep it a warning for that component only; our own code is unchanged.
idf_build_get_property(lk_build_components BUILD_COMPONENTS)
if("espressif__esp_asrc" IN_LIST lk_build_components)
    idf_component_get_property(lk_esp_asrc_lib espressif__esp_asrc COMPONENT_LIB)
    target_compile_options(${lk_esp_asrc_lib} PRIVATE "-Wno-error=format")
endif()
