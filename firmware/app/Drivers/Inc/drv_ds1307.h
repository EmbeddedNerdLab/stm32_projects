#ifndef DRV_DS1307_H
#define DRV_DS1307_H

#include "drv_common.h"
#include "stm32f4xx_hal.h"

#define DS1307_I2C_ADDR   0xD0U   /* 7-bit addr 0x68 shifted left by 1 */

/* BCD-encoded datetime — matches DS1307 register layout */
typedef struct {
    uint8_t sec;    /* 0x00–0x59 BCD, bit 7 = CH (clock halt) */
    uint8_t min;    /* 0x00–0x59 BCD */
    uint8_t hour;   /* 0x00–0x23 BCD (24h mode, bit 6 must be 0) */
    uint8_t wday;   /* 1–7 */
    uint8_t mday;   /* 0x01–0x31 BCD */
    uint8_t month;  /* 0x01–0x12 BCD */
    uint8_t year;   /* 0x00–0x99 BCD (2000 base) */
} DS1307_DateTime_t;

typedef struct {
    I2C_HandleTypeDef  *hi2c;
    SemaphoreHandle_t   mutex;
    bool                initialized;
} DS1307_Handle_t;

DRV_Status_t DS1307_Init(DS1307_Handle_t *h, I2C_HandleTypeDef *hi2c,
                          SemaphoreHandle_t mutex);
DRV_Status_t DS1307_Read(DS1307_Handle_t *h, DS1307_DateTime_t *dt);
DRV_Status_t DS1307_Write(DS1307_Handle_t *h, const DS1307_DateTime_t *dt);

#endif /* DRV_DS1307_H */
