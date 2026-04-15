/*
 * stm32f4xx_hal_msp.c — HAL MSP (MCU Support Package) initialization
 *
 * These functions override weak symbols in the HAL library and are called
 * automatically by HAL_Init() and individual HAL_XXX_Init() functions.
 */

#include "main.h"

/**
 * @brief  Global MSP initialization.
 *         Called by HAL_Init() before any peripheral is initialized.
 *
 *         Sets NVIC priority grouping to group 4: all 4 implemented bits
 *         are preemption priority, 0 bits for sub-priority.
 *         This is a hard requirement for FreeRTOS on Cortex-M; the priority
 *         masking in port.c only works correctly when there are no sub-priority
 *         bits.
 */
void HAL_MspInit(void)
{
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}
