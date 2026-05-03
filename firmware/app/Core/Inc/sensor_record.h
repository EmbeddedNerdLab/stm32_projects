#ifndef SENSOR_RECORD_H
#define SENSOR_RECORD_H

#include <stdint.h>

/* Packed 40-byte sensor record written to EEPROM.
 * CRC-16/CCITT covers bytes [0..37] (all fields except crc16 and _reserved). */
#pragma pack(push, 1)
typedef struct {
    /* DS1307 timestamp — BCD encoded (7 bytes) */
    uint8_t  sec;
    uint8_t  min;
    uint8_t  hour;
    uint8_t  wday;
    uint8_t  mday;
    uint8_t  month;
    uint8_t  year;

    /* MPU6050 raw ADC values — 16-bit signed (12 bytes) */
    int16_t  accel_x;
    int16_t  accel_y;
    int16_t  accel_z;
    int16_t  gyro_x;
    int16_t  gyro_y;
    int16_t  gyro_z;

    /* BME280 pre-compensated, fixed-point (10 bytes) */
    int32_t  temperature_cdeg;   /* Celsius × 100  (e.g. 2315 = 23.15 °C) */
    uint32_t pressure_pa;        /* Pascals         (e.g. 101325)          */
    uint16_t humidity_pct256;    /* %RH × 256 (Q8 format)                  */

    /* BH1750 raw lux count (2 bytes); actual lux = value / 1.2 */
    uint16_t lux;

    /* CRC-16/CCITT over bytes [0..30] (2 bytes) */
    uint16_t crc16;

    /* Pad to 40 bytes */
    uint8_t  _reserved[7];
} SensorRecord_t;
#pragma pack(pop)

/* Compile-time size check */
typedef char _sensor_record_size_check[(sizeof(SensorRecord_t) == 40U) ? 1 : -1];

#endif /* SENSOR_RECORD_H */
