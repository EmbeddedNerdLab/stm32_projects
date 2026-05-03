#pragma once
#include <QMainWindow>
#include <QThread>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QElapsedTimer>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include "sensordata.h"
#include "widgets.h"

class SerialWorker;

enum SigId {
    SIG_AX=0, SIG_AY, SIG_AZ,
    SIG_GX,   SIG_GY, SIG_GZ,
    SIG_ROLL, SIG_PITCH, SIG_YAW,
    SIG_TEMP, SIG_HUM, SIG_PRESSURE, SIG_LUX,
    SIG_DIST,
    SIG_COUNT
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onLogClicked();
    void onClearCharts();
    void onBootloaderClicked();
    void onFlashClicked();
    void onRefreshPorts();
    void onRecord(SensorRecord rec);
    void onConnectionChanged(bool connected);
    void onSerialError(QString msg);

private:
    static constexpr int    MAX_POINTS  = 120;
    static constexpr double ACCEL_SCALE = 16384.0;
    static constexpr double GYRO_SCALE  = 131.0;
    static constexpr double DT          = 0.05;
    static constexpr double ALPHA       = 0.96;
    static constexpr double RAD2DEG     = 57.29577951308232;
    static constexpr int    NUM_CHARTS  = 2;

    struct Ring {
        QVector<double> data;
        int head = 0;
        Ring() : data(MAX_POINTS, 0.0) {}
        void push(double v) { data[head % MAX_POINTS] = v; ++head; }
        double at(int i) const { return data[(head + i) % MAX_POINTS]; }
        void clear() { data.fill(0.0); head = 0; }
    };

    struct ChartBundle {
        QWidget               *container = nullptr;
        QChartView            *view      = nullptr;
        QChart                *chart     = nullptr;
        QValueAxis            *axisX     = nullptr;
        QValueAxis            *axisY     = nullptr;
        QGraphicsEllipseItem  *hoverDot  = nullptr;
        QList<SigId>           sigIds;
        QList<QLineSeries*>    series;
        QList<QPointF>         pts;
    };

    struct SigMeta { QString name; QColor color; };
    static const SigMeta k_sigMeta[SIG_COUNT];

    // Shared ring buffers — all updated every frame
    Ring m_rings[SIG_COUNT];

    // Top bar
    QComboBox   *m_portCombo  = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_logBtn     = nullptr;
    QPushButton *m_clearBtn   = nullptr;
    QPushButton *m_bootBtn    = nullptr;
    QPushButton *m_flashBtn   = nullptr;
    QLabel      *m_statusDot  = nullptr;
    QLabel      *m_statusText = nullptr;
    QLabel      *m_lblTime    = nullptr;
    QElapsedTimer m_elapsed;

    // Value cards
    QLabel *m_lblAx, *m_lblAy, *m_lblAz;
    QLabel *m_lblGx, *m_lblGy, *m_lblGz;
    QLabel *m_lblT,  *m_lblH,  *m_lblP, *m_lblLux;
    QLabel *m_lblRoll, *m_lblPitch, *m_lblYaw;
    QLabel *m_lblDist = nullptr;

    // Fancy env widgets
    ThermometerWidget *m_thermometer  = nullptr;
    HumidityWidget    *m_dropWidget   = nullptr;
    LampWidget        *m_lampWidget   = nullptr;
    PressureWidget    *m_pressWidget  = nullptr;
    DistanceWidget    *m_distWidget   = nullptr;
    OrientationWidget *m_orientWidget = nullptr;

    ChartBundle m_charts[NUM_CHARTS];

    double m_roll  = 0.0;
    double m_pitch = 0.0;
    double m_yaw   = 0.0;

    QThread      *m_thread    = nullptr;
    SerialWorker *m_worker    = nullptr;
    int           m_sample    = 0;
    bool          m_connected = false;

    QFile        *m_csvFile   = nullptr;
    QTextStream  *m_csvStream = nullptr;

    QWidget*    buildTopBar();
    QWidget*    buildLeftPanel();
    QWidget*    buildRightPanel();
    QWidget*    makeEnvCard();
    QWidget*    makeDistCard();
    QWidget*    makeOrientCard();
    QWidget*    makeCard(const QString &title,
                         const QStringList &names,
                         const QList<QLabel**> &labels,
                         const QString &color);
    QWidget*    makeChartPanel(ChartBundle &cb, const QList<SigId> &sigIds);
    void        rebuildSeries(ChartBundle &cb);
    void        appendChart(ChartBundle &cb);
    void        openConfigDialog(ChartBundle &cb);
    void        stopLogging();
};
