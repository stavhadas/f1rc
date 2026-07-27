# Codegen (main.proto -> main.pb-c.c/.h) needs a host tool at configure time.
# Prefer a standalone protoc-c wrapper if present (common on Linux distros);
# otherwise fall back to protoc + the protoc-gen-c plugin invoked directly
# (e.g. conda envs, which ship protoc.exe and protoc-gen-c.exe separately,
# with no protoc-c wrapper binary).
find_program(PROTOC_C protoc-c)
if(PROTOC_C)
	set(_PROTOC_COMMAND ${PROTOC_C} --c_out=${GENERATED_DIR} --proto_path=${PROTO_PATH})
else()
	find_program(PROTOC protoc REQUIRED)
	find_program(PROTOC_GEN_C protoc-gen-c REQUIRED)
	set(_PROTOC_COMMAND ${PROTOC} --plugin=protoc-gen-c=${PROTOC_GEN_C} --c_out=${GENERATED_DIR} --proto_path=${PROTO_PATH})
endif()

set(PROTO_FILE "main.proto")
file(MAKE_DIRECTORY ${GENERATED_DIR})

message("Proto file: ${PROTO_FILE}")
execute_process(
	COMMAND ${_PROTOC_COMMAND} ${PROTO_FILE}
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
