#include "drv_hcsr04.h"

/* ── DWT µs helpers ──────────────────────────────────────────────────────────
 * Uses the Cortex-M4 cycle counter (DWT) for µs-accurate busy-waits.
 * dwt_init() is idempotent — safe to call multiple times.
 * ─────────────────────────────────────────────────────────────────────────── */
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static void dwt_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks);
}

static uint32_t dwt_elapsed_us(uint32_t start)
{
    return (DWT->CYCCNT - start) / (SystemCoreClock / 1000000U);
}

/* ── HCSR04_Init ─────────────────────────────────────────────────────────── */
DRV_Status_t HCSR04_Init(HCSR04_Handle_t *h,
                          GPIO_TypeDef *trig_port, uint16_t trig_pin,
                          GPIO_TypeDef *echo_port, uint16_t echo_pin)
{
    h->trig_port   = trig_port;
    h->trig_pin    = trig_pin;
    h->echo_port   = echo_port;
    h->echo_pin    = echo_pin;
    h->initialized = false;

    /* TRIG: push-pull output */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = trig_pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(trig_port, &gpio);
    HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_RESET);

    /* ECHO: floating input (5V-tolerant on PB6) */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin  = echo_pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(echo_port, &gpio);

    dwt_init();
    h->initialized = true;
    return DRV_OK;
}

/* ── HCSR04_Read ──────────────────────────────────────────────────────────── */
DRV_Status_t HCSR04_Read(HCSR04_Handle_t *h, uint16_t *dist_cm, int32_t temp_cdeg)
{
    *dist_cm = 0;
    if (!h->initialized) return DRV_ERR_NOT_INIT;

    /* 10 µs trigger pulse */
    HAL_GPIO_WritePin(h->trig_port, h->trig_pin, GPIO_PIN_RESET);
    dwt_delay_us(2);
    HAL_GPIO_WritePin(h->trig_port, h->trig_pin, GPIO_PIN_SET);
    dwt_delay_us(10);
    HAL_GPIO_WritePin(h->trig_port, h->trig_pin, GPIO_PIN_RESET);

    /* Wait for ECHO high — 5 ms timeout (sensor settling) */
    uint32_t t0 = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(h->echo_port, h->echo_pin) == GPIO_PIN_RESET) {
        if (dwt_elapsed_us(t0) > 5000U) return DRV_TIMEOUT;
    }

    /* Measure echo pulse — 20 ms timeout (~3.4 m max range) */
    uint32_t rise = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(h->echo_port, h->echo_pin) == GPIO_PIN_SET) {
        if (dwt_elapsed_us(rise) > 20000U) return DRV_TIMEOUT;
    }
    uint32_t pulse_us = dwt_elapsed_us(rise);

    /* Temperature-compensated speed of sound:
     *   v = 331.3 + 0.606 * T_celsius  [m/s]
     *   v_cm_per_s = 33130 + temp_cdeg * 606 / 1000
     *   dist_cm = pulse_us * v_cm_per_s / 2_000_000  (round-trip in µs)
     *
     * Overflow check: pulse_us(max 20000) * v_cm_per_s(max ~36000) = 720M < 2^32 ✓
     */
    int32_t  v_cm_per_s = 33130 + (temp_cdeg * 606 / 1000);
    if (v_cm_per_s < 30000) v_cm_per_s = 30000;  /* clamp for extreme cold */
    *dist_cm = (uint16_t)((uint32_t)pulse_us * (uint32_t)v_cm_per_s / 2000000U);
    return DRV_OK;
}
