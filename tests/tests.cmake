# GTest requires C++ — compile .c test files as CXX
set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/hldc_tests.c
    PROPERTIES LANGUAGE CXX
)

target_sources(f1rc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/hldc_tests.c
    ${CMAKE_SOURCE_DIR}/comms/hldc/hldc.c
    ${CMAKE_SOURCE_DIR}/utils/crc/crc.c
)

target_include_directories(f1rc PRIVATE
    ${CMAKE_SOURCE_DIR}/comms/hldc/
    ${CMAKE_SOURCE_DIR}/comms/hldc/inc/
    ${CMAKE_SOURCE_DIR}/utils/crc/inc/
)
