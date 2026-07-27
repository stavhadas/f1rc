target_include_directories(f1rc PUBLIC
${CMAKE_CURRENT_LIST_DIR}/inc)

target_sources(f1rc
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/spi_interface.c
)
