cmake_minimum_required(VERSION 3.5)
if(WIN32)
    # ESP-IDF's media dependency graph exceeds Windows' process command-line limit.
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON CACHE BOOL "Use response files for long ESP-IDF commands" FORCE)
endif()
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lk_custom_hardware_s31)

if(NOT IDF_TARGET STREQUAL "esp32s31")
    message(FATAL_ERROR "custom_hardware_s31 supports only the esp32s31 target")
endif()
