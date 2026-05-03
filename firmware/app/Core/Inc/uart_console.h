#ifndef __UART_CONSOLE_H
#define __UART_CONSOLE_H

#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef  hdma_usart2_tx;

void UART_Console_Init(void);
int  uart_write(const uint8_t *data, uint16_t len);
int  uart_printf(const char *fmt, ...);

#endif /* __UART_CONSOLE_H */
