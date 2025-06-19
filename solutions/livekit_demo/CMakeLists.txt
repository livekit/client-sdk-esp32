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

# Check for required environment variables
if(NOT DEFINED ENV{WIFI_SSID})
  message(FATAL_ERROR "WiFi SSID not set in env variable WIFI_SSID")
endif()

if(NOT DEFINED ENV{WIFI_PASSWORD})
  message(FATAL_ERROR "WiFi password not set in env variable WIFI_PASSWORD")
endif()

if(NOT DEFINED ENV{ROOM_NAME})
  message(FATAL_ERROR "Room name not set in env variable ROOM_NAME")
endif()

if(NOT DEFINED ENV{PARTICIPANT_NAME})
  message(FATAL_ERROR "Participant name not set in env variable PARTICIPANT_NAME")
endif()

# Check for token configuration - either sandbox or direct token
if(DEFINED ENV{LK_TOKEN})
  # Using direct token approach
  if(NOT DEFINED ENV{LK_SERVER_URL})
    message(FATAL_ERROR "LK_TOKEN is set but LK_SERVER_URL is not set in env variable LK_SERVER_URL")
  endif()
  message(STATUS "Using direct token approach with LK_TOKEN and LK_SERVER_URL")
  add_compile_definitions(LK_TOKEN="$ENV{LK_TOKEN}")
  add_compile_definitions(LK_SERVER_URL="$ENV{LK_SERVER_URL}")
else()
  # Using sandbox approach
  if(NOT DEFINED ENV{LK_SANDBOX_ID})
    message(FATAL_ERROR "LiveKit sandbox ID not set in env variable LK_SANDBOX_ID (or set LK_TOKEN for direct token approach)")
  endif()
  message(STATUS "Using sandbox approach with LK_SANDBOX_ID")
  add_compile_definitions(LK_SANDBOX_ID="$ENV{LK_SANDBOX_ID}")
endif()

add_compile_definitions(WIFI_SSID="$ENV{WIFI_SSID}")
add_compile_definitions(WIFI_PASSWORD="$ENV{WIFI_PASSWORD}")
add_compile_definitions(ROOM_NAME="$ENV{ROOM_NAME}")
add_compile_definitions(PARTICIPANT_NAME="$ENV{PARTICIPANT_NAME}")

project(livekit_demo)
