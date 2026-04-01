cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_COMPILER "g++")

project(f1rc)

add_library(f1rc "")



# ── Sources ────────────────────────────────────────────────────────────────────
include(utils/utils.cmake)
include(comms/comms.cmake)
include(tests/tests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/apps/apps.cmake)
