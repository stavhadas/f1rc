target_sources(f1rc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/hldc.c
)

target_include_directories(f1rc PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/inc/
)

# hldc_thread.c depends on CMSIS-RTOS2 (cmsis_os2.h) and is only meaningful
# on the MCU target -- the host test build compiles hldc.c standalone with no
# FreeRTOS available, so keep this out of that build entirely.
if(CMAKE_CROSSCOMPILING)
    target_sources(f1rc PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/hldc_thread.c
    )
endif()