#include "drv_buzzer.h"

/* TIM3 input clock = APB1 × 2 = 90 MHz; PSC = 89 → tick = 1 MHz */
#define BUZZER_TIM_CLK_HZ  1000000UL

DRV_Status_t Buzzer_Init(Buzzer_Handle_t *h, TIM_HandleTypeDef *htim,
                          uint32_t channel)
{
    h->htim        = htim;
    h->channel     = channel;
    h->is_on       = false;
    h->initialized = true;
    return DRV_OK;
}

DRV_Status_t Buzzer_On(Buzzer_Handle_t *h, uint32_t freq_hz)
{
    if (!h->initialized) return DRV_ERR_NOT_INIT;
    if (freq_hz == 0U)   return DRV_ERR;

    uint32_t arr = (BUZZER_TIM_CLK_HZ / freq_hz) - 1U;
    uint32_t ccr = arr / 2U;  /* 50% duty cycle */

    /* Stop first to safely change ARR/CCR */
    HAL_TIM_PWM_Stop(h->htim, h->channel);

    __HAL_TIM_SET_AUTORELOAD(h->htim, arr);
    __HAL_TIM_SET_COMPARE(h->htim, h->channel, ccr);

    HAL_TIM_PWM_Start(h->htim, h->channel);
    h->is_on = true;
    return DRV_OK;
}

DRV_Status_t Buzzer_Off(Buzzer_Handle_t *h)
{
    if (!h->initialized) return DRV_ERR_NOT_INIT;
    HAL_TIM_PWM_Stop(h->htim, h->channel);
    h->is_on = false;
    return DRV_OK;
}

bool Buzzer_IsOn(const Buzzer_Handle_t *h)
{
    return h->is_on;
}
