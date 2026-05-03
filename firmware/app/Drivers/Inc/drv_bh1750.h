#ifndef DRV_BH1750_H
#define DRV_BH1750_H

#include "drv_common.h"
#include "stm32f4xx_hal.h"

#define BH1750_I2C_ADDR   0x46U   /* 7-bit addr 0x23 shifted left by 1 */

typedef struct {
    I2C_HandleTypeDef  *hi2c;
    SemaphoreHandle_t   mutex;
    bool                initialized;
} BH1750_Handle_t;

DRV_Status_t BH1750_Init(BH1750_Handle_t *h, I2C_HandleTypeDef *hi2c,
                          SemaphoreHandle_t mutex);

/* Returns raw count; actual lux = raw / 1.2 */
DRV_Status_t BH1750_ReadRaw(BH1750_Handle_t *h, uint16_t *raw_out);

#endif /* DRV_BH1750_H */
