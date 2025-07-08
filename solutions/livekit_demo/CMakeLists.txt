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

# Wi-Fi credentials
if(NOT DEFINED ENV{WIFI_SSID})
  message(FATAL_ERROR "WiFi SSID not set in env variable WIFI_SSID")
endif()
if(NOT DEFINED ENV{WIFI_PASSWORD})
  message(FATAL_ERROR "WiFi password not set in env variable WIFI_PASSWORD")
endif()

add_compile_definitions(WIFI_SSID="$ENV{WIFI_SSID}")
add_compile_definitions(WIFI_PASSWORD="$ENV{WIFI_PASSWORD}")

# Room connection method
if(DEFINED ENV{LK_TOKEN} AND NOT "$ENV{LK_TOKEN}" STREQUAL "")
  # Option A: Pre-generated token.
  message(STATUS "Using a pre-generated token and server URL for room connection")
  if(NOT DEFINED ENV{LK_SERVER_URL})
    message(FATAL_ERROR "LK_SERVER_URL is required when using a pre-generated token")
  endif()
  add_compile_definitions(LK_TOKEN="$ENV{LK_TOKEN}")
  add_compile_definitions(LK_SERVER_URL="$ENV{LK_SERVER_URL}")
elseif(DEFINED ENV{LK_SANDBOX_ID})
  # Option B: Sandbox token server.
  message(STATUS "Using a sandbox token server for room connection")
  add_compile_definitions(LK_SANDBOX_ID="$ENV{LK_SANDBOX_ID}")
  foreach(var LK_SANDBOX_ROOM_NAME LK_SANDBOX_PARTICIPANT_NAME)
    if(DEFINED ENV{${var}})
      add_compile_definitions(${var}="$ENV{${var}}")
    else()
      add_compile_definitions(${var}=NULL)
    endif()
  endforeach()
else()
  message(FATAL_ERROR
    "No method for room connection is configured.\n"
    "Please refer to the README.md for setup instructions."
  )
endif()

project(livekit_demo)
