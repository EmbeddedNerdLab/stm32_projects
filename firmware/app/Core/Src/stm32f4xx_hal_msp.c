/*
 * stm32f4xx_hal_msp.c — HAL MSP (MCU Support Package) initialization
 *
 * HAL_MspInit:        called by HAL_Init() — sets NVIC priority grouping
 * HAL_I2C_MspInit:    called by HAL_I2C_Init() — configures PB8/PB9 (AF4)
 * HAL_SPI_MspInit:    called by HAL_SPI_Init() — configures PB12–PB15
 * HAL_TIM_PWM_MspInit:called by HAL_TIM_PWM_Init() — configures PB4 (AF2)
 */

#include "main.h"
#include "uart_console.h"

void HAL_MspInit(void)
{
    /* All 4 priority bits must be preemption bits — FreeRTOS requirement */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    /* PB8 = SCL, PB9 = SDA — AF4, open-drain with internal pull-up */
    GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI2) return;

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    /* PB13 = SCK, PB14 = MISO, PB15 = MOSI — AF5 push-pull */
    GPIO_InitStruct.Pin       = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PB12 = CS (software-controlled) — start HIGH (deasserted) */
    GPIO_InitStruct.Pin       = GPIO_PIN_12;
    GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM3) return;

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* PB4 = TIM3 CH1 — AF2 push-pull */
    GPIO_InitStruct.Pin       = GPIO_PIN_4;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2)
        return;

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* PA2 = USART2_TX, PA3 = USART2_RX — AF7 */
    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* DMA1 Stream6 Channel4 = USART2_TX */
    hdma_usart2_tx.Instance                 = DMA1_Stream6;
    hdma_usart2_tx.Init.Channel             = DMA_CHANNEL_4;
    hdma_usart2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_usart2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart2_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart2_tx.Init.Mode                = DMA_NORMAL;
    hdma_usart2_tx.Init.Priority            = DMA_PRIORITY_LOW;
    hdma_usart2_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_usart2_tx);

    __HAL_LINKDMA(huart, hdmatx, hdma_usart2_tx);

    /* Priority 6: safe for FreeRTOS FromISR API */
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}
