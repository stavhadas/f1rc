
target_sources(f1rc-car PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/car_bsp.c
    ${CMAKE_CURRENT_LIST_DIR}/stm32f4xx_hal_msp.c
    ${CMAKE_CURRENT_LIST_DIR}/stm32f4xx_it.c
    ${CMAKE_CURRENT_LIST_DIR}/syscalls.c
    ${CMAKE_CURRENT_LIST_DIR}/sysmem.c
    ${CMAKE_CURRENT_LIST_DIR}/system_stm32f4xx.c
    ${CMAKE_CURRENT_LIST_DIR}/stm32f4xx_hal_timebase_tim.c
)
target_include_directories(f1rc-car PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc)