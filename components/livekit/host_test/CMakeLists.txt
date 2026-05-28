# Minimal host-side build that exercises pure-logic pieces of the
# LiveKit SDK on Linux for CI. Currently it verifies the manual
# second-pass parser introduced in protocol.c (protocol_data_packet_
# extract_attributes), which is responsible for the RPC v2 attribute
# decode path.
#
# This is NOT a full host port of the SDK. The RPC managers depend on
# FreeRTOS xTimer / esp_timer which would need shims to run on Linux;
# those are out of scope for this minimal test.

cmake_minimum_required(VERSION 3.16)
project(livekit_host_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

set(LIVEKIT_ROOT  ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(NANOPB_ROOT   ${LIVEKIT_ROOT}/../third_party/nanopb)

# --- nanopb runtime -------------------------------------------------
add_library(nanopb STATIC
    ${NANOPB_ROOT}/src/pb_common.c
    ${NANOPB_ROOT}/src/pb_decode.c
    ${NANOPB_ROOT}/src/pb_encode.c
)
target_include_directories(nanopb PUBLIC ${NANOPB_ROOT}/include)

# --- generated protobuf code ---------------------------------------
add_library(livekit_pb STATIC
    ${LIVEKIT_ROOT}/protocol/livekit_models.pb.c
    ${LIVEKIT_ROOT}/protocol/livekit_rtc.pb.c
    ${LIVEKIT_ROOT}/protocol/livekit_metrics.pb.c
    ${LIVEKIT_ROOT}/protocol/timestamp.pb.c
)
target_include_directories(livekit_pb PUBLIC
    ${LIVEKIT_ROOT}/protocol
    ${LIVEKIT_ROOT}/include
)
target_link_libraries(livekit_pb PUBLIC nanopb)

# --- ESP-IDF shims --------------------------------------------------
# Only what protocol.c actually pulls in: esp_log macros and cJSON
# (cJSON is only used in signal helpers we don't call, so the stubs are
# enough to satisfy the linker).
add_library(host_shims STATIC shim/cJSON_stubs.c)
target_include_directories(host_shims PUBLIC shim)

# --- protocol module under test ------------------------------------
add_library(livekit_protocol STATIC ${LIVEKIT_ROOT}/core/protocol.c)
target_include_directories(livekit_protocol PUBLIC
    ${LIVEKIT_ROOT}/core
    ${LIVEKIT_ROOT}/protocol
    ${LIVEKIT_ROOT}/include
    shim
)
target_link_libraries(livekit_protocol PUBLIC livekit_pb host_shims)

# --- tests ----------------------------------------------------------
enable_testing()
add_executable(test_extract_attributes test_extract_attributes.c)
target_link_libraries(test_extract_attributes PRIVATE livekit_protocol)
add_test(NAME extract_attributes COMMAND test_extract_attributes)
