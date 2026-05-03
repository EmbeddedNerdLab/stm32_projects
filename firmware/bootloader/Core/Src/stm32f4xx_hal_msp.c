#include "stm32f4xx_hal.h"

/* Bootloader HAL MSP — minimal: UART2 GPIO is init'd inline in main.c,
 * RTC uses LSI (no external crystal).
 * This file satisfies HAL_MspInit() and HAL_RTC_MspInit() weak symbols. */

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc)
{
    (void)hrtc;
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    __HAL_RCC_LSI_ENABLE();
    uint32_t t = HAL_GetTick();
    while (!__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY)) {
        if (HAL_GetTick() - t > 2000U) break;
    }

    __HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSI);
    __HAL_RCC_RTC_ENABLE();
}

void HAL_RTC_MspDeInit(RTC_HandleTypeDef *hrtc)
{
    (void)hrtc;
    __HAL_RCC_RTC_DISABLE();
}
