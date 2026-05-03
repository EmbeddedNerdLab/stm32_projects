#pragma once
#include <QDateTime>
#include <QMetaType>

struct ImuData {
    int ax = 0, ay = 0, az = 0;
    int gx = 0, gy = 0, gz = 0;
};

struct EnvData {
    double temp = 0.0, hum = 0.0;
    int pressure = 0, lux = 0, dist_cm = 0;
};

struct SensorRecord {
    QDateTime ts;
    ImuData   imu;
    EnvData   env;
};

Q_DECLARE_METATYPE(SensorRecord)
