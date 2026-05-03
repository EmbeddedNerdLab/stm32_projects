#pragma once
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QThread>
#include "flashworker.h"

class FlashDialog : public QDialog {
    Q_OBJECT
public:
    explicit FlashDialog(const QString &portName, QWidget *parent = nullptr);
    ~FlashDialog();

private slots:
    void onBrowse();
    void onStart();
    void onProgress(int pct, QString status);
    void onFinished(bool success, QString msg);

private:
    QString         m_port;
    QLineEdit      *m_filePath  = nullptr;
    QPushButton    *m_browseBtn = nullptr;
    QPushButton    *m_startBtn  = nullptr;
    QPushButton    *m_closeBtn  = nullptr;
    QProgressBar   *m_bar       = nullptr;
    QPlainTextEdit *m_log       = nullptr;
    QLabel         *m_portLabel = nullptr;

    QThread      *m_thread = nullptr;
    FlashWorker  *m_worker = nullptr;

    void log(const QString &msg);
    void setRunning(bool running);
};
