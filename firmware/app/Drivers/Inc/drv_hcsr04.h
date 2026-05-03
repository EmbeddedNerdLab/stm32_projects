#ifndef DRV_HCSR04_H
#define DRV_HCSR04_H

#include "drv_common.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

typedef struct {
    GPIO_TypeDef *trig_port;
    uint16_t      trig_pin;
    GPIO_TypeDef *echo_port;
    uint16_t      echo_pin;
    bool          initialized;
} HCSR04_Handle_t;

DRV_Status_t HCSR04_Init(HCSR04_Handle_t *h,
                          GPIO_TypeDef *trig_port, uint16_t trig_pin,
                          GPIO_TypeDef *echo_port, uint16_t echo_pin);

/* Trigger a measurement with temperature-compensated speed of sound.
   temp_cdeg: temperature in hundredths of °C (from BME280).
   Returns dist_cm = 0 on timeout / out of range. */
DRV_Status_t HCSR04_Read(HCSR04_Handle_t *h, uint16_t *dist_cm, int32_t temp_cdeg);

#endif /* DRV_HCSR04_H */
