#include <QApplication>
#include <QFile>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("STM32Monitor");
    app.setOrganizationName("STM32Project");

    // Load dark stylesheet
    QFile qss("style.qss");
    if (qss.open(QFile::ReadOnly))
        app.setStyleSheet(qss.readAll());

    MainWindow w;
    w.show();
    return app.exec();
}
