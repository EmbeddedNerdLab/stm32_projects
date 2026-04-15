#ifndef STM32F4XX_HAL_CONF_H
#define STM32F4XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── HAL module selection ─────────────────────────────────────────────────
 * Only enable modules whose .c files are compiled in CMakeLists.txt.
 * Enabling a module here without compiling its .c file causes linker errors.
 */
#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED

/* ── Oscillator values ────────────────────────────────────────────────────
 * Nucleo-F446RE: HSE input is driven by the ST-LINK MCO output at 8 MHz
 * (not a crystal — use RCC_HSE_BYPASS in SystemClock_Config).
 */
#define HSE_VALUE                    8000000U   /* Hz */
#define HSE_STARTUP_TIMEOUT          100U       /* ms */
#define HSI_VALUE                    16000000U  /* Hz */
#define LSI_VALUE                    32000U
#define LSE_VALUE                    32768U
#define LSE_STARTUP_TIMEOUT          5000U
#define EXTERNAL_CLOCK_VALUE         12288000U  /* I2S external clock (unused) */

/* ── VDD voltage — affects Flash wait-state calculation in HAL ─────────── */
#define VDD_VALUE                    3300U      /* mV */

/* ── Misc ─────────────────────────────────────────────────────────────────*/
#define USE_RTOS                     0U
#define PREFETCH_ENABLE              1U
#define INSTRUCTION_CACHE_ENABLE     1U
#define DATA_CACHE_ENABLE            1U

/* SysTick priority — set to lowest; FreeRTOS takes over SysTick anyway */
#define TICK_INT_PRIORITY            15U

/* ── Include HAL driver headers for enabled modules ─────────────────────── */
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_pwr.h"

#ifdef __cplusplus
}
#endif

#endif /* STM32F4XX_HAL_CONF_H */
