#!/usr/bin/env python3
"""
fw_encrypt.py — AES-128-CTR firmware encryptor + UART uploader for STM32 bootloader.

Usage:
    # Encrypt only (produces .enc file for offline transfer):
    python fw_encrypt.py encrypt app.bin app.enc

    # Encrypt and upload over UART:
    python fw_encrypt.py upload app.bin COM3

    # Upload pre-encrypted file:
    python fw_encrypt.py upload-enc app.enc COM3

Requires: pyserial, pycryptodome  (pip install pyserial pycryptodome)
"""

import argparse
import os
import struct
import sys
import time
import zlib

# ── AES key — must match bootloader/Core/Inc/boot_proto.h k_aes_key ─────────
AES_KEY = bytes([
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C,
])

# ── Protocol constants (must match boot_proto.h) ─────────────────────────────
PROTO_BAUD   = 115200
PROTO_CHUNK  = 256
CMD_START    = 0x01
CMD_DATA     = 0x02
CMD_END      = 0x03
CMD_ACK      = 0x06
CMD_NAK      = 0x15
CMD_ERR      = 0xFF

TIMEOUT_S    = 5.0


# ── CRC helpers ──────────────────────────────────────────────────────────────
def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def crc16_ccitt(data: bytes, init: int = 0xFFFF) -> int:
    crc = init
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
            crc &= 0xFFFF
    return crc


# ── AES-128-CTR encryption ────────────────────────────────────────────────────
def aes_ctr_encrypt(key: bytes, nonce12: bytes, plaintext: bytes) -> bytes:
    try:
        from Crypto.Cipher import AES
        # pycryptodome CTR: nonce=12 bytes, counter starts at 0, big-endian
        cipher = AES.new(key, AES.MODE_CTR, nonce=nonce12,
                         initial_value=0)
        return cipher.encrypt(plaintext)
    except ImportError:
        # Fallback: pure Python AES (slow but dependency-free)
        return _aes_ctr_pure(key, nonce12, plaintext)


def _aes_ctr_pure(key: bytes, nonce12: bytes, data: bytes) -> bytes:
    """Pure-Python AES-128-CTR, no external dependencies."""
    sbox = [
        0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
        0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
        0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
        0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
        0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
        0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
        0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
        0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
        0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
        0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
        0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
        0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
        0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
        0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
        0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
        0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
    ]
    rcon = [0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36]

    def xtime(x):
        return ((x << 1) ^ 0x1b) & 0xff if x & 0x80 else (x << 1) & 0xff

    def key_expand(k):
        rk = list(k)
        for r in range(1, 11):
            prev = rk[(r-1)*16:r*16]
            cur = [0]*16
            cur[0] = sbox[prev[13]] ^ rcon[r] ^ prev[0]
            cur[1] = sbox[prev[14]] ^ prev[1]
            cur[2] = sbox[prev[15]] ^ prev[2]
            cur[3] = sbox[prev[12]] ^ prev[3]
            for j in range(4, 16):
                cur[j] = cur[j-4] ^ prev[j]
            rk.extend(cur)
        return rk

    def encrypt_block(rk, block):
        s = [block[i] ^ rk[i] for i in range(16)]
        for r in range(1, 10):
            base = r * 16
            t = [
                sbox[s[ 0]], sbox[s[ 5]], sbox[s[10]], sbox[s[15]],
                sbox[s[ 4]], sbox[s[ 9]], sbox[s[14]], sbox[s[ 3]],
                sbox[s[ 8]], sbox[s[13]], sbox[s[ 2]], sbox[s[ 7]],
                sbox[s[12]], sbox[s[ 1]], sbox[s[ 6]], sbox[s[11]],
            ]
            for c in range(4):
                a = t[c*4:c*4+4]
                s[c*4+0] = xtime(a[0])^xtime(a[1])^a[1]^a[2]^a[3] ^ rk[base+c*4+0]
                s[c*4+1] = a[0]^xtime(a[1])^xtime(a[2])^a[2]^a[3] ^ rk[base+c*4+1]
                s[c*4+2] = a[0]^a[1]^xtime(a[2])^xtime(a[3])^a[3] ^ rk[base+c*4+2]
                s[c*4+3] = xtime(a[0])^a[0]^a[1]^a[2]^xtime(a[3]) ^ rk[base+c*4+3]
        base = 160
        return bytes([
            sbox[s[ 0]]^rk[base+ 0], sbox[s[ 5]]^rk[base+ 1],
            sbox[s[10]]^rk[base+ 2], sbox[s[15]]^rk[base+ 3],
            sbox[s[ 4]]^rk[base+ 4], sbox[s[ 9]]^rk[base+ 5],
            sbox[s[14]]^rk[base+ 6], sbox[s[ 3]]^rk[base+ 7],
            sbox[s[ 8]]^rk[base+ 8], sbox[s[13]]^rk[base+ 9],
            sbox[s[ 2]]^rk[base+10], sbox[s[ 7]]^rk[base+11],
            sbox[s[12]]^rk[base+12], sbox[s[ 1]]^rk[base+13],
            sbox[s[ 6]]^rk[base+14], sbox[s[11]]^rk[base+15],
        ])

    rk = key_expand(key)
    out = bytearray()
    counter = bytearray(nonce12) + b'\x00\x00\x00\x00'
    for i in range(0, len(data), 16):
        ks = encrypt_block(rk, counter)
        chunk = data[i:i+16]
        out += bytes(a ^ b for a, b in zip(chunk, ks))
        # Increment big-endian 32-bit counter at bytes [12..15]
        for j in range(15, 11, -1):
            counter[j] = (counter[j] + 1) & 0xFF
            if counter[j]:
                break
    return bytes(out)


# ── .enc file format ─────────────────────────────────────────────────────────
# Header (28 bytes):
#   magic[4]   = 0x424C454E  ("BLEN")
#   nonce[12]
#   fw_size[4] (LE, plaintext size)
#   fw_crc32[4] (LE, plaintext CRC32)
#   hdr_crc16[2] (LE, CRC16 of preceding 26 bytes)
#   padding[2]  = 0x0000
# Followed by: encrypted_data (padded to multiple of PROTO_CHUNK)

ENC_MAGIC = 0x424C454E

def encrypt_firmware(fw_data: bytes) -> tuple[bytes, bytes, bytes, int, int]:
    """Returns (nonce12, encrypted, nonce12, fw_size, fw_crc32)."""
    nonce = os.urandom(12)
    fw_size = len(fw_data)
    fw_crc = crc32(fw_data)
    # Pad to multiple of PROTO_CHUNK with 0xFF (flash erased value)
    pad = (-fw_size) % PROTO_CHUNK
    padded = fw_data + b'\xff' * pad
    enc = aes_ctr_encrypt(AES_KEY, nonce, padded)
    return nonce, enc, fw_size, fw_crc


def write_enc_file(path: str, fw_data: bytes):
    nonce, enc, fw_size, fw_crc = encrypt_firmware(fw_data)
    hdr = struct.pack('<I12sII', ENC_MAGIC, nonce, fw_size, fw_crc)
    hdr_crc = crc16_ccitt(hdr)
    hdr += struct.pack('<HH', hdr_crc, 0)  # crc16 + padding
    with open(path, 'wb') as f:
        f.write(hdr)
        f.write(enc)
    print(f"Encrypted {fw_size} bytes -> {path}")
    print(f"  Nonce:   {nonce.hex()}")
    print(f"  CRC32:   0x{fw_crc:08X}")


def read_enc_file(path: str):
    with open(path, 'rb') as f:
        raw = f.read()
    magic, nonce, fw_size, fw_crc, hdr_crc, _ = struct.unpack_from('<I12sIIHH', raw, 0)
    if magic != ENC_MAGIC:
        raise ValueError(f"Bad magic 0x{magic:08X}")
    calc = crc16_ccitt(raw[:26])
    if calc != hdr_crc:
        raise ValueError(f"Header CRC mismatch: got 0x{hdr_crc:04X}, calc 0x{calc:04X}")
    enc = raw[28:]
    return nonce, enc, fw_size, fw_crc


# ── UART upload ───────────────────────────────────────────────────────────────
def wait_ack(ser, timeout=TIMEOUT_S) -> int:
    ser.timeout = timeout
    b = ser.read(1)
    if not b:
        raise TimeoutError("No response from bootloader")
    return b[0]


def upload(port: str, nonce: bytes, enc: bytes, fw_size: int, fw_crc: int):
    import serial
    ser = serial.Serial(port, PROTO_BAUD, timeout=TIMEOUT_S)
    time.sleep(0.1)
    ser.reset_input_buffer()

    print(f"Waiting for bootloader on {port}...")
    resp = wait_ack(ser, 10.0)
    if resp != CMD_ACK:
        raise RuntimeError(f"Expected ACK(0x{CMD_ACK:02X}), got 0x{resp:02X}")
    print("Bootloader ready.")

    # ── Send START frame ──────────────────────────────────────────────────────
    sf_body = nonce + struct.pack('<II', fw_size, fw_crc)
    sf_crc  = struct.pack('<H', crc16_ccitt(sf_body))
    ser.write(bytes([CMD_START]) + sf_body + sf_crc)
    resp = wait_ack(ser)
    if resp == CMD_NAK:
        raise RuntimeError("START frame CRC rejected by bootloader")
    if resp != CMD_ACK:
        raise RuntimeError(f"START unexpected response: 0x{resp:02X}")
    print("START accepted, erasing flash...")

    # ── Send DATA frames ──────────────────────────────────────────────────────
    total_chunks = (len(enc) + PROTO_CHUNK - 1) // PROTO_CHUNK
    for i in range(total_chunks):
        chunk = enc[i*PROTO_CHUNK : (i+1)*PROTO_CHUNK]
        # Always send exactly PROTO_CHUNK bytes (last chunk already padded)
        frame_crc = struct.pack('<H', crc16_ccitt(chunk))
        ser.write(bytes([CMD_DATA]) + chunk + frame_crc)

        resp = wait_ack(ser)
        if resp == CMD_NAK:
            raise RuntimeError(f"Chunk {i+1}/{total_chunks} CRC rejected")
        if resp != CMD_ACK:
            raise RuntimeError(f"Chunk {i+1} unexpected response: 0x{resp:02X}")

        pct = (i + 1) * 100 // total_chunks
        print(f"\r  Uploading... {pct}%  ({i+1}/{total_chunks} chunks)", end='', flush=True)

    print()
    ser.write(bytes([CMD_END]))

    resp = wait_ack(ser)
    if resp == CMD_ACK:
        print("Upload complete! Bootloader verifying CRC32...")
        print("Device will reset and boot new firmware.")
    elif resp == CMD_ERR:
        raise RuntimeError("CRC32 verification FAILED — firmware rejected")
    else:
        raise RuntimeError(f"END unexpected response: 0x{resp:02X}")

    ser.close()


# ── CLI ───────────────────────────────────────────────────────────────────────
def main():
    p = argparse.ArgumentParser(description="STM32 bootloader firmware uploader")
    sub = p.add_subparsers(dest='cmd', required=True)

    enc_p = sub.add_parser('encrypt', help='Encrypt firmware binary to .enc file')
    enc_p.add_argument('input',  help='Input .bin file')
    enc_p.add_argument('output', help='Output .enc file')

    up_p = sub.add_parser('upload', help='Encrypt and upload firmware over UART')
    up_p.add_argument('input', help='Input .bin file')
    up_p.add_argument('port',  help='Serial port (e.g. COM3 or /dev/ttyUSB0)')

    upe_p = sub.add_parser('upload-enc', help='Upload pre-encrypted .enc file over UART')
    upe_p.add_argument('input', help='Input .enc file')
    upe_p.add_argument('port',  help='Serial port')

    args = p.parse_args()

    if args.cmd == 'encrypt':
        with open(args.input, 'rb') as f:
            fw = f.read()
        write_enc_file(args.output, fw)

    elif args.cmd == 'upload':
        with open(args.input, 'rb') as f:
            fw = f.read()
        nonce, enc, fw_size, fw_crc = encrypt_firmware(fw)
        print(f"Firmware: {fw_size} bytes, CRC32=0x{fw_crc:08X}")
        upload(args.port, nonce, enc, fw_size, fw_crc)

    elif args.cmd == 'upload-enc':
        nonce, enc, fw_size, fw_crc = read_enc_file(args.input)
        print(f"Firmware: {fw_size} bytes, CRC32=0x{fw_crc:08X}")
        upload(args.port, nonce, enc, fw_size, fw_crc)


if __name__ == '__main__':
    main()
