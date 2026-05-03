/*
 * uart_console.c — Thread-safe UART2 debug console with DMA TX
 *
 * PA2 = USART2_TX → ST-LINK bridge → virtual COM port on PC
 * PA3 = USART2_RX (not used for debug output)
 *
 * DMA1 Stream6 Channel4 handles TX autonomously; the CPU is free during
 * transfer. HAL_UART_TxCpltCallback fires from the USART2 IRQ when the
 * last bit has shifted out, giving the binary semaphore that unblocks the
 * waiting task.
 *
 * Both USART2_IRQn and DMA1_Stream6_IRQn must be set to priority 6 so
 * xSemaphoreGiveFromISR() can be called safely.
 */

#include "uart_console.h"
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define UART_TX_BUF_SIZE  512U

UART_HandleTypeDef  huart2;
DMA_HandleTypeDef   hdma_usart2_tx;

static SemaphoreHandle_t xUARTTxMutex;
static SemaphoreHandle_t xUARTTxDone;

static uint8_t TxBuf[UART_TX_BUF_SIZE];

void UART_Console_Init(void)
{
    xUARTTxMutex = xSemaphoreCreateMutex();
    xUARTTxDone  = xSemaphoreCreateBinary();
    configASSERT(xUARTTxMutex != NULL);
    configASSERT(xUARTTxDone  != NULL);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK)
        Error_Handler();
}

/* Called from USART2 IRQ when last bit shifted out */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2)
        return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xUARTTxDone, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int uart_write(const uint8_t *data, uint16_t len)
{
    if (len == 0)
        return 0;

    if (xSemaphoreTake(xUARTTxMutex, pdMS_TO_TICKS(200)) != pdTRUE)
        return -1;

    uint16_t chunk = (len > UART_TX_BUF_SIZE) ? UART_TX_BUF_SIZE : len;
    memcpy(TxBuf, data, chunk);

    xSemaphoreTake(xUARTTxDone, 0);  /* clear stale token */

    if (HAL_UART_Transmit_DMA(&huart2, TxBuf, chunk) != HAL_OK) {
        xSemaphoreGive(xUARTTxMutex);
        return -1;
    }

    /* Block until DMA finishes and TC fires (callback gives semaphore) */
    xSemaphoreTake(xUARTTxDone, pdMS_TO_TICKS(200));

    xSemaphoreGive(xUARTTxMutex);
    return (int)chunk;
}

int uart_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0)
        return uart_write((uint8_t *)buf, (uint16_t)len);

    return len;
}
