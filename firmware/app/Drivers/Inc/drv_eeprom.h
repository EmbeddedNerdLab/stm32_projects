#ifndef DRV_EEPROM_H
#define DRV_EEPROM_H

#include "drv_common.h"
#include "stm32f4xx_hal.h"

/* 25AA512 / 25LC512 — 64 KB SPI EEPROM */
#define EEPROM_CMD_READ   0x03U
#define EEPROM_CMD_WRITE  0x02U
#define EEPROM_CMD_WREN   0x06U
#define EEPROM_CMD_RDSR   0x05U
#define EEPROM_PAGE_SIZE  128U
#define EEPROM_WIP_BIT    0x01U   /* STATUS register bit 0 */

typedef struct {
    SPI_HandleTypeDef  *hspi;
    GPIO_TypeDef       *cs_port;
    uint16_t            cs_pin;
    SemaphoreHandle_t   mutex;
    bool                initialized;
} EEPROM_Handle_t;

DRV_Status_t EEPROM_Init(EEPROM_Handle_t *h, SPI_HandleTypeDef *hspi,
                          GPIO_TypeDef *cs_port, uint16_t cs_pin,
                          SemaphoreHandle_t mutex);

/* Read up to any byte count from any address (no page restriction on reads) */
DRV_Status_t EEPROM_Read(EEPROM_Handle_t *h, uint16_t addr,
                          uint8_t *buf, uint16_t len);

/* Write up to EEPROM_PAGE_SIZE bytes within a single page.
 * Caller must not cross a page boundary in one call.
 * Issues WREN, writes, then polls WIP until complete. */
DRV_Status_t EEPROM_Write(EEPROM_Handle_t *h, uint16_t addr,
                           const uint8_t *buf, uint16_t len);

#endif /* DRV_EEPROM_H */
