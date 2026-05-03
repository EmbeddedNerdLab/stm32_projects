#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>

class FlashWorker : public QObject {
    Q_OBJECT
public:
    explicit FlashWorker(QString port, QString binPath, QObject *parent = nullptr);

public slots:
    void run();

signals:
    void progress(int pct, QString status);
    void finished(bool success, QString msg);

private:
    QString    m_port;
    QString    m_binPath;

    static QByteArray aesCtrEncrypt(const QByteArray &key,
                                    const QByteArray &nonce12,
                                    const QByteArray &data);
    static quint32    crc32(const QByteArray &data);
    static quint16    crc16(const QByteArray &data, quint16 init = 0xFFFF);
};
