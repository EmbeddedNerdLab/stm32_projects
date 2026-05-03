#include "drv_ds1307.h"

DRV_Status_t DS1307_Init(DS1307_Handle_t *h, I2C_HandleTypeDef *hi2c,
                          SemaphoreHandle_t mutex)
{
    h->hi2c  = hi2c;
    h->mutex = mutex;
    h->initialized = false;

    DRV_MUTEX_TAKE(h->mutex);

    uint8_t val;

    /* Clear CH bit (bit 7) in seconds register to start the oscillator */
    if (HAL_I2C_Mem_Read(h->hi2c, DS1307_I2C_ADDR, 0x00U,
                         I2C_MEMADD_SIZE_8BIT, &val, 1,
                         DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }
    val &= 0x7FU;  /* clear CH bit */
    if (HAL_I2C_Mem_Write(h->hi2c, DS1307_I2C_ADDR, 0x00U,
                          I2C_MEMADD_SIZE_8BIT, &val, 1,
                          DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    /* Ensure 24-hour mode: clear bit 6 in hours register */
    if (HAL_I2C_Mem_Read(h->hi2c, DS1307_I2C_ADDR, 0x02U,
                         I2C_MEMADD_SIZE_8BIT, &val, 1,
                         DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }
    val &= 0xBFU;  /* clear 12/24 bit (bit 6) */
    if (HAL_I2C_Mem_Write(h->hi2c, DS1307_I2C_ADDR, 0x02U,
                          I2C_MEMADD_SIZE_8BIT, &val, 1,
                          DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    DRV_MUTEX_GIVE(h->mutex);

    h->initialized = true;
    return DRV_OK;
}

DRV_Status_t DS1307_Read(DS1307_Handle_t *h, DS1307_DateTime_t *dt)
{
    if (!h->initialized) return DRV_ERR_NOT_INIT;

    uint8_t buf[7];

    DRV_MUTEX_TAKE(h->mutex);

    if (HAL_I2C_Mem_Read(h->hi2c, DS1307_I2C_ADDR, 0x00U,
                         I2C_MEMADD_SIZE_8BIT, buf, 7,
                         DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    DRV_MUTEX_GIVE(h->mutex);

    dt->sec   = buf[0] & 0x7FU;  /* mask CH bit */
    dt->min   = buf[1];
    dt->hour  = buf[2] & 0x3FU;  /* mask 12/24 bit */
    dt->wday  = buf[3];
    dt->mday  = buf[4];
    dt->month = buf[5];
    dt->year  = buf[6];

    return DRV_OK;
}

DRV_Status_t DS1307_Write(DS1307_Handle_t *h, const DS1307_DateTime_t *dt)
{
    if (!h->initialized) return DRV_ERR_NOT_INIT;

    uint8_t buf[7];
    buf[0] = dt->sec   & 0x7FU;  /* ensure CH = 0 */
    buf[1] = dt->min;
    buf[2] = dt->hour  & 0x3FU;  /* ensure 24h mode */
    buf[3] = dt->wday;
    buf[4] = dt->mday;
    buf[5] = dt->month;
    buf[6] = dt->year;

    DRV_MUTEX_TAKE(h->mutex);

    if (HAL_I2C_Mem_Write(h->hi2c, DS1307_I2C_ADDR, 0x00U,
                          I2C_MEMADD_SIZE_8BIT, buf, 7,
                          DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    DRV_MUTEX_GIVE(h->mutex);
    return DRV_OK;
}
