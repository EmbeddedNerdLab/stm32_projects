#include "drv_mpu6050.h"

/* MPU6050 register addresses */
#define REG_PWR_MGMT_1  0x6BU
#define REG_ACCEL_XOUT  0x3BU
#define REG_WHO_AM_I    0x75U

/* Both possible 8-bit I2C addresses depending on AD0 pin wiring */
#define MPU6050_ADDR_AD0_HIGH  0xD2U   /* AD0 tied to VCC  → 7-bit 0x69 */
#define MPU6050_ADDR_AD0_LOW   0xD0U   /* AD0 tied to GND  → 7-bit 0x68 */

DRV_Status_t MPU6050_Init(MPU6050_Handle_t *h, I2C_HandleTypeDef *hi2c,
                           SemaphoreHandle_t mutex)
{
    h->hi2c        = hi2c;
    h->mutex       = mutex;
    h->i2c_addr    = 0;
    h->initialized = false;

    DRV_MUTEX_TAKE(h->mutex);

    /* Auto-detect address: try AD0=HIGH (0xD2) first, fall back to AD0=LOW (0xD0).
     * HAL_I2C_IsDeviceReady sends a bare address probe and checks for ACK. */
    const uint8_t candidates[2] = { MPU6050_ADDR_AD0_HIGH, MPU6050_ADDR_AD0_LOW };
    for (int i = 0; i < 2; ++i) {
        if (HAL_I2C_IsDeviceReady(h->hi2c, candidates[i], 3, 20) == HAL_OK) {
            h->i2c_addr = candidates[i];
            break;
        }
    }
    if (h->i2c_addr == 0) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;   /* no ACK on either address → not connected */
    }

    /* Wake the device: clear SLEEP bit in PWR_MGMT_1 */
    uint8_t val = 0x00U;
    if (HAL_I2C_Mem_Write(h->hi2c, h->i2c_addr, REG_PWR_MGMT_1,
                          I2C_MEMADD_SIZE_8BIT, &val, 1,
                          DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    /* Verify identity */
    if (HAL_I2C_Mem_Read(h->hi2c, h->i2c_addr, REG_WHO_AM_I,
                         I2C_MEMADD_SIZE_8BIT, &val, 1,
                         DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    DRV_MUTEX_GIVE(h->mutex);

    if (val != MPU6050_WHO_AM_I) {
        return DRV_ERR_ID_MISMATCH;
    }

    h->initialized = true;
    return DRV_OK;
}

DRV_Status_t MPU6050_ReadAll(MPU6050_Handle_t *h, MPU6050_Data_t *data)
{
    if (!h->initialized) return DRV_ERR_NOT_INIT;

    uint8_t buf[14];

    DRV_MUTEX_TAKE(h->mutex);

    /* Burst read 14 bytes starting at ACCEL_XOUT_H:
     * [0–1]  ACCEL_X, [2–3] ACCEL_Y, [4–5] ACCEL_Z,
     * [6–7]  TEMP,
     * [8–9]  GYRO_X,  [10–11] GYRO_Y, [12–13] GYRO_Z */
    if (HAL_I2C_Mem_Read(h->hi2c, h->i2c_addr, REG_ACCEL_XOUT,
                         I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf),
                         DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    DRV_MUTEX_GIVE(h->mutex);

    data->accel_x = (int16_t)((buf[0]  << 8) | buf[1]);
    data->accel_y = (int16_t)((buf[2]  << 8) | buf[3]);
    data->accel_z = (int16_t)((buf[4]  << 8) | buf[5]);
    data->temp_raw= (int16_t)((buf[6]  << 8) | buf[7]);
    data->gyro_x  = (int16_t)((buf[8]  << 8) | buf[9]);
    data->gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);

    return DRV_OK;
}
