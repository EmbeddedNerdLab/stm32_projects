#include "drv_bme280.h"

/* Register addresses */
#define REG_CHIP_ID     0xD0U
#define REG_RESET       0xE0U
#define REG_CTRL_HUM    0xF2U
#define REG_STATUS      0xF3U
#define REG_CTRL_MEAS   0xF4U
#define REG_CONFIG      0xF5U
#define REG_DATA        0xF7U   /* 0xF7–0xFE: press(3)+temp(3)+hum(2) */
#define REG_CALIB_TP    0x88U   /* 0x88–0x9F: T1-3, P1-9 (24 bytes) */
#define REG_CALIB_H1    0xA1U
#define REG_CALIB_H26   0xE1U   /* 0xE1–0xE7: H2-6 (7 bytes) */

#define RESET_VALUE     0xB6U

/* ── Bosch compensation formulas (integer, from datasheet) ────────────────── */

static int32_t compensate_temperature(BME280_Handle_t *h, int32_t adc_T)
{
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)h->calib.dig_T1 << 1))) *
                    ((int32_t)h->calib.dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)h->calib.dig_T1)) *
                      ((adc_T >> 4) - ((int32_t)h->calib.dig_T1))) >> 12) *
                    ((int32_t)h->calib.dig_T3)) >> 14;
    h->t_fine = var1 + var2;
    return (h->t_fine * 5 + 128) >> 8;  /* result in 0.01 °C units */
}

static uint32_t compensate_pressure(BME280_Handle_t *h, int32_t adc_P)
{
    int64_t var1 = ((int64_t)h->t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)h->calib.dig_P6;
    var2 += ((var1 * (int64_t)h->calib.dig_P5) << 17);
    var2 += ((int64_t)h->calib.dig_P4 << 35);
    var1  = ((var1 * var1 * (int64_t)h->calib.dig_P3) >> 8) +
            ((var1 * (int64_t)h->calib.dig_P2) << 12);
    var1  = (((((int64_t)1) << 47) + var1) * (int64_t)h->calib.dig_P1) >> 33;
    if (var1 == 0) return 0;
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)h->calib.dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)h->calib.dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)h->calib.dig_P7 << 4);
    return (uint32_t)(p >> 8);  /* result in Pa */
}

static uint32_t compensate_humidity(BME280_Handle_t *h, int32_t adc_H)
{
    int32_t v = h->t_fine - 76800;
    v = (((((adc_H << 14) - ((int32_t)h->calib.dig_H4 << 20) -
           ((int32_t)h->calib.dig_H5 * v)) + 16384) >> 15) *
         (((((((v * (int32_t)h->calib.dig_H6) >> 10) *
             (((v * (int32_t)h->calib.dig_H3) >> 11) + 32768)) >> 10) +
            2097152) * (int32_t)h->calib.dig_H2 + 8192) >> 14));
    v -= (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)h->calib.dig_H1) >> 4);
    if (v < 0) v = 0;
    if (v > 419430400) v = 419430400;
    return (uint32_t)(v >> 12);  /* Q22.10: %RH = value / 1024 */
}

/* ── Load calibration ─────────────────────────────────────────────────────── */

static DRV_Status_t load_calibration(BME280_Handle_t *h)
{
    uint8_t buf[24];

    /* Temperature and pressure calibration (0x88–0x9F, 24 bytes) */
    if (HAL_I2C_Mem_Read(h->hi2c, BME280_I2C_ADDR, REG_CALIB_TP,
                         I2C_MEMADD_SIZE_8BIT, buf, 24,
                         DRV_I2C_TIMEOUT_MS) != HAL_OK) return DRV_ERR;

    h->calib.dig_T1 = (uint16_t)(buf[1]  << 8 | buf[0]);
    h->calib.dig_T2 = (int16_t) (buf[3]  << 8 | buf[2]);
    h->calib.dig_T3 = (int16_t) (buf[5]  << 8 | buf[4]);
    h->calib.dig_P1 = (uint16_t)(buf[7]  << 8 | buf[6]);
    h->calib.dig_P2 = (int16_t) (buf[9]  << 8 | buf[8]);
    h->calib.dig_P3 = (int16_t) (buf[11] << 8 | buf[10]);
    h->calib.dig_P4 = (int16_t) (buf[13] << 8 | buf[12]);
    h->calib.dig_P5 = (int16_t) (buf[15] << 8 | buf[14]);
    h->calib.dig_P6 = (int16_t) (buf[17] << 8 | buf[16]);
    h->calib.dig_P7 = (int16_t) (buf[19] << 8 | buf[18]);
    h->calib.dig_P8 = (int16_t) (buf[21] << 8 | buf[20]);
    h->calib.dig_P9 = (int16_t) (buf[23] << 8 | buf[22]);

    /* Humidity calibration part 1 (0xA1, 1 byte) */
    if (HAL_I2C_Mem_Read(h->hi2c, BME280_I2C_ADDR, REG_CALIB_H1,
                         I2C_MEMADD_SIZE_8BIT, buf, 1,
                         DRV_I2C_TIMEOUT_MS) != HAL_OK) return DRV_ERR;
    h->calib.dig_H1 = buf[0];

    /* Humidity calibration part 2 (0xE1–0xE7, 7 bytes) */
    if (HAL_I2C_Mem_Read(h->hi2c, BME280_I2C_ADDR, REG_CALIB_H26,
                         I2C_MEMADD_SIZE_8BIT, buf, 7,
                         DRV_I2C_TIMEOUT_MS) != HAL_OK) return DRV_ERR;

    h->calib.dig_H2 = (int16_t)(buf[1] << 8 | buf[0]);
    h->calib.dig_H3 = buf[2];
    h->calib.dig_H4 = (int16_t)((buf[3] << 4) | (buf[4] & 0x0FU));
    h->calib.dig_H5 = (int16_t)((buf[5] << 4) | ((buf[4] >> 4) & 0x0FU));
    h->calib.dig_H6 = (int8_t)buf[6];

    return DRV_OK;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

DRV_Status_t BME280_Init(BME280_Handle_t *h, I2C_HandleTypeDef *hi2c,
                          SemaphoreHandle_t mutex)
{
    h->hi2c  = hi2c;
    h->mutex = mutex;
    h->t_fine = 0;
    h->initialized = false;

    DRV_MUTEX_TAKE(h->mutex);

    /* Soft reset */
    uint8_t val = RESET_VALUE;
    if (HAL_I2C_Mem_Write(h->hi2c, BME280_I2C_ADDR, REG_RESET,
                          I2C_MEMADD_SIZE_8BIT, &val, 1,
                          DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }
    HAL_Delay(3);  /* startup time after reset: max 2 ms */

    /* Verify chip ID */
    if (HAL_I2C_Mem_Read(h->hi2c, BME280_I2C_ADDR, REG_CHIP_ID,
                         I2C_MEMADD_SIZE_8BIT, &val, 1,
                         DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }
    if (val != BME280_CHIP_ID) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR_ID_MISMATCH;
    }

    /* Load calibration data */
    if (load_calibration(h) != DRV_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    /* Configure oversampling and normal mode.
     * ctrl_hum (0xF2) must be written before ctrl_meas (0xF4).
     * osrs_h = 1x; osrs_t = 1x; osrs_p = 1x; mode = normal (0b11)
     * ctrl_meas = 0b001_001_11 = 0x27 */
    val = 0x01U;
    if (HAL_I2C_Mem_Write(h->hi2c, BME280_I2C_ADDR, REG_CTRL_HUM,
                          I2C_MEMADD_SIZE_8BIT, &val, 1,
                          DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }
    val = 0x27U;
    if (HAL_I2C_Mem_Write(h->hi2c, BME280_I2C_ADDR, REG_CTRL_MEAS,
                          I2C_MEMADD_SIZE_8BIT, &val, 1,
                          DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    DRV_MUTEX_GIVE(h->mutex);

    h->initialized = true;
    return DRV_OK;
}

DRV_Status_t BME280_ReadAll(BME280_Handle_t *h, BME280_Data_t *data)
{
    if (!h->initialized) return DRV_ERR_NOT_INIT;

    uint8_t buf[8];

    DRV_MUTEX_TAKE(h->mutex);

    /* Read 8 bytes from 0xF7: press_msb/lsb/xlsb, temp_msb/lsb/xlsb, hum_msb/lsb */
    if (HAL_I2C_Mem_Read(h->hi2c, BME280_I2C_ADDR, REG_DATA,
                         I2C_MEMADD_SIZE_8BIT, buf, 8,
                         DRV_I2C_TIMEOUT_MS) != HAL_OK) {
        DRV_MUTEX_GIVE(h->mutex);
        return DRV_ERR;
    }

    DRV_MUTEX_GIVE(h->mutex);

    int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
    int32_t adc_H = ((int32_t)buf[6] << 8)  |  buf[7];

    /* Temperature must be computed first — it sets h->t_fine for P and H */
    int32_t  T = compensate_temperature(h, adc_T);
    uint32_t P = compensate_pressure(h, adc_P);
    uint32_t H = compensate_humidity(h, adc_H);

    data->temperature_cdeg = T;
    data->pressure_pa      = P;
    /* Convert Q22.10 humidity (%RH * 1024) to %RH * 256 (divide by 4) */
    data->humidity_pct256  = (uint16_t)(H >> 2);

    return DRV_OK;
}
