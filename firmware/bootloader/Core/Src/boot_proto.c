#include "boot_proto.h"

/* CRC32 (ISO 3309 / Ethernet polynomial 0xEDB88320, LSB-first) */
uint32_t crc32_update(uint32_t crc, const uint8_t *buf, uint32_t len)
{
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320U & -(crc & 1U));
    }
    return ~crc;
}

/* CRC16-CCITT (poly 0x1021, init 0xFFFF, no reflection) */
uint16_t crc16_update(uint16_t crc, const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int b = 0; b < 8; b++)
            crc = (crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U);
    }
    return crc;
}
