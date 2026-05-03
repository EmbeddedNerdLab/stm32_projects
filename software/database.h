#pragma once
#include <QList>
#include <QString>
#include "sensordata.h"

class Database {
public:
    Database();
    ~Database();

    bool open(const QString &path);
    void insert(const SensorRecord &rec);
    QList<SensorRecord> fetchLast(int n);

private:
    QString m_connName;
};
