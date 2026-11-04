set(component_requires unity)

# The test_utils component was removed from IDF in v6.0
if(EXISTS "$ENV{IDF_PATH}/tools/unit-test-app/components/test_utils")
    list(APPEND component_requires test_utils)
endif()

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "../../include"
                       PRIV_REQUIRES ${component_requires})
