set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

set(COMPONENT_ADD_INCLUDEDIRS include include/port)

# Edit following two lines to set component requirements (see docs)

list (APPEND COMPONENT_SRCDIRS ./ ./port ./mem_trace)

list(APPEND COMPONENT_REQUIRES esp-tls mbedtls esp_netif)

register_component()
