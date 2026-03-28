target_sources(f1rc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/crc.c
)

target_include_directories(f1rc PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/inc/
)