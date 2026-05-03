#ifndef DRV_BME280_H
#define DRV_BME280_H

#include "drv_common.h"
#include "stm32f4xx_hal.h"

#define BME280_I2C_ADDR   0xECU   /* 7-bit addr 0x76 shifted left by 1 */
#define BME280_CHIP_ID    0x60U   /* expected chip_id register value    */

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
} BME280_Calib_t;

typedef struct {
    I2C_HandleTypeDef  *hi2c;
    SemaphoreHandle_t   mutex;
    BME280_Calib_t      calib;
    int32_t             t_fine;   /* intermediate value shared by T/P/H compensation */
    bool                initialized;
} BME280_Handle_t;

typedef struct {
    int32_t  temperature_cdeg;   /* Celsius × 100  */
    uint32_t pressure_pa;        /* Pascals         */
    uint16_t humidity_pct256;    /* %RH × 256       */
} BME280_Data_t;

DRV_Status_t BME280_Init(BME280_Handle_t *h, I2C_HandleTypeDef *hi2c,
                          SemaphoreHandle_t mutex);
DRV_Status_t BME280_ReadAll(BME280_Handle_t *h, BME280_Data_t *data);

#endif /* DRV_BME280_H */
