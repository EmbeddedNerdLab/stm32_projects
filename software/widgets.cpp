#include "widgets.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>
#include <algorithm>
#include <cmath>

// ── ThermometerWidget ─────────────────────────────────────────────────────────

ThermometerWidget::ThermometerWidget(QWidget *parent) : QWidget(parent)
{
    setFixedSize(sizeHint());
}

void ThermometerWidget::setTemperature(double celsius)
{
    if (m_temp == celsius) return;
    m_temp = celsius;
    update();
}

void ThermometerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double W  = width();
    const double H  = height();

    const double bulbR   = W * 0.30;
    const double stemW   = W * 0.22;
    const double stemX   = (W - stemW) / 2.0;
    const double bulbCx  = W / 2.0;
    const double bulbCy  = H - bulbR - 2.0;
    const double stemTop = 6.0;
    const double stemBot = bulbCy - bulbR + 4.0;
    const double stemH   = stemBot - stemTop;

    // Temperature → fill fraction (clamp -10..50 °C range)
    const double minT = -10.0, maxT = 50.0;
    const double frac = std::clamp((m_temp - minT) / (maxT - minT), 0.0, 1.0);

    // Colour: cold=steel-blue, cool=cyan, warm=orange, hot=red
    QColor fillColor;
    if (m_temp < 10.0)
        fillColor = QColor::fromHsvF(0.60, 0.85, 0.95);   // blue
    else if (m_temp < 25.0)
        fillColor = QColor::fromHsvF(0.33, 0.80, 0.90);   // green
    else if (m_temp < 35.0)
        fillColor = QColor::fromHsvF(0.10, 0.90, 1.00);   // orange
    else
        fillColor = QColor::fromHsvF(0.02, 1.00, 1.00);   // red

    const QColor glassEdge("#2a3a5e");
    const QColor glassFill("#0d1830");

    // ── outer glass tube ──────────────────────────────────────────────────────
    QPainterPath tube;
    tube.addRoundedRect(stemX, stemTop, stemW, stemH, stemW / 2.0, stemW / 2.0);
    p.fillPath(tube, glassFill);
    p.setPen(QPen(glassEdge, 1.2));
    p.drawPath(tube);

    // ── mercury fill inside tube ──────────────────────────────────────────────
    if (frac > 0.0) {
        const double fillH = stemH * frac;
        const double fillY = stemBot - fillH;
        const double inner = stemW - 4.0;
        const double innerX = stemX + 2.0;

        QPainterPath fill;
        fill.addRoundedRect(innerX, fillY, inner, fillH, inner / 2.0, inner / 2.0);
        // Gradient from darker base to brighter tip
        QLinearGradient grad(0, stemBot, 0, fillY);
        grad.setColorAt(0.0, fillColor.darker(130));
        grad.setColorAt(1.0, fillColor.lighter(130));
        p.fillPath(fill, grad);
    }

    // ── tick marks (5 lines along right side of stem) ────────────────────────
    p.setPen(QPen(QColor("#3a4e72"), 1.0));
    for (int i = 1; i <= 4; ++i) {
        double y = stemTop + stemH * i / 5.0;
        double x0 = stemX + stemW;
        p.drawLine(QPointF(x0, y), QPointF(x0 + 3.0, y));
    }

    // ── glass bulb (outer) ────────────────────────────────────────────────────
    QPainterPath bulbOuter;
    bulbOuter.addEllipse(QPointF(bulbCx, bulbCy), bulbR, bulbR);
    p.fillPath(bulbOuter, glassFill);
    p.setPen(QPen(glassEdge, 1.2));
    p.drawPath(bulbOuter);

    // ── mercury in bulb ───────────────────────────────────────────────────────
    {
        double innerR = bulbR - 2.5;
        QPainterPath bulbFill;
        bulbFill.addEllipse(QPointF(bulbCx, bulbCy), innerR, innerR);
        QRadialGradient rg(bulbCx - innerR * 0.2, bulbCy - innerR * 0.2, innerR * 1.2);
        rg.setColorAt(0.0, fillColor.lighter(150));
        rg.setColorAt(1.0, fillColor.darker(110));
        p.fillPath(bulbFill, rg);
    }

    // ── glass sheen (highlight streak on left of tube) ────────────────────────
    p.setPen(QPen(QColor(255, 255, 255, 35), 1.2));
    p.drawLine(QPointF(stemX + 2.5, stemTop + 4),
               QPointF(stemX + 2.5, stemBot - 4));
}

// ── LampWidget ────────────────────────────────────────────────────────────────

LampWidget::LampWidget(QWidget *parent) : QWidget(parent)
{
    setFixedSize(sizeHint());
}

void LampWidget::setLux(double lux)
{
    if (m_lux == lux) return;
    m_lux = lux;
    update();
}

void LampWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double W  = width();
    const double H  = height();
    const double cx = W / 2.0;

    // Brightness: 0..1, logarithmic feel (so even low lux shows some glow)
    const double t = std::clamp(m_lux / 800.0, 0.0, 1.0);
    const double tLog = t > 0.0 ? (std::log1p(t * 9.0) / std::log(10.0)) : 0.0;

    const double R       = W * 0.36;        // bulb radius
    const double bulbCy  = H * 0.38;        // bulb centre y
    const double sockTop = bulbCy + R * 0.62;
    const double sockBot = bulbCy + R * 1.60;
    const double sockW   = R * 0.90;

    // ── Outer glow (draw first, behind everything) ────────────────────────────
    if (tLog > 0.02) {
        const double glowR = R * (1.3 + tLog * 1.1);
        QRadialGradient glow(cx, bulbCy, glowR);
        glow.setColorAt(0.0, QColor(255, 220,  80, int(tLog * 110)));
        glow.setColorAt(0.4, QColor(255, 160,  20, int(tLog *  55)));
        glow.setColorAt(1.0, QColor(255, 120,   0,   0));
        QPainterPath glowPath;
        glowPath.addEllipse(cx - glowR, bulbCy - glowR, glowR * 2, glowR * 2);
        p.fillPath(glowPath, glow);
    }

    // ── Bulb body (glass) ─────────────────────────────────────────────────────
    // Warm dark when off, warm bright when on
    QColor bulbCore  = QColor::fromHsvF(0.13,
                                        0.15 + tLog * 0.70,
                                        0.12 + tLog * 0.88);
    QColor bulbEdge  = QColor::fromHsvF(0.11,
                                        0.05 + tLog * 0.40,
                                        0.08 + tLog * 0.40);

    QRadialGradient bulbGrad(cx - R * 0.2, bulbCy - R * 0.2, R * 1.2);
    bulbGrad.setColorAt(0.0, bulbCore);
    bulbGrad.setColorAt(1.0, bulbEdge);

    QPainterPath bulbPath;
    bulbPath.addEllipse(cx - R, bulbCy - R, R * 2, R * 2);
    p.fillPath(bulbPath, bulbGrad);

    // Bulb glass outline
    QColor outlineCol = tLog > 0.1
        ? QColor(255, 200, 80, 140)
        : QColor(60, 80, 110, 180);
    p.setPen(QPen(outlineCol, 1.1));
    p.drawPath(bulbPath);

    // ── Filament (W-shape inside bulb) ────────────────────────────────────────
    {
        const double fR = R * 0.30;
        const double fy = bulbCy + R * 0.05;
        QPainterPath fil;
        fil.moveTo(cx - fR,       fy + fR * 0.35);
        fil.lineTo(cx - fR * 0.5, fy - fR * 0.6);
        fil.lineTo(cx,            fy + fR * 0.35);
        fil.lineTo(cx + fR * 0.5, fy - fR * 0.6);
        fil.lineTo(cx + fR,       fy + fR * 0.35);

        QColor filCol = tLog > 0.05
            ? QColor(255, int(160 + tLog * 95), int(tLog * 80), int(160 + tLog * 95))
            : QColor(70, 55, 40, 180);
        p.setPen(QPen(filCol, 1.6));
        p.drawPath(fil);

        // Filament support wires (two thin vertical lines)
        p.setPen(QPen(filCol.darker(120), 0.8));
        p.drawLine(QPointF(cx - fR,       fy + fR * 0.35),
                   QPointF(cx - fR,       fy + fR * 1.1));
        p.drawLine(QPointF(cx + fR,       fy + fR * 0.35),
                   QPointF(cx + fR,       fy + fR * 1.1));
    }

    // ── Glass sheen (highlight on upper-left of bulb) ─────────────────────────
    {
        QRadialGradient sheen(cx - R * 0.38, bulbCy - R * 0.42, R * 0.50);
        sheen.setColorAt(0.0, QColor(255, 255, 255, int(35 + tLog * 45)));
        sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
        QPainterPath sheenPath;
        sheenPath.addEllipse(cx - R * 0.82, bulbCy - R * 0.85,
                             R * 0.78, R * 0.66);
        p.fillPath(sheenPath, sheen);
    }

    // ── Socket / screw base ───────────────────────────────────────────────────
    {
        const QColor sockDark("#2a2e38");
        const QColor sockMid("#3e4455");
        const QColor sockLine("#1e2230");

        // Neck transition (tapers from bulb to socket)
        QPainterPath neck;
        neck.moveTo(cx - R * 0.42, sockTop);
        neck.lineTo(cx - sockW / 2, sockTop + (sockBot - sockTop) * 0.15);
        neck.lineTo(cx + sockW / 2, sockTop + (sockBot - sockTop) * 0.15);
        neck.lineTo(cx + R * 0.42,  sockTop);
        neck.closeSubpath();
        p.fillPath(neck, sockMid);

        // Main socket body
        QRectF sockRect(cx - sockW / 2,
                        sockTop + (sockBot - sockTop) * 0.12,
                        sockW,
                        (sockBot - sockTop) * 0.88);
        QLinearGradient sockGrad(sockRect.left(), 0, sockRect.right(), 0);
        sockGrad.setColorAt(0.0, sockDark);
        sockGrad.setColorAt(0.5, sockMid);
        sockGrad.setColorAt(1.0, sockDark);
        p.fillRect(sockRect, sockGrad);
        p.setPen(QPen(sockLine, 0.7));
        p.drawRect(sockRect);

        // Thread lines
        p.setPen(QPen(sockLine, 0.8));
        for (int i = 1; i <= 3; ++i) {
            double y = sockRect.top() + sockRect.height() * i / 4.0;
            p.drawLine(QPointF(sockRect.left(),  y),
                       QPointF(sockRect.right(), y));
        }

        // Bottom contact cap
        QRectF cap(cx - sockW / 2, sockBot - 3.5, sockW, 5.0);
        p.fillRect(cap, sockMid.lighter(110));
        p.setPen(QPen(sockLine, 0.7));
        p.drawRect(cap);
    }
}

// ── HumidityWidget ────────────────────────────────────────────────────────────

/* Teardrop path centred at origin, pointing upward.
 * w = total width, h = total height. */
QPainterPath HumidityWidget::dropPath(double w, double h)
{
    QPainterPath path;
    const double hw = w / 2.0;
    const double hh = h / 2.0;
    // tip at top-centre
    path.moveTo(0, -hh);
    // curve to bottom-right
    path.cubicTo( hw,  0,
                  hw,  hh * 0.5,
                  0,   hh);
    // curve from bottom-left back to tip
    path.cubicTo(-hw, hh * 0.5,
                 -hw, 0,
                 0,  -hh);
    return path;
}

HumidityWidget::HumidityWidget(QWidget *parent) : QWidget(parent)
{
    setFixedSize(sizeHint());
}

void HumidityWidget::setHumidity(double pct)
{
    if (m_hum == pct) return;
    m_hum = pct;
    update();
}

void HumidityWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int    N         = 5;
    const double W         = width();
    const double H         = height();
    const double dropW     = W * 0.13;
    const double dropH     = H * 0.55;
    const double spacing   = W / (N + 1.0);
    const double centerY   = H * 0.55;

    // Each drop i fills in progressively: drop i is full when hum > (i+1)*20 %
    // and partially filled when hum is in [i*20, (i+1)*20].
    for (int i = 0; i < N; ++i) {
        const double x = spacing * (i + 1);
        const double lo = i * (100.0 / N);
        const double hi = (i + 1) * (100.0 / N);
        const double t  = std::clamp((m_hum - lo) / (hi - lo), 0.0, 1.0);

        // Alpha: ghosted outline at t=0, fully lit at t=1
        const int alpha = static_cast<int>(30 + t * 220);

        QColor waterFill(0x29, 0xb6, 0xf6, alpha);
        QColor waterDark(0x01, 0x5e, 0x8c, alpha);
        QColor outline(0x62, 0xd1, 0xff, std::min(255, alpha + 60));

        QPainterPath drop = dropPath(dropW, dropH);
        QTransform tf;
        tf.translate(x, centerY);
        drop = tf.map(drop);

        // Fill with vertical gradient: darker blue at bottom, lighter at top
        QLinearGradient grad(x, centerY - dropH / 2, x, centerY + dropH / 2);
        grad.setColorAt(0.0, waterFill);
        grad.setColorAt(1.0, waterDark);
        p.fillPath(drop, grad);

        p.setPen(QPen(outline, 0.8));
        p.drawPath(drop);

        // Small white sheen inside each drop
        if (t > 0.15) {
            p.setPen(QPen(QColor(255, 255, 255, static_cast<int>(t * 60)), 0.8));
            p.drawEllipse(QPointF(x - dropW * 0.15, centerY - dropH * 0.25),
                          dropW * 0.08, dropH * 0.09);
        }
    }

    // Rain-streak lines above drops when humidity is high (> 60%)
    if (m_hum > 60.0) {
        const double rainAlpha = (m_hum - 60.0) / 40.0;   // 0..1 over 60-100%
        p.setPen(QPen(QColor(100, 200, 255, static_cast<int>(rainAlpha * 90)), 1.0));
        for (int i = 0; i < N; ++i) {
            double x = spacing * (i + 1);
            double y0 = centerY - dropH / 2.0 - 8.0;
            p.drawLine(QPointF(x, y0), QPointF(x - 2, y0 - 7));
        }
    }
}

// ── OrientationWidget ─────────────────────────────────────────────────────────

OrientationWidget::OrientationWidget(QWidget *parent) : QWidget(parent)
{
    setFixedSize(sizeHint());
}

void OrientationWidget::setAngles(double rollDeg, double pitchDeg, double yawDeg)
{
    if (m_roll == rollDeg && m_pitch == pitchDeg && m_yaw == yawDeg) return;
    m_roll  = rollDeg;
    m_pitch = pitchDeg;
    m_yaw   = yawDeg;
    update();
}

void OrientationWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double W     = width();
    const double H     = height();
    const double cx    = W * 0.50;
    const double cy    = H * 0.54;
    const double scale = std::min(W, H) * 0.34;

    // ── Build ZYX rotation matrix from roll / pitch / yaw ─────────────────────
    const double toRad = M_PI / 180.0;
    const double cr = std::cos(m_roll  * toRad), sr = std::sin(m_roll  * toRad);
    const double cp = std::cos(m_pitch * toRad), sp = std::sin(m_pitch * toRad);
    const double cy_ = std::cos(m_yaw  * toRad), sy = std::sin(m_yaw   * toRad);

    // R = Rz(yaw) * Ry(pitch) * Rx(roll)
    const double R[3][3] = {
        { cy_*cp,  cy_*sp*sr - sy*cr,  cy_*sp*cr + sy*sr },
        { sy *cp,  sy *sp*sr + cy_*cr, sy *sp*cr - cy_*sr },
        { -sp,     cp*sr,              cp*cr              }
    };

    // Rotate a unit axis vector and project isometrically to screen:
    // iso: camera from front-right-above (azimuth 30°, elevation 35°)
    // screen_x = cx + scale * (x - y) * cos30
    // screen_y = cy + scale * ((x + y) * sin30 - z)
    auto project = [&](double ax, double ay, double az) -> QPointF {
        double rx = R[0][0]*ax + R[0][1]*ay + R[0][2]*az;
        double ry = R[1][0]*ax + R[1][1]*ay + R[1][2]*az;
        double rz = R[2][0]*ax + R[2][1]*ay + R[2][2]*az;
        return { cx + scale * (rx - ry) * 0.866,
                 cy + scale * ((rx + ry) * 0.5 - rz) };
    };

    const QPointF origin = project(0, 0, 0);

    // ── Draw faint reference grid on the "floor" (XY plane, Z=0) ─────────────
    p.setPen(QPen(QColor("#1e2d4a"), 0.8));
    for (int i = -1; i <= 1; ++i) {
        p.drawLine(project(-1, i, 0), project(1, i, 0));
        p.drawLine(project(i, -1, 0), project(i, 1, 0));
    }

    // ── Draw each axis ────────────────────────────────────────────────────────
    struct Axis { double x, y, z; QColor col; QString label; };
    const Axis axes[] = {
        { 1, 0, 0, QColor("#f03030"), "X" },
        { 0, 1, 0, QColor("#37c64e"), "Y" },
        { 0, 0, 1, QColor("#29b8f0"), "Z" },
    };

    for (const auto &ax : axes) {
        QPointF tip = project(ax.x, ax.y, ax.z);

        // Axis line
        p.setPen(QPen(ax.col, 2.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(origin, tip);

        // Arrowhead — small filled triangle at tip
        QPointF dir  = tip - origin;
        double  len  = std::hypot(dir.x(), dir.y());
        if (len > 1.0) {
            QPointF unit = dir / len;
            QPointF perp(-unit.y() * 3.5, unit.x() * 3.5);
            QPointF base = tip - unit * 7.0;
            QPolygonF arrow;
            arrow << tip << (base + perp) << (base - perp);
            p.setBrush(ax.col);
            p.setPen(Qt::NoPen);
            p.drawPolygon(arrow);
        }

        // Label just beyond the tip
        QPointF lblPos = tip + (tip - origin) * 0.18;
        p.setPen(ax.col);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.drawText(QRectF(lblPos.x() - 8, lblPos.y() - 8, 16, 16),
                   Qt::AlignCenter, ax.label);
    }

    // Origin dot
    p.setBrush(QColor("#c0c8e0"));
    p.setPen(QPen(QColor("#60708a"), 0.8));
    p.drawEllipse(origin, 3.0, 3.0);
}

// ── PressureWidget ────────────────────────────────────────────────────────────

PressureWidget::PressureWidget(QWidget *parent) : QWidget(parent)
{
    setFixedSize(sizeHint());
}

void PressureWidget::setPressure(double hpa)
{
    if (m_hpa == hpa) return;
    m_hpa = hpa;
    update();
}

void PressureWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double W  = width();
    const double H  = height();
    const double cx = W / 2.0;
    const double cy = H * 0.86;          // centre near bottom so arc fans upward
    const double R  = std::min(W * 0.44, H * 0.92);
    const double arcW = R * 0.22;        // track width

    // Gauge sweep: 225° start, –270° span (CW), 12 o'clock at centre
    // Qt angles: CCW from 3-o'clock, in 1/16ths of a degree
    const int startDeg = 225;            // 7:30 o'clock
    const int sweepDeg = 270;            // full CW arc to 4:30 o'clock

    // Pressure range
    const double minP = 960.0, maxP = 1060.0;
    const double frac = std::clamp((m_hpa - minP) / (maxP - minP), 0.0, 1.0);

    QRectF arcRect(cx - R, cy - R, R * 2, R * 2);

    // ── Track (background arc) ─────────────────────────────────────────────────
    // Draw three coloured zones: blue (low), green (normal), red (high)
    // Low:    960–1000 → 0.00–0.40 of sweep
    // Normal: 1000–1025 → 0.40–0.65 of sweep
    // High:   1025–1060 → 0.65–1.00 of sweep
    struct Zone { double f0, f1; QColor col; };
    const Zone zones[] = {
        { 0.00, 0.40, QColor(0x29, 0x9d, 0xd4, 80) },   // blue
        { 0.40, 0.65, QColor(0x37, 0xb2, 0x4d, 80) },   // green
        { 0.65, 1.00, QColor(0xe0, 0x30, 0x30, 80) },   // red
    };
    for (const auto &z : zones) {
        int a0 = startDeg - static_cast<int>(z.f0 * sweepDeg);
        int sp = -static_cast<int>((z.f1 - z.f0) * sweepDeg);
        p.setPen(QPen(z.col, arcW, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arcRect, a0 * 16, sp * 16);
    }

    // ── Bright value arc (from start to needle) ────────────────────────────────
    QColor valCol;
    if (m_hpa < 1000.0)      valCol = QColor("#29b8f0");
    else if (m_hpa < 1025.0) valCol = QColor("#37c64e");
    else                     valCol = QColor("#f03030");

    int valSweep = -static_cast<int>(frac * sweepDeg);
    p.setPen(QPen(valCol, arcW * 0.55, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(arcRect, startDeg * 16, valSweep * 16);

    // ── Tick marks ────────────────────────────────────────────────────────────
    p.setPen(QPen(QColor("#3a4e72"), 1.0));
    for (int i = 0; i <= 6; ++i) {
        double ang = (startDeg - i * sweepDeg / 6.0) * M_PI / 180.0;
        double r0 = R - arcW * 0.9, r1 = R + arcW * 0.1;
        p.drawLine(QPointF(cx + r0 * std::cos(ang), cy - r0 * std::sin(ang)),
                   QPointF(cx + r1 * std::cos(ang), cy - r1 * std::sin(ang)));
    }

    // ── Needle ────────────────────────────────────────────────────────────────
    const double needleAng = (startDeg - frac * sweepDeg) * M_PI / 180.0;
    const double nx = cx + (R - arcW) * std::cos(needleAng);
    const double ny = cy - (R - arcW) * std::sin(needleAng);

    // Shadow
    p.setPen(QPen(QColor(0, 0, 0, 60), 2.5, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(cx + 1, cy + 1), QPointF(nx + 1, ny + 1));
    // Needle body
    p.setPen(QPen(QColor("#e8e8f0"), 1.8, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(cx, cy), QPointF(nx, ny));
    // Centre pivot dot
    p.setBrush(QColor("#c0c8e0"));
    p.setPen(QPen(QColor("#60708a"), 0.8));
    p.drawEllipse(QPointF(cx, cy), 4.0, 4.0);
}

// ── DistanceWidget ────────────────────────────────────────────────────────────

DistanceWidget::DistanceWidget(QWidget *parent) : QWidget(parent)
{
    setFixedSize(sizeHint());
}

void DistanceWidget::setDistance(double cm)
{
    if (m_cm == cm) return;
    m_cm = cm;
    update();
}

void DistanceWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double W       = width();
    const double H       = height();
    const double maxDist = 300.0;

    // Layout: sensor icon | beam track | end wall
    const double sensorW = W * 0.09;
    const double sensorH = H * 0.38;
    const double trackX0 = sensorW + 3.0;          // beam starts here
    const double trackX1 = W - 4.0;                // beam ends here
    const double trackLen = trackX1 - trackX0;
    const double trackCy = H * 0.42;               // vertical centre of beam
    const double trackH  = H * 0.22;               // beam height

    // Clamp measured distance to range
    const double dist  = std::clamp(m_cm, 0.0, maxDist);
    const double frac  = dist / maxDist;
    const double objX  = trackX0 + frac * trackLen; // object cursor x

    // ── Sensor body (left) ────────────────────────────────────────────────────
    QRectF sensor(2, trackCy - sensorH / 2.0, sensorW, sensorH);
    QLinearGradient sGrad(sensor.left(), 0, sensor.right(), 0);
    sGrad.setColorAt(0, QColor("#2a3a5e"));
    sGrad.setColorAt(1, QColor("#3e5080"));
    p.fillRect(sensor, sGrad);
    p.setPen(QPen(QColor("#5a7aaa"), 0.8));
    p.drawRect(sensor);

    // Sensor "eye" dots
    for (int i = 0; i < 2; ++i) {
        double ey = trackCy - sensorH * 0.18 + i * sensorH * 0.36;
        p.setBrush(QColor("#29b8f0"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(sensor.center().x(), ey), 1.8, 1.8);
    }

    // ── Beam track background ─────────────────────────���───────────────────────
    QRectF track(trackX0, trackCy - trackH / 2.0, trackLen, trackH);
    p.fillRect(track, QColor("#0d1830"));
    p.setPen(QPen(QColor("#1e2d4a"), 0.7));
    p.drawRect(track);

    // ── Filled beam from sensor to object (colour by zone) ───────────────────��
    if (dist > 0.0) {
        QColor beamCol;
        if (dist < 60.0)       beamCol = QColor("#f03030");
        else if (dist < 150.0) beamCol = QColor("#ff8c00");
        else                   beamCol = QColor("#37c64e");

        QLinearGradient bGrad(trackX0, 0, objX, 0);
        bGrad.setColorAt(0.0, beamCol.darker(160));
        bGrad.setColorAt(0.6, beamCol);
        bGrad.setColorAt(1.0, beamCol.lighter(130));

        QRectF beam(trackX0, trackCy - trackH / 2.0 + 1,
                    frac * trackLen, trackH - 2);
        p.fillRect(beam, bGrad);
    }

    // ── Object cursor (bright vertical line) ─────────────────────────────────
    if (dist > 0.0) {
        QColor cursorCol = dist < 60.0 ? QColor("#ff5555")
                         : dist < 150.0 ? QColor("#ffaa33")
                                        : QColor("#51cf66");
        // Glow
        p.setPen(QPen(cursorCol.darker(120), 4.0, Qt::SolidLine, Qt::FlatCap));
        p.setOpacity(0.3);
        p.drawLine(QPointF(objX, trackCy - trackH),
                   QPointF(objX, trackCy + trackH));
        p.setOpacity(1.0);
        // Sharp line
        p.setPen(QPen(cursorCol, 1.5, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(QPointF(objX, trackCy - trackH * 0.85),
                   QPointF(objX, trackCy + trackH * 0.85));

        // Small diamond at cursor centre
        QPolygonF diamond;
        double ds = 3.5;
        diamond << QPointF(objX, trackCy - ds) << QPointF(objX + ds, trackCy)
                << QPointF(objX, trackCy + ds) << QPointF(objX - ds, trackCy);
        p.setBrush(cursorCol);
        p.setPen(Qt::NoPen);
        p.drawPolygon(diamond);
    }

    // ── Scale ticks at 0, 100, 200, 300 cm ─────────────────────────��─────────
    p.setFont(QFont("Arial", 6));
    const double tickY0 = trackCy + trackH / 2.0 + 1.0;
    const double tickY1 = tickY0 + 4.0;
    const double lblY   = tickY1 + 1.0;

    const struct { int cm; const char *lbl; } ticks[] = {
        {   0, "0" }, { 100, "1m" }, { 200, "2m" }, { 300, "3m" }
    };
    for (const auto &t : ticks) {
        double tx = trackX0 + (t.cm / maxDist) * trackLen;
        p.setPen(QPen(QColor("#4a5e80"), 0.8));
        p.drawLine(QPointF(tx, tickY0), QPointF(tx, tickY1));
        p.setPen(QColor("#6a7e9a"));
        p.drawText(QRectF(tx - 8, lblY, 16, 10), Qt::AlignHCenter, t.lbl);
    }
}
