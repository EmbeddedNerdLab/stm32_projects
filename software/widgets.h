#pragma once
#include <QWidget>

/* ── ThermometerWidget ───────────────────────────────────────────────────────
 * Paints a glass thermometer with a coloured fill level.
 * Range: -10 °C (empty) to 50 °C (full).
 * Colour: blue → green → orange → red as temperature rises.
 */
class ThermometerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ThermometerWidget(QWidget *parent = nullptr);
    void setTemperature(double celsius);
    QSize sizeHint() const override { return {34, 80}; }
    QSize minimumSizeHint() const override { return {28, 60}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_temp = 20.0;
};

/* ── LampWidget ──────────────────────────────────────────────────────────────
 * Paints a classic incandescent bulb that glows brighter with lux.
 * Range: 0 lx (dark/off) to 1000 lx (fully lit).
 */
class LampWidget : public QWidget {
    Q_OBJECT
public:
    explicit LampWidget(QWidget *parent = nullptr);
    void setLux(double lux);
    QSize sizeHint() const override { return {52, 72}; }
    QSize minimumSizeHint() const override { return {40, 56}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_lux = 0.0;
};

/* ── PressureWidget ──────────────────────────────────────────────────────────
 * Analogue barometer dial: 270° arc with a needle.
 * Range: 960 – 1060 hPa.  Zones: blue (low) / green (normal) / red (high).
 */
class PressureWidget : public QWidget {
    Q_OBJECT
public:
    explicit PressureWidget(QWidget *parent = nullptr);
    void setPressure(double hpa);
    QSize sizeHint() const override { return {110, 80}; }
    QSize minimumSizeHint() const override { return {88, 64}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_hpa = 1013.0;
};

/* ── DistanceWidget ──────────────────────────────────────────────────────────
 * Sonar proximity display: 5 concentric semicircular arcs.
 * Arcs fill inward as the target gets closer (parking-sensor style).
 * Range: 0 – 300 cm.
 */
class DistanceWidget : public QWidget {
    Q_OBJECT
public:
    explicit DistanceWidget(QWidget *parent = nullptr);
    void setDistance(double cm);
    QSize sizeHint() const override { return {110, 68}; }
    QSize minimumSizeHint() const override { return {88, 54}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_cm = 0.0;
};

/* ── OrientationWidget ───────────────────────────────────────────────────────
 * Draws three 3-D axes (X=red, Y=green, Z=blue) rotated by roll/pitch/yaw
 * and projected with a fixed isometric camera — gives an instant visual of
 * how the board is tilted in space.
 */
class OrientationWidget : public QWidget {
    Q_OBJECT
public:
    explicit OrientationWidget(QWidget *parent = nullptr);
    void setAngles(double rollDeg, double pitchDeg, double yawDeg);
    QSize sizeHint() const override { return {110, 90}; }
    QSize minimumSizeHint() const override { return {88, 72}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_roll  = 0.0;
    double m_pitch = 0.0;
    double m_yaw   = 0.0;
};

/* ── HumidityWidget ──────────────────────────────────────────────────────────
 * Paints 5 stylised rain drops; they fill in (bottom → top) as humidity rises.
 * 0 % → all drops are ghosted outlines; 100 % → all drops fully lit blue.
 */
class HumidityWidget : public QWidget {
    Q_OBJECT
public:
    explicit HumidityWidget(QWidget *parent = nullptr);
    void setHumidity(double pct);
    QSize sizeHint() const override { return {88, 48}; }
    QSize minimumSizeHint() const override { return {70, 38}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_hum = 50.0;

    static QPainterPath dropPath(double w, double h);
};
