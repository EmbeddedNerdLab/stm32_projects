#include "eeprom_buffer.h"
#include <string.h>

static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000U)
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            else
                crc <<= 1;
        }
    }
    return crc;
}

static DRV_Status_t write_meta(EepromCtx_t *ctx)
{
    EepromMeta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.magic        = EEPROM_META_MAGIC;
    meta.write_head   = ctx->write_head;
    meta.record_count = ctx->record_count;
    meta.crc16        = crc16_ccitt((uint8_t *)&meta, 12U);

    return EEPROM_Write(ctx->heep, EEPROM_META_ADDR,
                        (uint8_t *)&meta, sizeof(meta));
}

/* Split-safe write: handles records that straddle a 128-byte page boundary */
static DRV_Status_t write_record_raw(EEPROM_Handle_t *heep, uint32_t addr,
                                     const SensorRecord_t *rec)
{
    uint16_t a        = (uint16_t)addr;
    uint32_t page_end = ((uint32_t)(a & ~(EEPROM_PAGE_SIZE - 1U))) + EEPROM_PAGE_SIZE;
    uint16_t first    = (uint16_t)(page_end - addr);

    if (first >= EEPROM_REC_SIZE) {
        return EEPROM_Write(heep, a, (const uint8_t *)rec, EEPROM_REC_SIZE);
    }

    /* Record straddles a page boundary: two writes */
    DRV_Status_t s = EEPROM_Write(heep, a, (const uint8_t *)rec, first);
    if (s != DRV_OK) return s;
    return EEPROM_Write(heep, (uint16_t)(a + first),
                        (const uint8_t *)rec + first,
                        (uint16_t)(EEPROM_REC_SIZE - first));
}

DRV_Status_t EepromBuf_Init(EepromCtx_t *ctx, EEPROM_Handle_t *heep)
{
    ctx->heep         = heep;
    ctx->write_head   = EEPROM_DATA_BASE;
    ctx->record_count = 0;
    ctx->wrapped      = false;

    EepromMeta_t meta;
    DRV_Status_t s = EEPROM_Read(heep, EEPROM_META_ADDR,
                                  (uint8_t *)&meta, sizeof(meta));
    if (s != DRV_OK) return s;

    uint16_t expected_crc = crc16_ccitt((uint8_t *)&meta, 12U);

    if (meta.magic == EEPROM_META_MAGIC && meta.crc16 == expected_crc &&
        meta.write_head >= EEPROM_DATA_BASE &&
        meta.write_head <  EEPROM_RING_END) {
        /* Valid metadata: resume from stored position */
        ctx->write_head   = meta.write_head;
        ctx->record_count = meta.record_count;
    } else {
        /* First boot or corrupted metadata: format */
        s = write_meta(ctx);
    }

    return s;
}

DRV_Status_t EepromBuf_WriteRecord(EepromCtx_t *ctx, const SensorRecord_t *rec)
{
    DRV_Status_t s = write_record_raw(ctx->heep, ctx->write_head, rec);
    if (s != DRV_OK) return s;

    ctx->write_head += EEPROM_REC_SIZE;
    if (ctx->write_head >= EEPROM_RING_END) {
        ctx->write_head = EEPROM_DATA_BASE;
        ctx->wrapped    = true;
    }
    ctx->record_count++;

    /* Persist metadata every 16 records to limit write cycles on page 0 */
    if ((ctx->record_count & 0x0FU) == 0U) {
        s = write_meta(ctx);
    }

    return s;
}
