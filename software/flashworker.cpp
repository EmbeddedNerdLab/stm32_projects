#include "flashworker.h"
#include <QSerialPort>
#include <QFile>
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

// ── .enc file format (produced by firmware/tools/fw_encrypt.py) ──────────────
// Offset  Size  Field
//   0      4    magic = 0x424C454E ("BLEN")
//   4     12    nonce (AES-CTR nonce, random per build)
//  16      4    fw_size  (LE, plaintext byte count)
//  20      4    fw_crc32 (LE, CRC32 of plaintext)
//  24      2    hdr_crc16 (CRC16-CCITT of bytes 0..25)
//  26      2    padding = 0x0000
//  28      *    encrypted firmware (multiple of PROTO_CHUNK)
static const quint32 ENC_MAGIC    = 0x424C454EU;
static const int     ENC_HDR_SIZE = 28;

// ── CRC helpers ───────────────────────────────────────────────────────────────
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
FlashWorker::FlashWorker(QString port, QString encPath, QObject *parent)
    : QObject(parent), m_port(port), m_binPath(encPath) {}

// ── Main upload procedure ─────────────────────────────────────────────────────
void FlashWorker::run()
{
    // ── Read and validate .enc file ───────────────────────────────────────────
    QFile f(m_binPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit finished(false, "Cannot open file: " + m_binPath);
        return;
    }
    QByteArray raw = f.readAll();
    f.close();

    if (raw.size() < ENC_HDR_SIZE) {
        emit finished(false, "File too small to be a valid .enc file");
        return;
    }

    // Parse header fields (all little-endian)
    auto le32 = [&](int off) -> quint32 {
        return (quint8)raw[off] | ((quint8)raw[off+1] << 8) |
               ((quint8)raw[off+2] << 16) | ((quint8)raw[off+3] << 24);
    };
    auto le16 = [&](int off) -> quint16 {
        return (quint8)raw[off] | ((quint8)raw[off+1] << 8);
    };

    quint32 magic         = le32(0);
    QByteArray nonce      = raw.mid(4, 12);
    quint32 fw_size       = le32(16);
    quint32 fw_crc        = le32(20);
    quint16 hdr_crc_stored = le16(24);

    if (magic != ENC_MAGIC) {
        emit finished(false, QString("Not a valid .enc file (bad magic 0x%1)").arg(magic, 8, 16, QChar('0')));
        return;
    }

    quint16 hdr_crc_calc = crc16(raw.left(24));
    if (hdr_crc_calc != hdr_crc_stored) {
        emit finished(false, "Header CRC mismatch — .enc file is corrupted");
        return;
    }

    QByteArray enc = raw.mid(ENC_HDR_SIZE);
    if (enc.isEmpty() || enc.size() % PROTO_CHUNK != 0) {
        emit finished(false, "Encrypted payload size is invalid");
        return;
    }

    emit progress(5, QString("Loaded %1 bytes firmware  CRC32=0x%2  nonce=%3")
                  .arg(fw_size)
                  .arg(fw_crc, 8, 16, QChar('0'))
                  .arg(QString(nonce.toHex())));

    // ── Open serial port ──────────────────────────────────────────────────────
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

    auto readByte = [&](int timeout_ms) -> int {
        if (serial.bytesAvailable() > 0 || serial.waitForReadyRead(timeout_ms)) {
            QByteArray d = serial.read(1);
            if (!d.isEmpty()) return (quint8)d[0];
        }
        return -1;
    };

    // ── Wait for bootloader ACK ───────────────────────────────────────────────
    emit progress(6, "Waiting for bootloader ACK (up to 10 s)...");
    bool gotAck = false;
    QElapsedTimer ackTimer;
    ackTimer.start();
    while (ackTimer.elapsed() < 10000 && !gotAck) {
        if (serial.bytesAvailable() > 0) {
            for (char c : serial.readAll())
                if ((quint8)c == CMD_ACK) { gotAck = true; break; }
        } else {
            serial.waitForReadyRead(100);
        }
    }
    if (!gotAck) {
        serial.close();
        emit finished(false, "No ACK from bootloader — is the device in bootloader mode?");
        return;
    }
    emit progress(10, "Bootloader ready.");

    // ── Send START frame: nonce(12) + fw_size(4 LE) + fw_crc32(4 LE) + crc16(2 LE)
    int resp = -1;
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
    if (resp == CMD_NAK) { serial.close(); emit finished(false, "START frame CRC rejected"); return; }
    if (resp != CMD_ACK) { serial.close(); emit finished(false, QString("START: unexpected 0x%1").arg((quint8)resp,2,16,QChar('0'))); return; }
    emit progress(13, "START accepted — erasing flash sectors 4-7 (please wait)...");

    resp = readByte(15000);
    if (resp != CMD_ACK) {
        serial.close();
        emit finished(false, QString("Erase ACK: unexpected 0x%1").arg((quint8)resp,2,16,QChar('0')));
        return;
    }
    emit progress(15, "Flash erased — uploading firmware...");

    // ── Send DATA frames (already encrypted) ──────────────────────────────────
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
        if (resp == CMD_NAK) { serial.close(); emit finished(false, QString("Chunk %1/%2 CRC rejected").arg(i+1).arg(total_chunks)); return; }
        if (resp != CMD_ACK) { serial.close(); emit finished(false, QString("Chunk %1/%2: unexpected 0x%3").arg(i+1).arg(total_chunks).arg((quint8)resp,2,16,QChar('0'))); return; }

        emit progress(15 + (i+1)*80/total_chunks,
                      QString("Uploading... %1/%2 chunks  (%3 KB)")
                      .arg(i+1).arg(total_chunks).arg((i+1)*PROTO_CHUNK/1024.0,0,'f',1));
    }

    // ── Send END ──────────────────────────────────────────────────────────────
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
        emit finished(false, QString("END: unexpected response 0x%1").arg((quint8)resp,2,16,QChar('0')));
    }
}
