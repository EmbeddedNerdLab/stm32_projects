#ifndef AES128_H
#define AES128_H

#include <stdint.h>

/* AES-128: expanded key (11 round keys × 16 bytes = 176 bytes) */
typedef struct { uint8_t rk[176]; } AES128_ctx;

void AES128_init    (AES128_ctx *ctx, const uint8_t key[16]);
void AES128_ctr_xcrypt(const AES128_ctx *ctx, uint8_t counter[16],
                        const uint8_t *in, uint8_t *out, uint32_t len);

#endif
