#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* LD2 green LED on Nucleo-F446RE: PA5 */
#define LD2_PIN               GPIO_PIN_5
#define LD2_GPIO_PORT         GPIOA
#define LD2_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
