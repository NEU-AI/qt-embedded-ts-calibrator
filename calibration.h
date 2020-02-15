#ifndef CALIBRATION_H
#define CALIBRATION_H
#include <QThread>
#include <QDialog>
#include <QPoint>
#include <tslib.h>

typedef struct {
    int x[5], xfb[5];
    int y[5], yfb[5];
    int a[7];
} calibration;

class CalibThread : public QThread
{
    Q_OBJECT

private:
    calibration cal;
    struct tsdev *ts;
    int xres, yres;
    void getSample (int index, int x, int y, char *name);
    void getxy(tsdev *ts, int *x, int *y);
    static int sort_by_x(const void *a, const void *b);
    static int sort_by_y(const void *a, const void *b);
public:
    bool config();
    int performCalibration();
    int calibrationWrite();
    void run();

signals:
    void nextPoint(int x,int y);
};

class Calibrator : public QDialog
{
    Q_OBJECT

private:
    CalibThread* calibThread;
    QPoint screenPoint;
public:
    Calibrator();
    int exec();
protected:
    void paintEvent(QPaintEvent *);
    void accept();
public slots:
    void onNextPoint(int x,int y);
};

#endif // CALIBRATION_H
