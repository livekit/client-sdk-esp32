if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    set(LK_JSON_COMPONENT espressif__cjson)
else()
    set(LK_JSON_COMPONENT json)
endif()

idf_component_register(
    SRC_DIRS ./core ./protocol
    PRIV_INCLUDE_DIRS ./core ./protocol
    INCLUDE_DIRS ./include
    REQUIRES
        av_render
        esp_capture
        media_lib_sal
    PRIV_REQUIRES
        esp_netif
        esp_peer
        esp_websocket_client
        ${LK_JSON_COMPONENT}
        khash
        mbedtls
        nanopb
)

idf_component_get_property(LIVEKIT_SDK_VERSION ${COMPONENT_NAME} COMPONENT_VERSION)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "LIVEKIT_SDK_VERSION=\"${LIVEKIT_SDK_VERSION}\"")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wconversion")

# IDF 6 RISC-V headers contain inline sign conversions diagnosed by GCC 15.
# Treat those SDK headers as system headers, retaining -Wconversion for our code.
if(IDF_VERSION_MAJOR GREATER_EQUAL 6 AND CONFIG_IDF_TARGET_ARCH_RISCV)
    target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE
        "${IDF_PATH}/components/riscv/include"
        "${IDF_PATH}/components/esp_hw_support/include"
    )
endif()

# ESP-IDF 6.1's ESP32-S31 RISC-V headers currently emit sign-conversion
# diagnostics when included from a component compiled with -Wconversion.
# Keep the diagnostics visible without promoting IDF header warnings to errors.
if(IDF_TARGET STREQUAL "esp32s31")
    target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=sign-conversion")
endif()
