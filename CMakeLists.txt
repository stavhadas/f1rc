cmake_minimum_required(VERSION 3.23)

project(f1rc C CXX ASM)

add_library(f1rc "")

add_subdirectory(f1rc/cmake/stm32cubemx) # ST BSP


# ── Sources ────────────────────────────────────────────────────────────────────
include(utils/utils.cmake)
include(comms/comms.cmake)
# include(tests/tests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/apps/apps.cmake)
