cmake_minimum_required(VERSION 3.20)

# Toolchain must be set before project()
# Pass via: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-toolchain.cmake ..
project(stm32_freertos_blink C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ── Compile definitions ─────────────────────────────────────────────────────
# USE_HAL_DRIVER  → activates stm32f4xx_hal.h include chain
# STM32F446xx     → selects device header in stm32f4xx.h
add_compile_definitions(
    USE_HAL_DRIVER
    STM32F446xx
)

# ── Compiler flags ───────────────────────────────────────────────────────────
add_compile_options(
    -Wall
    -Wextra
    -fdata-sections
    -ffunction-sections
    -fno-exceptions
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-ggdb>
    $<$<CONFIG:Release>:-O2>
)

# ── Linker flags ─────────────────────────────────────────────────────────────
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32F446RETx_FLASH.ld")

add_link_options(
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map,--cref
    -Wl,--print-memory-usage
    --specs=nano.specs
    --specs=nosys.specs
    -lc
    -lm
)

# ── Submodule paths ──────────────────────────────────────────────────────────
set(FREERTOS_DIR   ${CMAKE_SOURCE_DIR}/libs/FreeRTOS-Kernel)
set(HAL_DIR        ${CMAKE_SOURCE_DIR}/libs/stm32f4xx-hal-driver)
set(CMSIS_DEV_DIR  ${CMAKE_SOURCE_DIR}/libs/cmsis-device-f4)
set(CMSIS_CORE_DIR ${CMAKE_SOURCE_DIR}/libs/CMSIS_5/CMSIS/Core/Include)

# ── FreeRTOS sources ─────────────────────────────────────────────────────────
set(FREERTOS_SOURCES
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/timers.c
    ${FREERTOS_DIR}/event_groups.c
    ${FREERTOS_DIR}/stream_buffer.c
    # Port: GCC / Cortex-M4 with FPU
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c
    # Heap: coalescing best-fit allocator
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c
)

# ── HAL sources (only modules used by this project) ──────────────────────────
set(HAL_SOURCES
    ${HAL_DIR}/Src/stm32f4xx_hal.c
    ${HAL_DIR}/Src/stm32f4xx_hal_rcc.c
    ${HAL_DIR}/Src/stm32f4xx_hal_rcc_ex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_gpio.c
    ${HAL_DIR}/Src/stm32f4xx_hal_cortex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_pwr.c
    ${HAL_DIR}/Src/stm32f4xx_hal_pwr_ex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_flash.c
    ${HAL_DIR}/Src/stm32f4xx_hal_flash_ex.c
    ${HAL_DIR}/Src/stm32f4xx_hal_dma.c
    ${HAL_DIR}/Src/stm32f4xx_hal_dma_ex.c
)

# ── CMSIS device sources ─────────────────────────────────────────────────────
# startup_stm32f446xx.s → vector table, Reset_Handler, stack/heap init
# system_stm32f4xx.c    → SystemInit() (FPU enable, vector table relocation)
set(CMSIS_DEVICE_SOURCES
    ${CMSIS_DEV_DIR}/Source/Templates/gcc/startup_stm32f446xx.s
    ${CMSIS_DEV_DIR}/Source/Templates/system_stm32f4xx.c
)

# ── Executable ───────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf
    Core/Src/main.c
    Core/Src/stm32f4xx_hal_msp.c
    Core/Src/stm32f4xx_it.c
    ${FREERTOS_SOURCES}
    ${HAL_SOURCES}
    ${CMSIS_DEVICE_SOURCES}
)

# ── Include directories ───────────────────────────────────────────────────────
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    # Application headers (main.h, FreeRTOSConfig.h, stm32f4xx_hal_conf.h)
    Core/Inc

    # FreeRTOS kernel public headers
    ${FREERTOS_DIR}/include

    # FreeRTOS port headers (portmacro.h)
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F

    # HAL public headers
    ${HAL_DIR}/Inc

    # CMSIS device headers (stm32f4xx.h, stm32f446xx.h, system_stm32f4xx.h)
    ${CMSIS_DEV_DIR}/Include

    # ARM CMSIS-Core headers (core_cm4.h, cmsis_gcc.h, etc.)
    ${CMSIS_CORE_DIR}
)

# Rebuild if linker script changes
set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
)

# ── Post-build: .bin + size report ───────────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating binary and printing size"
)
