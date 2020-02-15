#include "mainwindow.h"
#include <QApplication>
#include "calibration.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.showMaximized();

    Calibrator cal;
    cal.exec();
    return a.exec();
}
