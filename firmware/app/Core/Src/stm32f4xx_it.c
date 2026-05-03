/*
 * stm32f4xx_it.c — Interrupt service routines
 *
 * NOTE: SVC_Handler, PendSV_Handler, and SysTick_Handler are NOT defined here.
 * FreeRTOSConfig.h maps them via preprocessor:
 *
 *   #define vPortSVCHandler     SVC_Handler
 *   #define xPortPendSVHandler  PendSV_Handler
 *   #define xPortSysTickHandler SysTick_Handler
 *
 * This causes the FreeRTOS port.c functions to be compiled with the CMSIS
 * symbol names expected by the startup vector table, with zero call overhead.
 * Defining those handlers here would cause a "multiple definition" linker error.
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "usbd_conf.h"
#include "uart_console.h"

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

void NMI_Handler(void)
{
    for (;;) {}
}

void HardFault_Handler(void)
{
    /* Set a breakpoint here in debug builds to inspect the stacked PC. */
    for (;;) {}
}

void MemManage_Handler(void)
{
    for (;;) {}
}

void BusFault_Handler(void)
{
    for (;;) {}
}

void UsageFault_Handler(void)
{
    for (;;) {}
}

void DebugMon_Handler(void)
{
}

void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

void DMA1_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_tx);
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}
