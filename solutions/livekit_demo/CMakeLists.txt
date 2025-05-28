# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if("${IDF_TARGET}" STREQUAL "esp32p4")
    set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/examples/ethernet/basic/components/ethernet_init")
endif()

list(APPEND EXTRA_COMPONENT_DIRS "../../components")

list(APPEND EXTRA_COMPONENT_DIRS "../common")

if(NOT DEFINED ENV{LK_SERVER_URL})
  message(FATAL_ERROR "Env variable LK_SERVER_URL must be set")
endif()

add_compile_definitions(LK_SERVER_URL="$ENV{LK_SERVER_URL}")

if(NOT DEFINED ENV{LK_TOKEN})
  message(FATAL_ERROR "Env variable LK_TOKEN must be set")
endif()

add_compile_definitions(LK_TOKEN="$ENV{LK_TOKEN}")

project(livekit_demo)
