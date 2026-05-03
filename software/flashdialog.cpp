#include "flashdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDateTime>

FlashDialog::FlashDialog(const QString &portName, QWidget *parent)
    : QDialog(parent), m_port(portName)
{
    setWindowTitle("Firmware Update");
    setMinimumSize(520, 380);
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(16, 16, 16, 16);

    // Port info
    m_portLabel = new QLabel(QString("Target port: <b>%1</b>  —  115200 baud").arg(portName));
    root->addWidget(m_portLabel);

    // File picker row
    auto *fileRow = new QHBoxLayout;
    m_filePath = new QLineEdit;
    m_filePath->setPlaceholderText("Select encrypted firmware .enc file...");
    m_browseBtn = new QPushButton("Browse...");
    m_browseBtn->setFixedWidth(90);
    fileRow->addWidget(m_filePath);
    fileRow->addWidget(m_browseBtn);
    root->addLayout(fileRow);

    // Progress bar
    m_bar = new QProgressBar;
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
    m_bar->setTextVisible(true);
    root->addWidget(m_bar);

    // Log
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(200);
    m_log->setFont(QFont("Consolas", 9));
    root->addWidget(m_log, 1);

    // Buttons row
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_startBtn = new QPushButton("Start Update");
    m_startBtn->setFixedWidth(120);
    m_closeBtn = new QPushButton("Close");
    m_closeBtn->setFixedWidth(80);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_closeBtn);
    root->addLayout(btnRow);

    connect(m_browseBtn, &QPushButton::clicked, this, &FlashDialog::onBrowse);
    connect(m_startBtn,  &QPushButton::clicked, this, &FlashDialog::onStart);
    connect(m_closeBtn,  &QPushButton::clicked, this, &QDialog::reject);

    log(QString("Port: %1 — waiting for device in bootloader mode.").arg(portName));
    log("Make sure the LED is fast-blinking before clicking Start Update.");
}

FlashDialog::~FlashDialog()
{
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait(2000);
    }
}

void FlashDialog::onBrowse()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Select Encrypted Firmware", QString(),
        "Encrypted firmware (*.enc);;All files (*)");
    if (!path.isEmpty())
        m_filePath->setText(path);
}

void FlashDialog::onStart()
{
    QString path = m_filePath->text().trimmed();
    if (path.isEmpty()) {
        log("ERROR: No firmware file selected.");
        return;
    }

    setRunning(true);
    m_bar->setValue(0);
    log(QString("=== Starting firmware update on %1 ===").arg(m_port));
    log(QString("File: %1").arg(path));

    m_thread = new QThread(this);
    m_worker = new FlashWorker(m_port, path);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started,   m_worker, &FlashWorker::run);
    connect(m_worker, &FlashWorker::progress, this, &FlashDialog::onProgress);
    connect(m_worker, &FlashWorker::finished, this, &FlashDialog::onFinished);
    connect(m_worker, &FlashWorker::finished, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_thread->start();
}

void FlashDialog::onProgress(int pct, QString status)
{
    m_bar->setValue(pct);
    log(QString("[%1%] %2").arg(pct, 3).arg(status));
}

void FlashDialog::onFinished(bool success, QString msg)
{
    setRunning(false);
    if (success) {
        m_bar->setValue(100);
        log("=== SUCCESS: " + msg + " ===");
    } else {
        log("=== FAILED: " + msg + " ===");
    }
    m_thread = nullptr;
    m_worker = nullptr;
}

void FlashDialog::log(const QString &msg)
{
    m_log->appendPlainText(
        QDateTime::currentDateTime().toString("hh:mm:ss") + "  " + msg);
}

void FlashDialog::setRunning(bool running)
{
    m_startBtn->setEnabled(!running);
    m_browseBtn->setEnabled(!running);
    m_filePath->setEnabled(!running);
    m_closeBtn->setEnabled(!running);
}
