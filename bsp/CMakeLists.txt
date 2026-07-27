cmake_minimum_required(VERSION 3.23)

# ── Configurable cache variables ───────────────────────────────────────────────
set(BSP_MCU_DEFINES "USE_HAL_DRIVER;STM32F411xE"
    CACHE STRING "Semicolon-separated compile definitions for MCU and HAL")

set(BSP_FREERTOS_PORT "GCC/ARM_CM4F"
    CACHE STRING "FreeRTOS portable port directory under FreeRTOS/Source/portable/")

set(BSP_FREERTOS_HEAP "heap_4"
    CACHE STRING "FreeRTOS heap implementation (heap_1..heap_5) under portable/MemMang/")

# Required: directory containing FreeRTOSConfig.h and stm32f4xx_hal_conf.h
if(NOT DEFINED BSP_APP_CONFIG_DIR)
    message(FATAL_ERROR
        "BSP_APP_CONFIG_DIR must be set before add_subdirectory(bsp). "
        "It should point to the directory containing FreeRTOSConfig.h and stm32f4xx_hal_conf.h")
endif()

# ── Internal paths ─────────────────────────────────────────────────────────────
set(_BSP_DIR   ${CMAKE_CURRENT_LIST_DIR})
set(_HAL_DIR   ${_BSP_DIR}/Drivers/STM32F4xx_HAL_Driver)
set(_CMSIS_DIR ${_BSP_DIR}/Drivers/CMSIS)
set(_RTOS_DIR  ${_BSP_DIR}/Middlewares/Third_Party/FreeRTOS/Source)

# ── bsp INTERFACE library (include paths + compile definitions) ────────────────
add_library(bsp INTERFACE)
target_include_directories(bsp INTERFACE
    ${_HAL_DIR}/Inc
    ${_HAL_DIR}/Inc/Legacy
    ${_CMSIS_DIR}/Device/ST/STM32F4xx/Include
    ${_CMSIS_DIR}/Include
    ${_RTOS_DIR}/include
    ${_RTOS_DIR}/CMSIS_RTOS_V2
    ${_RTOS_DIR}/portable/${BSP_FREERTOS_PORT}
    ${BSP_APP_CONFIG_DIR}
)
target_compile_definitions(bsp INTERFACE
    ${BSP_MCU_DEFINES}
    $<$<CONFIG:Debug>:DEBUG>
)

# ── STM32 HAL drivers + startup ────────────────────────────────────────────────
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE
    ${_BSP_DIR}/startup_stm32f411xe.s
    ${_HAL_DIR}/Src/stm32f4xx_hal.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_cortex.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_dma.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_dma_ex.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_exti.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_flash.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_flash_ex.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_flash_ramfunc.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_gpio.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_pwr.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_pwr_ex.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_rcc.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_rcc_ex.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_spi.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_tim.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_tim_ex.c
    ${_HAL_DIR}/Src/stm32f4xx_hal_uart.c
)
target_link_libraries(STM32_Drivers PUBLIC bsp)

# ── FreeRTOS ───────────────────────────────────────────────────────────────────
add_library(FreeRTOS OBJECT)
target_sources(FreeRTOS PRIVATE
    ${_RTOS_DIR}/croutine.c
    ${_RTOS_DIR}/event_groups.c
    ${_RTOS_DIR}/list.c
    ${_RTOS_DIR}/queue.c
    ${_RTOS_DIR}/stream_buffer.c
    ${_RTOS_DIR}/tasks.c
    ${_RTOS_DIR}/timers.c
    ${_RTOS_DIR}/CMSIS_RTOS_V2/cmsis_os2.c
    ${_RTOS_DIR}/portable/MemMang/${BSP_FREERTOS_HEAP}.c
    ${_RTOS_DIR}/portable/${BSP_FREERTOS_PORT}/port.c
)
target_link_libraries(FreeRTOS PUBLIC bsp)

# ── Bundle into the top-level project library ──────────────────────────────────
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
    STM32_Drivers
    FreeRTOS
    ${TOOLCHAIN_LINK_LIBRARIES}
)

if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "BSP requires C11 or higher")
endif()
