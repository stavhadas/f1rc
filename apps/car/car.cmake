set(PROTO_PATH "${CMAKE_CURRENT_LIST_DIR}/protobuf/")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/car")
include (${CMAKE_CURRENT_LIST_DIR}/../commander/commander.cmake)

add_executable(f1rc-car
	${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/car.c
    ${CMAKE_CURRENT_LIST_DIR}/../commander/commander.c
    ${GENERATED_DIR}/main.pb-c.c
)
target_include_directories(f1rc-car PRIVATE
    ${GENERATED_DIR}
    ${PROTOBUF_C_INCLUDE_DIRS}
    ${CMAKE_CURRENT_LIST_DIR}/inc
)
target_link_libraries(f1rc-car PRIVATE f1rc ${PROTOBUF_C_LIBRARIES})

include(${CMAKE_CURRENT_LIST_DIR}/dispatcher/dispatcher.cmake)