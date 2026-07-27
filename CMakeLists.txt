cmake_minimum_required(VERSION 3.23)

project(f1rc C CXX ASM)

# Must be set before add_subdirectory(bsp) -- bsp's library targets otherwise
# get configured with an empty CMAKE_BUILD_TYPE (car.cmake's own fallback for
# this runs too late, via include() after bsp is already processed), which
# silently drops -g3 debug info for the vendored FreeRTOS/HAL sources.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

add_library(f1rc "")

set(BSP_APP_CONFIG_DIR "${CMAKE_SOURCE_DIR}/apps/car/car-bsp/inc")
add_subdirectory(bsp) # ST BSP


# ── Sources ────────────────────────────────────────────────────────────────────
include(third_party/protobuf-c/protobuf-c.cmake)
include(utils/utils.cmake)
include(comms/comms.cmake)
# include(tests/tests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/apps/apps.cmake)
