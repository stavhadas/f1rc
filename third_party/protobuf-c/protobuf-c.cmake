# protobuf-c's runtime is a single portable .c/.h pair (BSD-2-Clause, vendored
# from https://github.com/protobuf-c/protobuf-c) with no dependencies beyond
# libc, so it's built here as a small static library rather than pulled in
# via a host pkg-config package -- that lets it cross-compile for the ARM MCU
# targets the same way it builds for host tests, with no special-casing.
add_library(protobuf_c STATIC ${CMAKE_CURRENT_LIST_DIR}/protobuf-c/protobuf-c.c)
target_include_directories(protobuf_c PUBLIC ${CMAKE_CURRENT_LIST_DIR})
