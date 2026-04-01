# ── Dependencies ───────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── Targets ────────────────────────────────────────────────────────────────────
add_executable(f1rc-tests "")



# GTest requires C++ — compile .c test files as CXX
set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/hldc_tests.c
    ${CMAKE_CURRENT_LIST_DIR}/simpliciti_tests.c
    PROPERTIES LANGUAGE CXX
)

target_sources(f1rc-tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/hldc_tests.c
    ${CMAKE_CURRENT_LIST_DIR}/simpliciti_tests.c
    ${CMAKE_SOURCE_DIR}/comms/hldc/hldc.c
    ${CMAKE_SOURCE_DIR}/utils/crc/crc.c
    ${CMAKE_SOURCE_DIR}/comms/simpliciti/simpliciti.c
    ${CMAKE_SOURCE_DIR}/comms/simpliciti/mfri/mfri.c
    ${CMAKE_SOURCE_DIR}/comms/simpliciti/nwk/nwk.c
)

target_include_directories(f1rc-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/comms/hldc/
    ${CMAKE_SOURCE_DIR}/comms/hldc/inc/
    ${CMAKE_SOURCE_DIR}/utils/crc/inc/
    ${CMAKE_SOURCE_DIR}/comms/simpliciti/inc/
    ${CMAKE_SOURCE_DIR}/utils/tracing/inc/
)

target_link_libraries(f1rc-tests PRIVATE GTest::gtest f1rc)

# ── Test discovery ─────────────────────────────────────────────────────────────
enable_testing()
include(GoogleTest)
gtest_discover_tests(f1rc-tests)