#include "drv_eeprom.h"

static void cs_low(EEPROM_Handle_t *h)
{
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_RESET);
}

static void cs_high(EEPROM_Handle_t *h)
{
    HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_SET);
}

/* Poll WIP bit until clear or timeout. Uses HAL_Delay (works before/after scheduler). */
static DRV_Status_t wait_wip(EEPROM_Handle_t *h)
{
    uint32_t t_start = HAL_GetTick();
    uint8_t  cmd     = EEPROM_CMD_RDSR;
    uint8_t  status;

    while (1) {
        cs_low(h);
        HAL_SPI_Transmit(h->hspi, &cmd, 1, DRV_SPI_TIMEOUT_MS);
        HAL_SPI_Receive(h->hspi, &status, 1, DRV_SPI_TIMEOUT_MS);
        cs_high(h);

        if (!(status & EEPROM_WIP_BIT)) return DRV_OK;

        if ((HAL_GetTick() - t_start) >= 10U) return DRV_ERR_WIP_TIMEOUT;

        HAL_Delay(1);
    }
}

DRV_Status_t EEPROM_Init(EEPROM_Handle_t *h, SPI_HandleTypeDef *hspi,
                          GPIO_TypeDef *cs_port, uint16_t cs_pin,
                          SemaphoreHandle_t mutex)
{
    h->hspi        = hspi;
    h->cs_port     = cs_port;
    h->cs_pin      = cs_pin;
    h->mutex       = mutex;
    h->initialized = false;

    cs_high(h);  /* ensure CS is deasserted */

    /* Verify device is not stuck in a write cycle */
    DRV_MUTEX_TAKE(h->mutex);
    DRV_Status_t s = wait_wip(h);
    DRV_MUTEX_GIVE(h->mutex);

    if (s != DRV_OK) return s;

    h->initialized = true;
    return DRV_OK;
}

DRV_Status_t EEPROM_Read(EEPROM_Handle_t *h, uint16_t addr,
                          uint8_t *buf, uint16_t len)
{
    if (!h->initialized) return DRV_ERR_NOT_INIT;

    uint8_t cmd[3] = {
        EEPROM_CMD_READ,
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFFU)
    };

    DRV_MUTEX_TAKE(h->mutex);

    cs_low(h);
    HAL_SPI_Transmit(h->hspi, cmd, 3, DRV_SPI_TIMEOUT_MS);
    HAL_SPI_Receive(h->hspi, buf, len, DRV_SPI_TIMEOUT_MS);
    cs_high(h);

    DRV_MUTEX_GIVE(h->mutex);
    return DRV_OK;
}

DRV_Status_t EEPROM_Write(EEPROM_Handle_t *h, uint16_t addr,
                           const uint8_t *buf, uint16_t len)
{
    if (!h->initialized)         return DRV_ERR_NOT_INIT;
    if (len > EEPROM_PAGE_SIZE)  return DRV_ERR_PAGE_OVERFLOW;

    uint8_t wren = EEPROM_CMD_WREN;
    uint8_t cmd[3] = {
        EEPROM_CMD_WRITE,
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFFU)
    };

    DRV_MUTEX_TAKE(h->mutex);

    /* Send WREN (write enable latch) as a separate CS transaction */
    cs_low(h);
    HAL_SPI_Transmit(h->hspi, &wren, 1, DRV_SPI_TIMEOUT_MS);
    cs_high(h);

    /* Write command + address + data */
    cs_low(h);
    HAL_SPI_Transmit(h->hspi, cmd, 3, DRV_SPI_TIMEOUT_MS);
    HAL_SPI_Transmit(h->hspi, (uint8_t *)buf, len, DRV_SPI_TIMEOUT_MS);
    cs_high(h);

    /* Poll until write cycle completes (max 5 ms) */
    DRV_Status_t s = wait_wip(h);

    DRV_MUTEX_GIVE(h->mutex);
    return s;
}
