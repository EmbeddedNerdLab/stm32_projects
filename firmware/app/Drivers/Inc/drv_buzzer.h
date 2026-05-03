#ifndef DRV_BUZZER_H
#define DRV_BUZZER_H

#include "drv_common.h"
#include "stm32f4xx_hal.h"

/* TIM3 CH1 on PB4 (AF2).
 * TIM3 input clock = APB1 × 2 = 90 MHz.
 * Prescaler = 89 → TIM tick = 1 MHz.
 * ARR = (1_000_000 / freq_hz) - 1  */

typedef struct {
    TIM_HandleTypeDef  *htim;
    uint32_t            channel;
    bool                is_on;
    bool                initialized;
} Buzzer_Handle_t;

DRV_Status_t Buzzer_Init(Buzzer_Handle_t *h, TIM_HandleTypeDef *htim,
                          uint32_t channel);
DRV_Status_t Buzzer_On(Buzzer_Handle_t *h, uint32_t freq_hz);
DRV_Status_t Buzzer_Off(Buzzer_Handle_t *h);
bool         Buzzer_IsOn(const Buzzer_Handle_t *h);

#endif /* DRV_BUZZER_H */
