target_include_directories(f1rc PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/inc
)

# Unlike hldc.c (pure protocol logic, host-testable), cc1101.c and
# cc1101_thread.c both depend on spi_interface.h -> stm32f4xx_hal.h and
# cmsis_os2.h, so they're inherently MCU-only either way -- no
# CMAKE_CROSSCOMPILING split needed here.
target_sources(f1rc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/cc1101.c
    ${CMAKE_CURRENT_LIST_DIR}/cc1101_thread.c
)
