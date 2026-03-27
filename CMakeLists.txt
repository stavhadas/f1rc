cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_COMPILER "g++")

project(f1rc)

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
add_executable(f1rc "")

target_link_libraries(f1rc PRIVATE GTest::gtest)

# ── Sources ────────────────────────────────────────────────────────────────────
include(utils/utils.cmake)
include(comms/comms.cmake)
include(tests/tests.cmake)

# ── Test discovery ─────────────────────────────────────────────────────────────
enable_testing()
include(GoogleTest)
gtest_discover_tests(f1rc)
