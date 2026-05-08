cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()



set(PROTO_PATH "${CMAKE_CURRENT_LIST_DIR}/protobuf/")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/car")
# include (${CMAKE_CURRENT_LIST_DIR}/../commander/commander.cmake)

add_executable(f1rc-car
	${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/car.c
    # ${CMAKE_CURRENT_LIST_DIR}/../commander/commander.c
    # ${GENERATED_DIR}/main.pb-c.c
)
target_include_directories(f1rc-car PRIVATE
    ${GENERATED_DIR}
    # ${PROTOBUF_C_INCLUDE_DIRS}
    ${CMAKE_CURRENT_LIST_DIR}/inc
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
target_link_libraries(f1rc-car PRIVATE f1rc stm32cubemx ${PROTOBUF_C_LIBRARIES})

add_custom_command(TARGET f1rc-car POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:f1rc-car> $<TARGET_FILE_DIR:f1rc-car>/f1rc-car.bin
    COMMENT "Converting ELF to binary"
)

# include(${CMAKE_CURRENT_LIST_DIR}/dispatcher/dispatcher.cmake)
include (${CMAKE_CURRENT_LIST_DIR}/car-bsp/car_bsp.cmake)
