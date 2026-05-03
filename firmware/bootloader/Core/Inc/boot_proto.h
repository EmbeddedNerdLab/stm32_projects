#ifndef BOOT_PROTO_H
#define BOOT_PROTO_H

#include <stdint.h>

/* ── Flash layout ────────────────────────────────────────────────────────── */
#define BL_FLASH_BASE       0x08000000U   /* Sectors 0-1: bootloader (32 KB) */
#define APP_FLASH_BASE      0x08010000U   /* Sectors 4-7: application (448 KB) */
#define APP_FLASH_SIZE      (448U * 1024U)
#define APP_FLASH_END       (APP_FLASH_BASE + APP_FLASH_SIZE)

/* ── AES-128 key (change this per deployment, never transmit) ────────────── */
static const uint8_t k_aes_key[16] = {
    0x2B,0x7E,0x15,0x16,0x28,0xAE,0xD2,0xA6,
    0xAB,0xF7,0x15,0x88,0x09,0xCF,0x4F,0x3C
};

/* ── Update trigger ──────────────────────────────────────────────────────── */
#define UPDATE_MAGIC        0xDEAD5AFEU
/* BKP0R is used by bootloader to track RTC-initialized state (0x32F2).
 * BKP1R is the update flag: app writes UPDATE_MAGIC here, then resets. */
#define RTC_BKP_UPDATE_REG  RTC_BKP_DR1   /* HAL backup register index */

/* ── UART protocol ───────────────────────────────────────────────────────── */
#define PROTO_BAUD          115200U
#define PROTO_CHUNK         256U          /* encrypted payload bytes per DATA frame */
#define PROTO_TIMEOUT_MS    5000U         /* inter-frame timeout */

/* Frame types (1-byte command byte) */
#define CMD_START           0x01U  /* host → device: START frame */
#define CMD_DATA            0x02U  /* host → device: DATA chunk */
#define CMD_END             0x03U  /* host → device: end of firmware */
#define CMD_ACK             0x06U  /* device → host: acknowledge */
#define CMD_NAK             0x15U  /* device → host: negative ack (retry) */
#define CMD_ERR             0xFFU  /* device → host: fatal error, abort */

/* START frame payload (after CMD byte)
 *   nonce[12]       — AES CTR nonce (random, from host)
 *   fw_size[4]      — plaintext firmware size, little-endian
 *   fw_crc32[4]     — CRC32 of plaintext firmware
 *   frame_crc16[2]  — CRC16-CCITT of preceding 20 bytes
 *   Total: 22 bytes
 */
typedef struct __attribute__((packed)) {
    uint8_t  nonce[12];
    uint32_t fw_size;
    uint32_t fw_crc32;
    uint16_t frame_crc16;
} StartFrame_t;

/* DATA frame payload
 *   data[PROTO_CHUNK]   — AES-CTR encrypted chunk (last frame may be shorter)
 *   frame_crc16[2]      — CRC16-CCITT of data bytes only
 */
typedef struct __attribute__((packed)) {
    uint8_t  data[PROTO_CHUNK];
    uint16_t frame_crc16;
} DataFrame_t;

/* ── CRC helpers (in boot_proto.c) ──────────────────────────────────────── */
uint32_t crc32_update(uint32_t crc, const uint8_t *buf, uint32_t len);
uint16_t crc16_update(uint16_t crc, const uint8_t *buf, uint32_t len);

#endif /* BOOT_PROTO_H */
