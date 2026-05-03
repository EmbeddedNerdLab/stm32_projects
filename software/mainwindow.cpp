#include "mainwindow.h"
#include "serialworker.h"
#include "widgets.h"
#include "flashdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QFrame>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QtCharts/QChart>
#include <QApplication>
#include <QMessageBox>
#include <QToolTip>
#include <QGraphicsEllipseItem>
#include <cfloat>
#include <cmath>

const MainWindow::SigMeta MainWindow::k_sigMeta[SIG_COUNT] = {
    { "AX",         QColor("#00d4ff") },
    { "AY",         QColor("#ff6b6b") },
    { "AZ",         QColor("#51cf66") },
    { "GX",         QColor("#ffd43b") },
    { "GY",         QColor("#cc5de8") },
    { "GZ",         QColor("#ff922b") },
    { "Roll",       QColor("#a9e34b") },
    { "Pitch",      QColor("#f783ac") },
    { "Yaw",        QColor("#74c0fc") },
    { "Temp(°C)",   QColor("#ff6b6b") },
    { "Hum(%)",     QColor("#51cf66") },
    { "Press(hPa)", QColor("#74c0fc") },
    { "Lux",        QColor("#ffd43b") },
    { "Dist(cm)",   QColor("#ff6b6b") },
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("STM32 Monitor");
    resize(1280, 800);

    qRegisterMetaType<SensorRecord>();

    m_thread = new QThread(this);
    m_worker = new SerialWorker;
    m_worker->moveToThread(m_thread);

    connect(m_worker, &SerialWorker::recordReady,
            this,     &MainWindow::onRecord,            Qt::QueuedConnection);
    connect(m_worker, &SerialWorker::connectionChanged,
            this,     &MainWindow::onConnectionChanged, Qt::QueuedConnection);
    connect(m_worker, &SerialWorker::errorOccurred,
            this,     &MainWindow::onSerialError,       Qt::QueuedConnection);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();

    auto *central    = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->addWidget(buildTopBar());

    mainLayout->addWidget(buildLeftPanel(), 1);

    // Charts panel — commented out, re-enable when needed:
    // auto *splitter = new QSplitter(Qt::Horizontal);
    // splitter->addWidget(buildLeftPanel());
    // splitter->addWidget(buildRightPanel());
    // splitter->setStretchFactor(0, 0);
    // splitter->setStretchFactor(1, 1);
    // splitter->setSizes({220, 1060});
    // mainLayout->addWidget(splitter, 1);

    setCentralWidget(central);
    onRefreshPorts();
}

MainWindow::~MainWindow()
{
    stopLogging();
    QMetaObject::invokeMethod(m_worker, "close", Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait();
}

// ── Top bar ───────────────────────────────────────────────────────────────────

QWidget *MainWindow::buildTopBar()
{
    auto *bar = new QFrame;
    bar->setObjectName("topBar");
    bar->setFixedHeight(48);

    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(8, 4, 8, 4);

    auto *title = new QLabel("STM32 Monitor");
    title->setObjectName("title");
    lay->addWidget(title);
    lay->addStretch();

    lay->addWidget(new QLabel("Port:"));
    m_portCombo = new QComboBox;
    m_portCombo->setFixedWidth(120);
    lay->addWidget(m_portCombo);

    auto *refreshBtn = new QPushButton("⟳");
    refreshBtn->setFixedWidth(32);
    refreshBtn->setToolTip("Refresh port list");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    lay->addWidget(refreshBtn);

    m_connectBtn = new QPushButton("Connect");
    m_connectBtn->setObjectName("connectBtn");
    m_connectBtn->setFixedWidth(100);
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    lay->addWidget(m_connectBtn);

    lay->addSpacing(8);

    m_logBtn = new QPushButton("⏺  Start Log");
    m_logBtn->setObjectName("logBtn");
    m_logBtn->setFixedWidth(120);
    m_logBtn->setToolTip("Start / stop CSV logging to file");
    connect(m_logBtn, &QPushButton::clicked, this, &MainWindow::onLogClicked);
    lay->addWidget(m_logBtn);

    m_clearBtn = new QPushButton("✕ Clear");
    m_clearBtn->setFixedWidth(90);
    m_clearBtn->setToolTip("Clear all charts");
    connect(m_clearBtn, &QPushButton::clicked, this, &MainWindow::onClearCharts);
    lay->addWidget(m_clearBtn);

    m_bootBtn = new QPushButton("⟳  Bootloader");
    m_bootBtn->setObjectName("bootBtn");
    m_bootBtn->setFixedWidth(120);
    m_bootBtn->setToolTip("Send command to reboot device into bootloader mode (must be connected)");
    connect(m_bootBtn, &QPushButton::clicked, this, &MainWindow::onBootloaderClicked);
    lay->addWidget(m_bootBtn);

    m_flashBtn = new QPushButton("⬆  Flash");
    m_flashBtn->setObjectName("flashBtn");
    m_flashBtn->setFixedWidth(90);
    m_flashBtn->setToolTip("Upload firmware to device (device must already be in bootloader mode)");
    connect(m_flashBtn, &QPushButton::clicked, this, &MainWindow::onFlashClicked);
    lay->addWidget(m_flashBtn);

    lay->addSpacing(16);
    m_lblTime = new QLabel("0.0 s");
    m_lblTime->setObjectName("timeLabel");
    m_lblTime->setFixedWidth(70);
    m_lblTime->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(m_lblTime);

    lay->addSpacing(8);
    m_statusDot = new QLabel("●");
    m_statusDot->setObjectName("statusDotOff");
    lay->addWidget(m_statusDot);
    m_statusText = new QLabel("Disconnected");
    lay->addWidget(m_statusText);

    return bar;
}

// ── Left panel ────────────────────────────────────────────────────────────────

QWidget *MainWindow::buildLeftPanel()
{
    auto *panel = new QWidget;
    auto *grid  = new QGridLayout(panel);
    grid->setSpacing(10);
    grid->setContentsMargins(4, 4, 4, 4);

    // ── 3-column dashboard layout ──────────────────────────────────────────────
    //  Col 0           │  Col 1                   │  Col 2
    //  ACCELEROMETER   │  ENVIRONMENT (2 rows)    │  ORIENTATION
    //  GYROSCOPE       │                          │  DISTANCE

    grid->addWidget(makeCard("ACCELEROMETER",
        {"AX","AY","AZ"}, {&m_lblAx,&m_lblAy,&m_lblAz}, "#00d4ff"),
        0, 0);

    grid->addWidget(makeCard("GYROSCOPE",
        {"GX","GY","GZ"}, {&m_lblGx,&m_lblGy,&m_lblGz}, "#ffd43b"),
        1, 0);

    grid->addWidget(makeEnvCard(),   0, 1, 2, 1);  // spans both rows

    grid->addWidget(makeOrientCard(), 0, 2);
    grid->addWidget(makeDistCard(),   1, 2);

    // Equal column widths, rows share space evenly
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    return panel;
}

QWidget *MainWindow::makeCard(const QString &title,
                               const QStringList &names,
                               const QList<QLabel**> &labels,
                               const QString &color)
{
    auto *frame = new QFrame;
    frame->setObjectName("card");
    auto *lay = new QVBoxLayout(frame);
    lay->setSpacing(4);
    lay->setContentsMargins(10, 8, 10, 8);

    auto *titleLbl = new QLabel(title);
    titleLbl->setObjectName("cardTitle");
    lay->addWidget(titleLbl);

    for (int i = 0; i < names.size(); ++i) {
        auto *row    = new QWidget;
        auto *rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);

        auto *nameLbl = new QLabel(names[i]);
        nameLbl->setObjectName("cardName");
        nameLbl->setFixedWidth(50);

        auto *valLbl = new QLabel("—");
        valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valLbl->setStyleSheet(
            QString("color:%1; font-size:15px; font-weight:bold;").arg(color));
        *labels[i] = valLbl;

        rowLay->addWidget(nameLbl);
        rowLay->addWidget(valLbl, 1);
        lay->addWidget(row);
    }
    return frame;
}

// ── makeEnvCard ───────────────────────────────────────────────────────────────

QWidget *MainWindow::makeEnvCard()
{
    auto *frame = new QFrame;
    frame->setObjectName("card");
    auto *lay = new QVBoxLayout(frame);
    lay->setSpacing(4);
    lay->setContentsMargins(10, 8, 10, 8);

    auto *titleLbl = new QLabel("ENVIRONMENT");
    titleLbl->setObjectName("cardTitle");
    lay->addWidget(titleLbl);

    // ── Temperature row: thermometer icon + value label ───────────────────────
    {
        auto *row = new QWidget;
        auto *rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(6);

        m_thermometer = new ThermometerWidget;
        rowLay->addWidget(m_thermometer, 0, Qt::AlignVCenter);

        auto *col = new QVBoxLayout;
        col->setSpacing(2);
        auto *nameLbl = new QLabel("TEMP");
        nameLbl->setObjectName("cardName");
        m_lblT = new QLabel("—");
        m_lblT->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_lblT->setStyleSheet("color:#ff6b6b; font-size:15px; font-weight:bold;");
        col->addWidget(nameLbl);
        col->addWidget(m_lblT);
        rowLay->addLayout(col, 1);

        lay->addWidget(row);
    }

    // ── Humidity row: rain drops + value label ────────────────────────────────
    {
        auto *row = new QWidget;
        auto *rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(4);

        auto *col = new QVBoxLayout;
        col->setSpacing(2);
        auto *nameLbl = new QLabel("HUMID");
        nameLbl->setObjectName("cardName");
        m_lblH = new QLabel("—");
        m_lblH->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_lblH->setStyleSheet("color:#51cf66; font-size:15px; font-weight:bold;");
        col->addWidget(nameLbl);
        col->addWidget(m_lblH);
        rowLay->addLayout(col, 0);

        m_dropWidget = new HumidityWidget;
        rowLay->addWidget(m_dropWidget, 1, Qt::AlignVCenter | Qt::AlignRight);

        lay->addWidget(row);
    }

    // ── Pressure row: barometer gauge + value label ───────────────────────────
    {
        auto *row = new QWidget;
        auto *rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(4);

        auto *col = new QVBoxLayout;
        col->setSpacing(2);
        auto *nameLbl = new QLabel("PRESS");
        nameLbl->setObjectName("cardName");
        m_lblP = new QLabel("—");
        m_lblP->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_lblP->setStyleSheet("color:#ff922b; font-size:15px; font-weight:bold;");
        col->addWidget(nameLbl);
        col->addWidget(m_lblP);
        rowLay->addLayout(col, 0);

        m_pressWidget = new PressureWidget;
        rowLay->addWidget(m_pressWidget, 1, Qt::AlignVCenter | Qt::AlignRight);

        lay->addWidget(row);
    }

    // ── Lux row: lamp bulb + value label ─────────────────────────────────────
    {
        auto *row = new QWidget;
        auto *rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(4);

        auto *col = new QVBoxLayout;
        col->setSpacing(2);
        auto *nameLbl = new QLabel("LUX");
        nameLbl->setObjectName("cardName");
        m_lblLux = new QLabel("—");
        m_lblLux->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_lblLux->setStyleSheet("color:#ffd43b; font-size:15px; font-weight:bold;");
        col->addWidget(nameLbl);
        col->addWidget(m_lblLux);
        rowLay->addLayout(col, 0);

        m_lampWidget = new LampWidget;
        rowLay->addWidget(m_lampWidget, 1, Qt::AlignVCenter | Qt::AlignRight);

        lay->addWidget(row);
    }

    return frame;
}

// ── makeOrientCard ────────────────────────────────────────────────────────────

QWidget *MainWindow::makeOrientCard()
{
    auto *frame = new QFrame;
    frame->setObjectName("card");
    auto *lay = new QVBoxLayout(frame);
    lay->setSpacing(4);
    lay->setContentsMargins(10, 8, 10, 8);

    auto *titleLbl = new QLabel("ORIENTATION");
    titleLbl->setObjectName("cardTitle");
    lay->addWidget(titleLbl);

    // 3D axes widget centred above the numeric values
    m_orientWidget = new OrientationWidget;
    lay->addWidget(m_orientWidget, 0, Qt::AlignHCenter);

    // Three value rows: Roll, Pitch, Yaw
    const struct { const char *name; QLabel **lbl; const char *col; } rows[] = {
        { "ROLL",  &m_lblRoll,  "#a9e34b" },
        { "PITCH", &m_lblPitch, "#f783ac" },
        { "YAW",   &m_lblYaw,   "#74c0fc" },
    };
    for (const auto &r : rows) {
        auto *row    = new QWidget;
        auto *rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        auto *nameLbl = new QLabel(r.name);
        nameLbl->setObjectName("cardName");
        nameLbl->setFixedWidth(50);
        *r.lbl = new QLabel("—");
        (*r.lbl)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        (*r.lbl)->setStyleSheet(
            QString("color:%1; font-size:15px; font-weight:bold;").arg(r.col));
        rowLay->addWidget(nameLbl);
        rowLay->addWidget(*r.lbl, 1);
        lay->addWidget(row);
    }
    return frame;
}

// ── makeDistCard ──────────────────────────────────────────────────────────────

QWidget *MainWindow::makeDistCard()
{
    auto *frame = new QFrame;
    frame->setObjectName("card");
    auto *lay = new QVBoxLayout(frame);
    lay->setSpacing(4);
    lay->setContentsMargins(10, 8, 10, 8);

    auto *titleLbl = new QLabel("DISTANCE");
    titleLbl->setObjectName("cardTitle");
    lay->addWidget(titleLbl);

    auto *row    = new QWidget;
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(4);

    auto *col = new QVBoxLayout;
    col->setSpacing(2);
    auto *nameLbl = new QLabel("DIST");
    nameLbl->setObjectName("cardName");
    m_lblDist = new QLabel("—");
    m_lblDist->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_lblDist->setStyleSheet("color:#ff6b6b; font-size:15px; font-weight:bold;");
    col->addWidget(nameLbl);
    col->addWidget(m_lblDist);
    rowLay->addLayout(col, 0);

    m_distWidget = new DistanceWidget;
    rowLay->addWidget(m_distWidget, 1, Qt::AlignVCenter | Qt::AlignRight);

    lay->addWidget(row);
    return frame;
}

// ── Right panel ───────────────────────────────────────────────────────────────

QWidget *MainWindow::buildRightPanel()
{
    auto *panel = new QWidget;
    auto *grid  = new QGridLayout(panel);
    grid->setSpacing(4);
    grid->setContentsMargins(0, 0, 0, 0);

    // 2 charts: Accel | Gyro side by side, filling the full panel
    const QList<QList<SigId>> defaults = {
        { SIG_AX, SIG_AY, SIG_AZ },
        { SIG_GX, SIG_GY, SIG_GZ },
    };

    grid->addWidget(makeChartPanel(m_charts[0], defaults[0]), 0, 0);
    grid->addWidget(makeChartPanel(m_charts[1], defaults[1]), 0, 1);
    grid->setRowStretch(0, 1);

    return panel;
}

// ── makeChartPanel ────────────────────────────────────────────────────────────

QWidget *MainWindow::makeChartPanel(ChartBundle &cb, const QList<SigId> &sigIds)
{
    cb.chart = new QChart;
    cb.chart->setBackgroundBrush(QColor("#16213e"));
    cb.chart->setTitleBrush(QColor("#e0e0e0"));
    cb.chart->legend()->setLabelColor(QColor("#aaaaaa"));
    cb.chart->setMargins(QMargins(4, 4, 4, 4));
    cb.chart->setAnimationOptions(QChart::NoAnimation);

    cb.axisX = new QValueAxis;
    cb.axisX->setRange(0, MAX_POINTS * DT);
    cb.axisX->setLabelFormat("%.0fs");
    cb.axisX->setLabelsColor(QColor("#888888"));
    cb.axisX->setGridLineColor(QColor("#1e2d4a"));
    cb.axisX->setLinePenColor(QColor("#1e2d4a"));
    cb.axisX->setTickCount(7);
    cb.chart->addAxis(cb.axisX, Qt::AlignBottom);

    cb.axisY = new QValueAxis;
    cb.axisY->setRange(-1, 1);
    cb.axisY->setLabelFormat("%.2f");
    cb.axisY->setLabelsColor(QColor("#888888"));
    cb.axisY->setGridLineColor(QColor("#1e2d4a"));
    cb.axisY->setLinePenColor(QColor("#1e2d4a"));
    cb.axisY->setTickCount(5);
    cb.chart->addAxis(cb.axisY, Qt::AlignLeft);

    cb.pts.resize(MAX_POINTS);

    cb.view = new QChartView(cb.chart);
    cb.view->setRenderHint(QPainter::Antialiasing);
    cb.view->setBackgroundBrush(QColor("#16213e"));
    cb.view->setFrameShape(QFrame::NoFrame);

    // Hover dot — one per chart, repositioned on hover
    cb.hoverDot = new QGraphicsEllipseItem(-5, -5, 10, 10);
    cb.hoverDot->setBrush(Qt::black);
    cb.hoverDot->setPen(QPen(QColor("#e0e0e0"), 1.5));
    cb.hoverDot->setZValue(10);
    cb.hoverDot->hide();
    cb.view->scene()->addItem(cb.hoverDot);

    cb.sigIds = sigIds;
    rebuildSeries(cb);

    // Thin header: just a right-aligned gear button
    auto *header = new QWidget;
    header->setFixedHeight(20);
    auto *hlay = new QHBoxLayout(header);
    hlay->setContentsMargins(2, 0, 2, 0);
    hlay->addStretch();

    auto *gearBtn = new QPushButton("⚙");
    gearBtn->setFixedSize(20, 18);
    gearBtn->setObjectName("gearBtn");
    gearBtn->setToolTip("Configure signals");
    connect(gearBtn, &QPushButton::clicked, this, [this, &cb]() {
        openConfigDialog(cb);
    });
    hlay->addWidget(gearBtn);

    auto *container = new QWidget;
    auto *vlay = new QVBoxLayout(container);
    vlay->setSpacing(0);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->addWidget(header);
    vlay->addWidget(cb.view, 1);

    cb.container = container;
    return container;
}

// ── rebuildSeries ─────────────────────────────────────────────────────────────

void MainWindow::rebuildSeries(ChartBundle &cb)
{
    for (auto *s : cb.series)
        cb.chart->removeSeries(s);
    qDeleteAll(cb.series);
    cb.series.clear();

    for (SigId id : cb.sigIds) {
        auto *s = new QLineSeries;
        s->setName(k_sigMeta[id].name);
        s->setPen(QPen(k_sigMeta[id].color, 1.8));
        cb.chart->addSeries(s);
        s->attachAxis(cb.axisX);
        s->attachAxis(cb.axisY);
        cb.series.append(s);

        connect(s, &QLineSeries::hovered, this, [this, id, s, &cb](const QPointF &pt, bool on) {
            if (on) {
                QToolTip::showText(QCursor::pos(),
                    QString("<b style='color:%1'>%2</b>&nbsp;&nbsp;%3 s : <b>%4</b>")
                        .arg(k_sigMeta[id].color.name())
                        .arg(k_sigMeta[id].name)
                        .arg(pt.x(), 0, 'f', 2)
                        .arg(pt.y(), 0, 'f', 3));
                cb.hoverDot->setPos(cb.chart->mapToPosition(pt, s));
                cb.hoverDot->show();
            } else {
                QToolTip::hideText();
                cb.hoverDot->hide();
            }
        });
    }

    QStringList names;
    for (SigId id : cb.sigIds)
        names << k_sigMeta[id].name;
    cb.chart->setTitle(names.isEmpty() ? "(empty)" : names.join("  "));
}

// ── appendChart ───────────────────────────────────────────────────────────────

void MainWindow::appendChart(ChartBundle &cb)
{
    if (cb.series.isEmpty()) return;

    double yMin =  DBL_MAX;
    double yMax = -DBL_MAX;

    const int count  = qMin(m_sample + 1, MAX_POINTS);
    const int xBase  = m_sample + 1 - count;
    const int offset = MAX_POINTS - count;

    if (cb.pts.size() != count)
        cb.pts.resize(count);

    for (int i = 0; i < cb.sigIds.size() && i < cb.series.size(); ++i) {
        const Ring &ring = m_rings[cb.sigIds[i]];
        for (int j = 0; j < count; ++j) {
            const double y = ring.at(offset + j);
            cb.pts[j] = QPointF((xBase + j) * DT, y);
            if (y < yMin) yMin = y;
            if (y > yMax) yMax = y;
        }
        cb.series[i]->replace(cb.pts);
    }

    cb.axisX->setRange(xBase * DT, (xBase + count) * DT);

    double margin = (yMax - yMin) * 0.12;
    if (margin < 0.01) margin = 0.01;
    cb.axisY->setRange(yMin - margin, yMax + margin);
}

// ── openConfigDialog ──────────────────────────────────────────────────────────

void MainWindow::openConfigDialog(ChartBundle &cb)
{
    QDialog dlg(this);
    dlg.setWindowTitle("Configure Chart");
    dlg.setFixedWidth(230);
    dlg.setStyleSheet(
        "QDialog { background:#1a1a2e; color:#e0e0e0; }"
        "QListWidget { background:#16213e; border:1px solid #0f3460; color:#e0e0e0; }"
        "QListWidget::item { padding:5px; }"
        "QListWidget::item:hover { background:#0f3460; }"
        "QPushButton { background:#0f3460; color:#e0e0e0; border:1px solid #1a5276;"
        "              border-radius:4px; padding:4px 12px; }"
        "QPushButton:hover { background:#1a5276; }");

    auto *lay = new QVBoxLayout(&dlg);

    auto *list = new QListWidget(&dlg);
    for (int i = 0; i < SIG_COUNT; ++i) {
        auto *item = new QListWidgetItem(k_sigMeta[i].name, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(cb.sigIds.contains(static_cast<SigId>(i))
                            ? Qt::Checked : Qt::Unchecked);
        item->setForeground(k_sigMeta[i].color);
    }
    lay->addWidget(list);

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted) return;

    cb.sigIds.clear();
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->checkState() == Qt::Checked)
            cb.sigIds.append(static_cast<SigId>(i));

    rebuildSeries(cb);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::onClearCharts()
{
    for (auto &r : m_rings)
        r.clear();
    m_roll = m_pitch = m_yaw = 0.0;
    m_sample = 0;
    m_elapsed.restart();
    m_lblTime->setText("0.0 s");
}

void MainWindow::onBootloaderClicked()
{
    if (!m_connected) {
        QMessageBox::warning(this, "Bootloader",
            "Not connected. Connect to the device first, then click Bootloader.");
        return;
    }

    // Send single-byte trigger command
    QByteArray cmd(1, (char)0x05);
    QMetaObject::invokeMethod(m_worker, "sendRawBytes",
                              Qt::QueuedConnection, Q_ARG(QByteArray, cmd));

    // Give the device time to process the command and reset (~300 ms)
    QThread::msleep(300);

    // Disconnect so the port is free for the flash dialog
    QMetaObject::invokeMethod(m_worker, "close", Qt::BlockingQueuedConnection);
    m_connected = false;
    m_connectBtn->setText("Connect");
    m_statusDot->setObjectName("statusDotOff");
    m_statusDot->style()->unpolish(m_statusDot);
    m_statusDot->style()->polish(m_statusDot);
    m_statusText->setText("Bootloader mode — LED fast-blinking");
}

void MainWindow::onFlashClicked()
{
    QString portName = m_portCombo->currentText();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, "Flash", "No port selected.");
        return;
    }

    if (m_connected) {
        QMessageBox::information(this, "Flash",
            "Please click 'Bootloader' first to reboot the device into bootloader mode,\n"
            "then click 'Flash' once the LED is fast-blinking.");
        return;
    }

    FlashDialog dlg(portName, this);
    dlg.exec();
}

void MainWindow::onRefreshPorts()
{
    m_portCombo->clear();
    for (const auto &info : QSerialPortInfo::availablePorts())
        m_portCombo->addItem(info.portName());
}

void MainWindow::onConnectClicked()
{
    if (m_connected) {
        QMetaObject::invokeMethod(m_worker, "close", Qt::QueuedConnection);
    } else {
        if (m_portCombo->currentText().isEmpty()) {
            QMessageBox::warning(this, "No port", "Select a COM port first.");
            return;
        }
        QMetaObject::invokeMethod(m_worker, "open", Qt::QueuedConnection,
            Q_ARG(QString, m_portCombo->currentText()),
            Q_ARG(int, 115200));
    }
}

void MainWindow::onLogClicked()
{
    if (m_csvFile) {
        stopLogging();
    } else {
        QString filename = QString("stm32_%1.csv")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

        m_csvFile = new QFile(filename);
        if (!m_csvFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Log error",
                "Cannot create file:\n" + m_csvFile->errorString());
            delete m_csvFile; m_csvFile = nullptr;
            return;
        }

        m_csvStream = new QTextStream(m_csvFile);
        *m_csvStream << "timestamp,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,"
                        "roll,pitch,yaw,temp_C,hum_pct,pressure_hPa,lux\n";

        m_logBtn->setText("⏹  Stop Log");
        m_logBtn->setStyleSheet("color:#e94560; font-weight:bold;");
        m_statusText->setText("Logging → " + filename);
    }
}

void MainWindow::stopLogging()
{
    if (!m_csvFile) return;
    m_csvStream->flush();
    m_csvFile->close();
    delete m_csvStream; m_csvStream = nullptr;
    delete m_csvFile;   m_csvFile   = nullptr;
    m_logBtn->setText("⏺  Start Log");
    m_logBtn->setStyleSheet("");
    if (!m_connected)
        m_statusText->setText("Disconnected");
}

void MainWindow::onConnectionChanged(bool connected)
{
    m_connected = connected;
    m_connectBtn->setText(connected ? "Disconnect" : "Connect");
    m_statusDot->setObjectName(connected ? "statusDotOn" : "statusDotOff");
    if (!m_csvFile)
        m_statusText->setText(connected ? "Connected" : "Disconnected");
    m_statusDot->style()->unpolish(m_statusDot);
    m_statusDot->style()->polish(m_statusDot);
}

void MainWindow::onSerialError(QString msg)
{
    m_statusText->setText("Error: " + msg);
}

void MainWindow::onRecord(SensorRecord rec)
{
    const double ax_g   = rec.imu.ax / ACCEL_SCALE;
    const double ay_g   = rec.imu.ay / ACCEL_SCALE;
    const double az_g   = rec.imu.az / ACCEL_SCALE;
    const double gx_dps = rec.imu.gx / GYRO_SCALE;
    const double gy_dps = rec.imu.gy / GYRO_SCALE;
    const double gz_dps = rec.imu.gz / GYRO_SCALE;

    // Complementary filter — angle estimation
    const double accel_roll  = atan2(ay_g, az_g) * RAD2DEG;
    const double accel_pitch = atan2(-ax_g, sqrt(ay_g*ay_g + az_g*az_g)) * RAD2DEG;
    m_roll  = ALPHA * (m_roll  + gx_dps * DT) + (1.0 - ALPHA) * accel_roll;
    m_pitch = ALPHA * (m_pitch + gy_dps * DT) + (1.0 - ALPHA) * accel_pitch;
    // Yaw has no accelerometer reference — integrate only above a deadband
    // to prevent gyro bias from accumulating when the device is stationary.
    const double gyro_mag = sqrt(gx_dps*gx_dps + gy_dps*gy_dps + gz_dps*gz_dps);
    if (gyro_mag > 1.0)
        m_yaw += gz_dps * DT;

    // Start timer on first sample; update every record
    if (!m_elapsed.isValid())
        m_elapsed.start();
    m_lblTime->setText(QString::number(m_elapsed.elapsed() / 1000.0, 'f', 1) + " s");

    // Value cards
    m_lblAx->setText(QString::number(ax_g,   'f', 3) + " g");
    m_lblAy->setText(QString::number(ay_g,   'f', 3) + " g");
    m_lblAz->setText(QString::number(az_g,   'f', 3) + " g");
    m_lblGx->setText(QString::number(gx_dps, 'f', 1) + " °/s");
    m_lblGy->setText(QString::number(gy_dps, 'f', 1) + " °/s");
    m_lblGz->setText(QString::number(gz_dps, 'f', 1) + " °/s");
    m_lblT->setText(QString::number(rec.env.temp,     'f', 1) + " °C");
    m_lblH->setText(QString::number(rec.env.hum,      'f', 1) + " %");
    m_thermometer->setTemperature(rec.env.temp);
    m_dropWidget->setHumidity(rec.env.hum);
    m_lampWidget->setLux(rec.env.lux);
    m_pressWidget->setPressure(rec.env.pressure);
    m_distWidget->setDistance(rec.env.dist_cm);
    m_orientWidget->setAngles(m_roll, m_pitch, m_yaw);
    m_lblP->setText(QString::number(rec.env.pressure) + " hPa");
    m_lblLux->setText(QString::number(rec.env.lux)    + " lx");
    m_lblRoll->setText(QString::number(m_roll,  'f', 1) + "°");
    m_lblPitch->setText(QString::number(m_pitch,'f', 1) + "°");
    m_lblYaw->setText(QString::number(m_yaw,   'f', 1) + "°");
    m_lblDist->setText(QString::number(rec.env.dist_cm) + " cm");

    // Push all signals into shared rings
    m_rings[SIG_AX].push(ax_g);
    m_rings[SIG_AY].push(ay_g);
    m_rings[SIG_AZ].push(az_g);
    m_rings[SIG_GX].push(gx_dps);
    m_rings[SIG_GY].push(gy_dps);
    m_rings[SIG_GZ].push(gz_dps);
    m_rings[SIG_ROLL].push(m_roll);
    m_rings[SIG_PITCH].push(m_pitch);
    m_rings[SIG_YAW].push(m_yaw);
    m_rings[SIG_TEMP].push(rec.env.temp);
    m_rings[SIG_HUM].push(rec.env.hum);
    m_rings[SIG_PRESSURE].push(rec.env.pressure);
    m_rings[SIG_LUX].push(rec.env.lux);
    m_rings[SIG_DIST].push(rec.env.dist_cm);

    for (auto &cb : m_charts)
        appendChart(cb);

    ++m_sample;

    if (m_csvStream) {
        *m_csvStream << rec.ts.toString(Qt::ISODate) << ','
                     << ax_g   << ',' << ay_g   << ',' << az_g   << ','
                     << gx_dps << ',' << gy_dps << ',' << gz_dps << ','
                     << m_roll << ',' << m_pitch << ',' << m_yaw << ','
                     << rec.env.temp << ',' << rec.env.hum << ','
                     << rec.env.pressure << ',' << rec.env.lux << '\n';
    }
}
