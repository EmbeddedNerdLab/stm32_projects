set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Prevent CMake from testing the compiler with a full link (no OS runtime)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ── Toolchain location ──────────────────────────────────────────────────────
# On Windows, pass the full bin directory path at configure time:
#   cmake -DTOOLCHAIN_PATH="C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin" ...
# On Linux/macOS with arm-none-eabi-* on PATH, omit TOOLCHAIN_PATH entirely.

if(CMAKE_HOST_WIN32)
    set(TOOLCHAIN_EXT ".exe")
else()
    set(TOOLCHAIN_EXT "")
endif()

set(TOOLCHAIN_PREFIX "arm-none-eabi-")

if(DEFINED TOOLCHAIN_PATH AND NOT TOOLCHAIN_PATH STREQUAL "")
    # Normalise slashes and ensure no trailing slash duplication
    file(TO_CMAKE_PATH "${TOOLCHAIN_PATH}" _TC_PATH)
    set(TC_BIN "${_TC_PATH}/${TOOLCHAIN_PREFIX}")
else()
    set(TC_BIN "${TOOLCHAIN_PREFIX}")
endif()

set(CMAKE_C_COMPILER   "${TC_BIN}gcc${TOOLCHAIN_EXT}")
set(CMAKE_CXX_COMPILER "${TC_BIN}g++${TOOLCHAIN_EXT}")
set(CMAKE_ASM_COMPILER "${TC_BIN}gcc${TOOLCHAIN_EXT}")
set(CMAKE_OBJCOPY      "${TC_BIN}objcopy${TOOLCHAIN_EXT}")
set(CMAKE_SIZE         "${TC_BIN}size${TOOLCHAIN_EXT}")

# Core CPU flags for Cortex-M4F (hard float ABI)
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CPU_FLAGS}")

# Skip host paths entirely
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
