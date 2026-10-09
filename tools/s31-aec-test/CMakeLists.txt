cmake_minimum_required(VERSION 3.16)
# cspell:words Wextra Werror
project(s31_aec_test C)
# Use the installed capture interface headers, without linking ESP-IDF libraries.
set(ESP_CAPTURE_COMPONENT_DIR "" CACHE PATH "Installed esp_capture component")
if(NOT EXISTS "${ESP_CAPTURE_COMPONENT_DIR}/interface/esp_capture_audio_src_if.h")
    message(FATAL_ERROR "Set ESP_CAPTURE_COMPONENT_DIR to the installed esp_capture component")
endif()
add_executable(s31_aec_test test_aec.c)
target_include_directories(s31_aec_test PRIVATE stubs ../s31-audio-test/stubs
    ../../components/s31_board/include
    "${ESP_CAPTURE_COMPONENT_DIR}/include" "${ESP_CAPTURE_COMPONENT_DIR}/interface")
set_property(TARGET s31_aec_test PROPERTY C_STANDARD 11)
if(MSVC)
    target_compile_options(s31_aec_test PRIVATE /W4 /UNDEBUG)
else()
    target_compile_options(s31_aec_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
enable_testing()
add_test(NAME s31_aec_source COMMAND s31_aec_test)
