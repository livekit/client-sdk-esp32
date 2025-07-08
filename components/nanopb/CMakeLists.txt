idf_component_register(SRC_DIRS ./src
                       INCLUDE_DIRS ./include)

target_compile_definitions(${COMPONENT_LIB} PRIVATE PB_BUFFER_ONLY=1)
target_compile_definitions(${COMPONENT_LIB} PRIVATE PB_VALIDATE_UTF8=1)
target_compile_definitions(${COMPONENT_LIB} PRIVATE PB_ENABLE_MALLOC=1)