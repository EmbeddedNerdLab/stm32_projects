#pragma once
#include <QObject>
#include <QSerialPort>
#include "sensordata.h"

class SerialWorker : public QObject {
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker();

public slots:
    void open(const QString &portName, int baud);
    void close();
    void sendRawBytes(const QByteArray &data);

signals:
    void recordReady(SensorRecord rec);
    void connectionChanged(bool connected);
    void errorOccurred(QString msg);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_serial = nullptr;
    QString      m_buf;
    ImuData      m_pendingImu;
    EnvData      m_pendingEnv;
    bool         m_hasImu = false;
    bool         m_hasEnv = false;
};
