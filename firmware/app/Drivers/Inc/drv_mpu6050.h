#ifndef DRV_MPU6050_H
#define DRV_MPU6050_H

#include "drv_common.h"
#include "stm32f4xx_hal.h"

#define MPU6050_WHO_AM_I   0x68U   /* WHO_AM_I register value (fixed, regardless of AD0) */

typedef struct {
    I2C_HandleTypeDef  *hi2c;
    SemaphoreHandle_t   mutex;
    uint8_t             i2c_addr;  /* resolved at init: 0xD0 (AD0=LOW) or 0xD2 (AD0=HIGH) */
    bool                initialized;
} MPU6050_Handle_t;

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temp_raw;
} MPU6050_Data_t;

DRV_Status_t MPU6050_Init(MPU6050_Handle_t *h, I2C_HandleTypeDef *hi2c,
                           SemaphoreHandle_t mutex);
DRV_Status_t MPU6050_ReadAll(MPU6050_Handle_t *h, MPU6050_Data_t *data);

#endif /* DRV_MPU6050_H */
