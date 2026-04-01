find_package(PkgConfig REQUIRED)
pkg_check_modules(PROTOBUF_C REQUIRED libprotobuf-c)

# Find protoc-c compiler
find_program(PROTOC_C protoc-c REQUIRED)
set(PROTO_FILE "main.proto")
file(MAKE_DIRECTORY ${GENERATED_DIR})

message("Proto file: ${PROTO_FILE}")
execute_process(
	COMMAND ${PROTOC_C} --c_out=${GENERATED_DIR} --proto_path=${PROTO_PATH}
		${PROTO_FILE}
	RESULT_VARIABLE PROTOC_RESULT
	OUTPUT_VARIABLE PROTOC_OUTPUT
	ERROR_VARIABLE PROTOC_ERROR
)

# Check it succeeded
if(NOT PROTOC_RESULT EQUAL 0)
	message(FATAL_ERROR "protoc-c failed:\n${PROTOC_ERROR}")
else()
	message(STATUS "protoc-c generated files in ${GENERATED_DIR}")
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR}/inc/)