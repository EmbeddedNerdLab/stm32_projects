#pragma once
#include <QString>
#include <QRegularExpression>
#include "sensordata.h"

class DataParser {
public:
    static bool parseImu (const QString &line, ImuData &out);
    static bool parseEnv (const QString &line, EnvData &out);
    static bool parseDist(const QString &line, int &dist_cm);

private:
    static const QRegularExpression s_imuRe;
    static const QRegularExpression s_envRe;
    static const QRegularExpression s_distRe;
};
