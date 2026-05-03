#include "serialworker.h"
#include "dataparser.h"
#include <QDateTime>

SerialWorker::SerialWorker(QObject *parent) : QObject(parent) {}

SerialWorker::~SerialWorker()
{
    if (m_serial && m_serial->isOpen())
        m_serial->close();
}

void SerialWorker::open(const QString &portName, int baud)
{
    if (m_serial) {
        m_serial->close();
        m_serial->deleteLater();
        m_serial = nullptr;
    }

    m_serial = new QSerialPort(this);
    m_serial->setPortName(portName);
    m_serial->setBaudRate(baud);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    connect(m_serial, &QSerialPort::readyRead,
            this, &SerialWorker::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred,
            this, &SerialWorker::onErrorOccurred);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_serial->errorString());
        emit connectionChanged(false);
        return;
    }

    m_buf.clear();
    m_hasImu = false;
    m_hasEnv = false;
    emit connectionChanged(true);
}

void SerialWorker::close()
{
    if (m_serial && m_serial->isOpen()) {
        m_serial->close();
        emit connectionChanged(false);
    }
}

void SerialWorker::onReadyRead()
{
    m_buf += QString::fromLatin1(m_serial->readAll());

    int pos;
    while ((pos = m_buf.indexOf('\n')) != -1) {
        QString line = m_buf.left(pos).trimmed();
        m_buf.remove(0, pos + 1);

        ImuData imu;
        EnvData env;
        int     dist_cm = 0;

        if (DataParser::parseImu(line, imu)) {
            // If a complete IMU+ENV pair is pending but DIST never arrived
            // (old firmware or dropped line), emit it now before starting new cycle
            if (m_hasImu && m_hasEnv) {
                SensorRecord rec;
                rec.ts  = QDateTime::currentDateTime();
                rec.imu = m_pendingImu;
                rec.env = m_pendingEnv;  // dist_cm stays 0
                m_hasImu = false;
                m_hasEnv = false;
                emit recordReady(rec);
            }
            m_pendingImu = imu;
            m_hasImu = true;
            m_hasEnv = false;
        } else if (m_hasImu && !m_hasEnv && DataParser::parseEnv(line, env)) {
            m_pendingEnv = env;
            m_hasEnv = true;
        } else if (m_hasImu && m_hasEnv && DataParser::parseDist(line, dist_cm)) {
            SensorRecord rec;
            rec.ts          = QDateTime::currentDateTime();
            rec.imu         = m_pendingImu;
            rec.env         = m_pendingEnv;
            rec.env.dist_cm = dist_cm;
            m_hasImu = false;
            m_hasEnv = false;
            emit recordReady(rec);
        }
    }
}

void SerialWorker::sendRawBytes(const QByteArray &data)
{
    if (m_serial && m_serial->isOpen())
        m_serial->write(data);
}

void SerialWorker::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;
    emit errorOccurred(m_serial->errorString());
    emit connectionChanged(false);
}
