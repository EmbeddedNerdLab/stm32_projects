#include "drv_bh1750.h"

/* BH1750 one-byte commands (no register address — use Master_Transmit) */
#define CMD_POWER_ON       0x01U
#define CMD_RESET          0x07U
#define CMD_CONT_H_RES     0x10U   /* Continuous high-resolution mode (1 lux, 120ms) */

DRV_Status_t BH1750_Init(BH1750_Handle_t *h, I2C_HandleTypeDef *hi2c,
                          SemaphoreHandle_t mutex)
{
    h->hi2c  = hi2c;
    h->mutex = mutex;
    h->initialized = false;

    DRV_MUTEX_TAKE(h->mutex);

    uint8_t cmd;

    cmd = CMD_POWER_ON;
    if (HAL_I2C_Master_Transmit(h->hi2c, BH1750_I2C_ADDR, &cmd, 1,
                                 DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    cmd = CMD_RESET;
    if (HAL_I2C_Master_Transmit(h->hi2c, BH1750_I2C_ADDR, &cmd, 1,
                                 DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    cmd = CMD_CONT_H_RES;
    if (HAL_I2C_Master_Transmit(h->hi2c, BH1750_I2C_ADDR, &cmd, 1,
                                 DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    DRV_MUTEX_GIVE(h->mutex);

    /* First measurement takes up to 180 ms in high-resolution mode */
    HAL_Delay(180);

    h->initialized = true;
    return DRV_OK;
}

DRV_Status_t BH1750_ReadRaw(BH1750_Handle_t *h, uint16_t *raw_out)
{
    if (!h->initialized) return DRV_ERR_NOT_INIT;

    uint8_t buf[2];

    DRV_MUTEX_TAKE(h->mutex);

    if (HAL_I2C_Master_Receive(h->hi2c, BH1750_I2C_ADDR, buf, 2,
                                DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    DRV_MUTEX_GIVE(h->mutex);

    *raw_out = (uint16_t)((buf[0] << 8) | buf[1]);
    return DRV_OK;
}
