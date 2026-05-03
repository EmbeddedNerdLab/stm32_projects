#include "dataparser.h"

const QRegularExpression DataParser::s_imuRe(
    R"(IMU\s+AX=\s*(-?\d+)\s+AY=\s*(-?\d+)\s+AZ=\s*(-?\d+)\s+GX=\s*(-?\d+)\s+GY=\s*(-?\d+)\s+GZ=\s*(-?\d+))");

const QRegularExpression DataParser::s_envRe(
    R"(ENV\s+T=([\d.]+)\s+C\s+H=([\d.]+)%\s+P=(\d+)\s+hPa\s+LUX=(\d+))");

const QRegularExpression DataParser::s_distRe(R"(DIST\s+CM=(\d+))");

bool DataParser::parseImu(const QString &line, ImuData &out)
{
    auto m = s_imuRe.match(line);
    if (!m.hasMatch()) return false;
    out.ax = m.captured(1).toInt();
    out.ay = m.captured(2).toInt();
    out.az = m.captured(3).toInt();
    out.gx = m.captured(4).toInt();
    out.gy = m.captured(5).toInt();
    out.gz = m.captured(6).toInt();
    return true;
}

bool DataParser::parseEnv(const QString &line, EnvData &out)
{
    auto m = s_envRe.match(line);
    if (!m.hasMatch()) return false;
    out.temp     = m.captured(1).toDouble();
    out.hum      = m.captured(2).toDouble();
    out.pressure = m.captured(3).toInt();
    out.lux      = m.captured(4).toInt();
    return true;
}

bool DataParser::parseDist(const QString &line, int &dist_cm)
{
    auto m = s_distRe.match(line);
    if (!m.hasMatch()) return false;
    dist_cm = m.captured(1).toInt();
    return true;
}
