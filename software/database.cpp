#include "database.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QUuid>

Database::Database()
    : m_connName(QUuid::createUuid().toString())
{}

Database::~Database()
{
    QSqlDatabase::removeDatabase(m_connName);
}

bool Database::open(const QString &path)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
    db.setDatabaseName(path);
    if (!db.open()) {
        qWarning() << "DB open failed:" << db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS readings (
            id       INTEGER PRIMARY KEY AUTOINCREMENT,
            ts       TEXT    NOT NULL,
            ax       INTEGER, ay INTEGER, az INTEGER,
            gx       INTEGER, gy INTEGER, gz INTEGER,
            temp     REAL,    hum  REAL,
            pressure INTEGER, lux  INTEGER
        )
    )");
    return true;
}

void Database::insert(const SensorRecord &rec)
{
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO readings (ts, ax, ay, az, gx, gy, gz, temp, hum, pressure, lux)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    q.addBindValue(rec.ts.toString(Qt::ISODate));
    q.addBindValue(rec.imu.ax); q.addBindValue(rec.imu.ay); q.addBindValue(rec.imu.az);
    q.addBindValue(rec.imu.gx); q.addBindValue(rec.imu.gy); q.addBindValue(rec.imu.gz);
    q.addBindValue(rec.env.temp);
    q.addBindValue(rec.env.hum);
    q.addBindValue(rec.env.pressure);
    q.addBindValue(rec.env.lux);
    if (!q.exec())
        qWarning() << "DB insert failed:" << q.lastError().text();
}

QList<SensorRecord> Database::fetchLast(int n)
{
    QList<SensorRecord> result;
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.prepare("SELECT ts,ax,ay,az,gx,gy,gz,temp,hum,pressure,lux "
              "FROM readings ORDER BY id DESC LIMIT ?");
    q.addBindValue(n);
    if (!q.exec()) return result;
    while (q.next()) {
        SensorRecord rec;
        rec.ts             = QDateTime::fromString(q.value(0).toString(), Qt::ISODate);
        rec.imu.ax         = q.value(1).toInt();
        rec.imu.ay         = q.value(2).toInt();
        rec.imu.az         = q.value(3).toInt();
        rec.imu.gx         = q.value(4).toInt();
        rec.imu.gy         = q.value(5).toInt();
        rec.imu.gz         = q.value(6).toInt();
        rec.env.temp       = q.value(7).toDouble();
        rec.env.hum        = q.value(8).toDouble();
        rec.env.pressure   = q.value(9).toInt();
        rec.env.lux        = q.value(10).toInt();
        result.append(rec);
    }
    return result;
}
