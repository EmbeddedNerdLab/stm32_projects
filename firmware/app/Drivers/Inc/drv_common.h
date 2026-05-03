#ifndef DRV_COMMON_H
#define DRV_COMMON_H

#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"

typedef enum {
    DRV_OK               = 0x00,
    DRV_ERR              = 0x01,
    DRV_BUSY             = 0x02,
    DRV_TIMEOUT          = 0x03,
    DRV_ERR_ID_MISMATCH  = 0x10,
    DRV_ERR_MUTEX_TIMEOUT= 0x11,
    DRV_ERR_WIP_TIMEOUT  = 0x12,
    DRV_ERR_PAGE_OVERFLOW= 0x13,
    DRV_ERR_NOT_INIT     = 0x14,
} DRV_Status_t;

#define DRV_MUTEX_TAKE(m) do { \
    if (xSemaphoreTake((m), pdMS_TO_TICKS(100)) != pdTRUE) \
        return DRV_ERR_MUTEX_TIMEOUT; } while(0)

#define DRV_MUTEX_GIVE(m) xSemaphoreGive((m))

#define DRV_I2C_TIMEOUT_MS  50U
#define DRV_SPI_TIMEOUT_MS  50U

#endif /* DRV_COMMON_H */
