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

/* HC-SR04 ultrasonic: TRIG=PA8, ECHO=PB6 */
#define HCSR04_TRIG_PORT      GPIOA
#define HCSR04_TRIG_PIN       GPIO_PIN_8
#define HCSR04_ECHO_PORT      GPIOB
#define HCSR04_ECHO_PIN       GPIO_PIN_6

/* Peripheral handles — defined in main.c */
extern I2C_HandleTypeDef  hi2c1;
extern SPI_HandleTypeDef  hspi2;
extern TIM_HandleTypeDef  htim3;

void Error_Handler(void);
void App_RequestFirmwareUpdate(void);  /* write magic + reset into bootloader */

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
