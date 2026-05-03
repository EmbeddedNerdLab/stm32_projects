cmake_minimum_required(VERSION 3.20)

# Pass toolchain via:
#   cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi-toolchain.cmake ..
project(stm32_bootloader C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ── Compile definitions ──────────────────────────────────────────────────────
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
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32F446RETx_BL.ld")

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

# ── Library paths (libs/ lives one level up inside firmware/) ────────────────
set(LIBS_DIR       ${CMAKE_SOURCE_DIR}/../libs)
set(HAL_DIR        ${LIBS_DIR}/stm32f4xx-hal-driver)
set(CMSIS_DEV_DIR  ${LIBS_DIR}/cmsis-device-f4)
set(CMSIS_CORE_DIR ${LIBS_DIR}/CMSIS_5/CMSIS/Core/Include)

# ── HAL sources (minimal: only what bootloader needs) ────────────────────────
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
    ${HAL_DIR}/Src/stm32f4xx_hal_uart.c
    ${HAL_DIR}/Src/stm32f4xx_hal_rtc.c
    ${HAL_DIR}/Src/stm32f4xx_hal_rtc_ex.c
)

set(CMSIS_DEVICE_SOURCES
    ${CMSIS_DEV_DIR}/Source/Templates/gcc/startup_stm32f446xx.s
    ${CMSIS_DEV_DIR}/Source/Templates/system_stm32f4xx.c
)

# ── Executable ───────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME}.elf
    Core/Src/main.c
    Core/Src/aes128.c
    Core/Src/boot_proto.c
    Core/Src/stm32f4xx_hal_msp.c
    ${HAL_SOURCES}
    ${CMSIS_DEVICE_SOURCES}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Core/Inc
    ${HAL_DIR}/Inc
    ${CMSIS_DEV_DIR}/Include
    ${CMSIS_CORE_DIR}
)

set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    LINK_DEPENDS ${LINKER_SCRIPT}
)

# ── Post-build: .bin + size report ───────────────────────────────────────────
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating bootloader binary and size"
)
