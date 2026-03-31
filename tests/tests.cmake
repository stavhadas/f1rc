# GTest requires C++ — compile .c test files as CXX
set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/hldc_tests.c
    ${CMAKE_CURRENT_LIST_DIR}/simpliciti_tests.c
    PROPERTIES LANGUAGE CXX
)

target_sources(f1rc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/hldc_tests.c
    ${CMAKE_CURRENT_LIST_DIR}/simpliciti_tests.c
    ${CMAKE_SOURCE_DIR}/comms/hldc/hldc.c
    ${CMAKE_SOURCE_DIR}/utils/crc/crc.c
    ${CMAKE_SOURCE_DIR}/comms/simpliciti/simpliciti.c
    ${CMAKE_SOURCE_DIR}/comms/simpliciti/mfri/mfri.c
    ${CMAKE_SOURCE_DIR}/comms/simpliciti/nwk/nwk.c
)

target_include_directories(f1rc PRIVATE
    ${CMAKE_SOURCE_DIR}/comms/hldc/
    ${CMAKE_SOURCE_DIR}/comms/hldc/inc/
    ${CMAKE_SOURCE_DIR}/utils/crc/inc/
    ${CMAKE_SOURCE_DIR}/comms/simpliciti/inc/
    ${CMAKE_SOURCE_DIR}/utils/tracing/inc/
)
