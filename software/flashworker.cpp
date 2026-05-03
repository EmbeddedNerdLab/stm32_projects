#include "flashworker.h"
#include <QSerialPort>
#include <QFile>
#include <QRandomGenerator>
#include <QElapsedTimer>

// ── Protocol constants (must match bootloader/Core/Inc/boot_proto.h) ─────────
static const int     PROTO_BAUD  = 115200;
static const int     PROTO_CHUNK = 256;
static const quint8  CMD_START   = 0x01;
static const quint8  CMD_DATA    = 0x02;
static const quint8  CMD_END     = 0x03;
static const quint8  CMD_ACK     = 0x06;
static const quint8  CMD_NAK     = 0x15;
static const quint8  CMD_ERR     = 0xFF;

static const QByteArray AES_KEY = QByteArray::fromHex("2B7E151628AED2A6ABF7158809CF4F3C");

// ── AES-128 S-box ─────────────────────────────────────────────────────────────
static const quint8 SBOX[256] = {
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
};
static const quint8 RCON[11] = {0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};

static inline quint8 xtime(quint8 x) {
    return (x & 0x80) ? ((x << 1) ^ 0x1b) : (x << 1);
}

static void keyExpand(const quint8 *key, quint8 rk[176])
{
    memcpy(rk, key, 16);
    for (int r = 1; r <= 10; r++) {
        const quint8 *prev = rk + (r-1)*16;
        quint8       *cur  = rk + r*16;
        cur[0]  = SBOX[prev[13]] ^ RCON[r] ^ prev[0];
        cur[1]  = SBOX[prev[14]] ^ prev[1];
        cur[2]  = SBOX[prev[15]] ^ prev[2];
        cur[3]  = SBOX[prev[12]] ^ prev[3];
        for (int j = 4; j < 16; j++)
            cur[j] = cur[j-4] ^ prev[j];
    }
}

static void encryptBlock(const quint8 rk[176], const quint8 in[16], quint8 out[16])
{
    quint8 s[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[i];

    for (int r = 1; r < 10; r++) {
        const quint8 *k = rk + r*16;
        quint8 t[16] = {
            SBOX[s[ 0]], SBOX[s[ 5]], SBOX[s[10]], SBOX[s[15]],
            SBOX[s[ 4]], SBOX[s[ 9]], SBOX[s[14]], SBOX[s[ 3]],
            SBOX[s[ 8]], SBOX[s[13]], SBOX[s[ 2]], SBOX[s[ 7]],
            SBOX[s[12]], SBOX[s[ 1]], SBOX[s[ 6]], SBOX[s[11]],
        };
        for (int c = 0; c < 4; c++) {
            quint8 a=t[c*4],b=t[c*4+1],cc=t[c*4+2],d=t[c*4+3];
            s[c*4+0] = xtime(a)^xtime(b)^b^cc^d ^ k[c*4+0];
            s[c*4+1] = a^xtime(b)^xtime(cc)^cc^d ^ k[c*4+1];
            s[c*4+2] = a^b^xtime(cc)^xtime(d)^d  ^ k[c*4+2];
            s[c*4+3] = xtime(a)^a^b^cc^xtime(d)  ^ k[c*4+3];
        }
    }
    const quint8 *k = rk + 160;
    out[ 0]=SBOX[s[ 0]]^k[ 0]; out[ 1]=SBOX[s[ 5]]^k[ 1];
    out[ 2]=SBOX[s[10]]^k[ 2]; out[ 3]=SBOX[s[15]]^k[ 3];
    out[ 4]=SBOX[s[ 4]]^k[ 4]; out[ 5]=SBOX[s[ 9]]^k[ 5];
    out[ 6]=SBOX[s[14]]^k[ 6]; out[ 7]=SBOX[s[ 3]]^k[ 7];
    out[ 8]=SBOX[s[ 8]]^k[ 8]; out[ 9]=SBOX[s[13]]^k[ 9];
    out[10]=SBOX[s[ 2]]^k[10]; out[11]=SBOX[s[ 7]]^k[11];
    out[12]=SBOX[s[12]]^k[12]; out[13]=SBOX[s[ 1]]^k[13];
    out[14]=SBOX[s[ 6]]^k[14]; out[15]=SBOX[s[11]]^k[15];
}

QByteArray FlashWorker::aesCtrEncrypt(const QByteArray &key,
                                      const QByteArray &nonce12,
                                      const QByteArray &data)
{
    quint8 rk[176];
    keyExpand(reinterpret_cast<const quint8*>(key.constData()), rk);

    quint8 counter[16];
    memcpy(counter, nonce12.constData(), 12);
    counter[12] = counter[13] = counter[14] = counter[15] = 0;

    QByteArray out(data.size(), 0);
    const quint8 *in = reinterpret_cast<const quint8*>(data.constData());
    quint8       *o  = reinterpret_cast<quint8*>(out.data());

    for (int i = 0; i < data.size(); i += 16) {
        quint8 ks[16];
        encryptBlock(rk, counter, ks);
        int n = qMin(16, data.size() - i);
        for (int j = 0; j < n; j++)
            o[i+j] = in[i+j] ^ ks[j];
        // increment big-endian 32-bit counter at bytes [12..15]
        for (int j = 15; j >= 12; j--) {
            if (++counter[j]) break;
        }
    }
    return out;
}

quint32 FlashWorker::crc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFU;
    for (quint8 b : data) {
        crc ^= b;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320U & -(crc & 1U));
    }
    return ~crc;
}

quint16 FlashWorker::crc16(const QByteArray &data, quint16 init)
{
    quint16 crc = init;
    for (quint8 b : data) {
        crc ^= (quint16)b << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U);
    }
    return crc;
}

// ── Constructor ───────────────────────────────────────────────────────────────
FlashWorker::FlashWorker(QString port, QString binPath, QObject *parent)
    : QObject(parent), m_port(port), m_binPath(binPath) {}

// ── Main upload procedure ─────────────────────────────────────────────────────
void FlashWorker::run()
{
    // Read firmware binary
    QFile f(m_binPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit finished(false, "Cannot open file: " + m_binPath);
        return;
    }
    QByteArray fw = f.readAll();
    f.close();

    if (fw.isEmpty()) {
        emit finished(false, "Firmware file is empty");
        return;
    }

    emit progress(2, QString("Read %1 bytes from %2").arg(fw.size()).arg(m_binPath));

    // Generate nonce + compute CRC32 before padding
    QByteArray nonce(12, 0);
    for (int i = 0; i < 12; i++)
        nonce[i] = (quint8)(QRandomGenerator::global()->generate() & 0xFF);

    quint32 fw_size = (quint32)fw.size();
    quint32 fw_crc  = crc32(fw);

    emit progress(3, QString("CRC32 = 0x%1, nonce = %2")
                  .arg(fw_crc, 8, 16, QChar('0'))
                  .arg(QString(nonce.toHex())));

    // Pad to multiple of PROTO_CHUNK with 0xFF (erased flash value)
    int pad = (PROTO_CHUNK - (fw.size() % PROTO_CHUNK)) % PROTO_CHUNK;
    QByteArray padded = fw + QByteArray(pad, (char)0xFF);

    emit progress(5, "Encrypting firmware (AES-128-CTR)...");
    QByteArray enc = aesCtrEncrypt(AES_KEY, nonce, padded);

    // Open serial port
    QSerialPort serial;
    serial.setPortName(m_port);
    serial.setBaudRate(PROTO_BAUD);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial.open(QIODevice::ReadWrite)) {
        emit finished(false, "Cannot open port " + m_port + ": " + serial.errorString());
        return;
    }
    serial.clear();

    // Helper: read one byte with timeout
    auto readByte = [&](int timeout_ms) -> int {
        if (serial.bytesAvailable() > 0 || serial.waitForReadyRead(timeout_ms)) {
            QByteArray d = serial.read(1);
            if (!d.isEmpty()) return (quint8)d[0];
        }
        return -1;
    };

    // Wait for bootloader ACK (0x06) for up to 10 s.
    // The app streams sensor lines continuously (~130 B every 50 ms), so we
    // must drain all incoming bytes in bulk and scan for 0x06 rather than
    // checking one byte at a time with a fixed iteration count.
    emit progress(6, "Waiting for bootloader ACK (up to 10 s)...");
    bool gotAck = false;
    QElapsedTimer ackTimer;
    ackTimer.start();
    while (ackTimer.elapsed() < 10000 && !gotAck) {
        if (serial.bytesAvailable() > 0) {
            QByteArray chunk = serial.readAll();
            for (char c : chunk) {
                if ((quint8)c == CMD_ACK) { gotAck = true; break; }
            }
        } else {
            serial.waitForReadyRead(100);
        }
    }

    if (!gotAck) {
        serial.close();
        emit finished(false, "No ACK from bootloader. Check that the device is connected and the firmware is running.");
        return;
    }
    emit progress(10, "Bootloader ready.");

    // From here the protocol is strictly request/response — single bytes only.
    // readByte is used for all subsequent ACK/NAK/ERR responses.
    int resp = -1;

    // Build and send START frame
    // Body: nonce(12) + fw_size(4 LE) + fw_crc32(4 LE)
    QByteArray sfBody;
    sfBody.append(nonce);
    sfBody.append((char)(fw_size >>  0)); sfBody.append((char)(fw_size >>  8));
    sfBody.append((char)(fw_size >> 16)); sfBody.append((char)(fw_size >> 24));
    sfBody.append((char)(fw_crc  >>  0)); sfBody.append((char)(fw_crc  >>  8));
    sfBody.append((char)(fw_crc  >> 16)); sfBody.append((char)(fw_crc  >> 24));

    quint16 sfCrc = crc16(sfBody);
    QByteArray startFrame;
    startFrame.append((char)CMD_START);
    startFrame.append(sfBody);
    startFrame.append((char)(sfCrc >> 0));
    startFrame.append((char)(sfCrc >> 8));

    serial.write(startFrame);
    serial.flush();

    resp = readByte(5000);
    if (resp == CMD_NAK) {
        serial.close();
        emit finished(false, "START frame rejected (CRC mismatch in header)");
        return;
    }
    if (resp != CMD_ACK) {
        serial.close();
        emit finished(false, QString("START: unexpected 0x%1").arg((quint8)resp, 2, 16, QChar('0')));
        return;
    }
    emit progress(13, "START accepted — erasing flash sectors 4-7 (please wait)...");

    // Wait for second ACK: bootloader sends it after erase completes.
    // Erase of 4 × 128 KB sectors can take up to 6 s; use a 15 s timeout.
    resp = readByte(15000);
    if (resp != CMD_ACK) {
        serial.close();
        emit finished(false, QString("Erase ACK: unexpected 0x%1 (erase may have failed)")
                      .arg((quint8)resp, 2, 16, QChar('0')));
        return;
    }
    emit progress(15, "Flash erased — uploading firmware...");

    // Send DATA frames
    int total_chunks = enc.size() / PROTO_CHUNK;
    for (int i = 0; i < total_chunks; i++) {
        QByteArray chunk = enc.mid(i * PROTO_CHUNK, PROTO_CHUNK);
        quint16 chunkCrc = crc16(chunk);

        QByteArray frame;
        frame.append((char)CMD_DATA);
        frame.append(chunk);
        frame.append((char)(chunkCrc >> 0));
        frame.append((char)(chunkCrc >> 8));

        serial.write(frame);
        serial.flush();

        resp = readByte(5000);
        if (resp == CMD_NAK) {
            serial.close();
            emit finished(false, QString("Chunk %1/%2 CRC rejected").arg(i+1).arg(total_chunks));
            return;
        }
        if (resp != CMD_ACK) {
            serial.close();
            emit finished(false, QString("Chunk %1/%2: unexpected 0x%3")
                          .arg(i+1).arg(total_chunks).arg((quint8)resp, 2, 16, QChar('0')));
            return;
        }

        int pct = 15 + (i + 1) * 80 / total_chunks;
        emit progress(pct, QString("Uploading... %1 / %2 chunks  (%3 KB)")
                      .arg(i+1).arg(total_chunks).arg((i+1)*PROTO_CHUNK/1024.0, 0, 'f', 1));
    }

    // Send END
    serial.write(QByteArray(1, (char)CMD_END));
    serial.flush();

    resp = readByte(5000);
    serial.close();

    if (resp == CMD_ACK) {
        emit progress(100, "CRC32 verified — device rebooting into new firmware.");
        emit finished(true, "Firmware update successful!");
    } else if (resp == CMD_ERR) {
        emit finished(false, "CRC32 verification FAILED — bootloader rejected the firmware.");
    } else {
        emit finished(false, QString("END: unexpected response 0x%1").arg((quint8)resp, 2, 16, QChar('0')));
    }
}
