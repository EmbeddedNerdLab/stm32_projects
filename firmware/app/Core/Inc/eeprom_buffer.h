#ifndef EEPROM_BUFFER_H
#define EEPROM_BUFFER_H

#include "drv_eeprom.h"
#include "sensor_record.h"

/* EEPROM address map for 25AA512 (64 KB = 65536 bytes):
 *   0x0000 – 0x007F : Metadata sector (128 bytes = 1 page)
 *   0x0080 – 0xFF87 : Data ring buffer (65408 bytes = 1635 × 40-byte records)
 *   0xFF88 – 0xFFFF : Unused tail (8 bytes)                                  */

#define EEPROM_META_ADDR      0x0000U
#define EEPROM_DATA_BASE      0x0080U
#define EEPROM_REC_SIZE       ((uint16_t)sizeof(SensorRecord_t))   /* 40 */
#define EEPROM_MAX_RECORDS    ((uint32_t)((65536U - EEPROM_DATA_BASE) / EEPROM_REC_SIZE))
#define EEPROM_RING_END       ((uint32_t)(EEPROM_DATA_BASE + EEPROM_MAX_RECORDS * EEPROM_REC_SIZE))

#define EEPROM_META_MAGIC     0xDEADBEEFU

/* Metadata written at address 0x0000 (128 bytes = 1 page) */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t write_head;    /* byte address of next write slot */
    uint32_t record_count;  /* total records written (wraps) */
    uint16_t crc16;         /* CRC-16/CCITT of bytes [0..11] */
    uint8_t  _reserved[114];
} EepromMeta_t;
#pragma pack(pop)

typedef char _eeprom_meta_size_check[(sizeof(EepromMeta_t) == 128U) ? 1 : -1];

typedef struct {
    EEPROM_Handle_t *heep;
    uint32_t         write_head;
    uint32_t         record_count;
    bool             wrapped;
} EepromCtx_t;

/* Read metadata; format EEPROM if magic invalid. Safe before scheduler starts. */
DRV_Status_t EepromBuf_Init(EepromCtx_t *ctx, EEPROM_Handle_t *heep);

/* Write one record, advance write_head, persist metadata every 16 records.
 * Sets ctx->wrapped = true on first ring wrap.                               */
DRV_Status_t EepromBuf_WriteRecord(EepromCtx_t *ctx, const SensorRecord_t *rec);

#endif /* EEPROM_BUFFER_H */
