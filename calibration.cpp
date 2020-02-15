#include "calibration.h"
#include "calibration.h"
#include <tslib.h>

#include <QApplication>
#include <QRect>
#include <QScreen>
#include <QDesktopWidget>
#include <QFile>
#include <QWSServer>
#include <QPainter>
#include <QMouseEvent>

Calibrator::Calibrator()
{
    QRect desktop = QApplication::desktop()->geometry();
    desktop.moveTo(QPoint(0, 0));
    setGeometry(desktop);

    setFocusPolicy(Qt::StrongFocus);
    setFocus();
    setModal(true);

}

int Calibrator::exec()
{
    activateWindow();
    calibThread = new CalibThread ;
    if(calibThread->config()){
        connect(calibThread, SIGNAL(nextPoint(int,int)),
                this, SLOT(onNextPoint(int,int)), Qt::QueuedConnection);
        calibThread->start();
        int ret = QDialog::exec();
        return ret;
    }else{
        delete calibThread;
        return 0;
    }
}

void Calibrator::paintEvent(QPaintEvent*)
{
    QPainter p;
    p.begin(this);
    p.fillRect(rect(), Qt::white);

    // Map to logical coordinates in case the screen is transformed
    QSize screenSize(qt_screen->deviceWidth(), qt_screen->deviceHeight());
    QPoint point = qt_screen->mapFromDevice(screenPoint, screenSize);

    p.fillRect(point.x() - 6, point.y() - 1, 13, 3, Qt::black);
    p.fillRect(point.x() - 1, point.y() - 6, 3, 13, Qt::black);
    p.end();
}

void Calibrator::accept()
{
    if (calibThread->performCalibration()) {
        calibThread->calibrationWrite();
    } else {
        printf("perform_calibration failed.");
    }

    delete calibThread;

    QWSServer::instance()->closeMouse();
    QWSServer::instance()->openMouse();
    QDialog::accept();
}

void Calibrator::onNextPoint(int x,int y)
{
    if (x>0 && y>0) {
        screenPoint.setX(x);
        screenPoint.setY(y);
        repaint();
    }
    else
        accept();
}

int CalibThread::calibrationWrite()
{
    char *calfile = NULL;
    char cal_buffer[256];
    unsigned int len;
    QFile file;

    if ((calfile = getenv("TSLIB_CALIBFILE")) != NULL) {
        file.setFileName(calfile);
        if(!file.open(QIODevice::ReadWrite | QIODevice::Text)){
            printf("TSLIB_CALIBFILE.open error");
            return -1;
        }
    } else {
        file.setFileName("/etc/pointercal");
        if(!file.open(QIODevice::ReadWrite | QIODevice::Text)){
            printf("file.open error");
            return -1;
        }
    }
    len = sprintf(cal_buffer,"%d %d %d %d %d %d %d %d %d ",
                  cal.a[1], cal.a[2], cal.a[0],
                  cal.a[4], cal.a[5], cal.a[3], cal.a[6],
                  xres, yres);

    printf("Calibration constants: %s\n", cal_buffer);

    file.write(cal_buffer, len);
    file.close();
    return 0;
}


int CalibThread::performCalibration()
{
    int j;
    float n, x, y, x2, y2, xy, z, zx, zy;
    float det, a, b, c, e, f, i;
    float scaling = 65536.0;

    /* Get sums for matrix */
    n = x = y = x2 = y2 = xy = 0;
    for (j = 0; j < 5; j++) {
        n += 1.0;
        x += (float)cal.x[j];
        y += (float)cal.y[j];
        x2 += (float)(cal.x[j]*cal.x[j]);
        y2 += (float)(cal.y[j]*cal.y[j]);
        xy += (float)(cal.x[j]*cal.y[j]);
    }

    /* Get determinant of matrix -- check if determinant is too small */
    det = n*(x2*y2 - xy*xy) + x*(xy*y - x*y2) + y*(x*xy - y*x2);
    if (det < 0.1 && det > -0.1) {
        printf("ts_calibrate: determinant is too small -- %f\n", det);
        return 0;
    }

    /* Get elements of inverse matrix */
    a = (x2*y2 - xy*xy)/det;
    b = (xy*y - x*y2)/det;
    c = (x*xy - y*x2)/det;
    e = (n*y2 - y*y)/det;
    f = (x*y - n*xy)/det;
    i = (n*x2 - x*x)/det;

    /* Get sums for x calibration */
    z = zx = zy = 0;
    for (j = 0; j < 5; j++) {
        z += (float)cal.xfb[j];
        zx += (float)(cal.xfb[j]*cal.x[j]);
        zy += (float)(cal.xfb[j]*cal.y[j]);
    }

    /* Now multiply out to get the calibration for framebuffer x coord */
    cal.a[0] = (int)((a*z + b*zx + c*zy)*(scaling));
    cal.a[1] = (int)((b*z + e*zx + f*zy)*(scaling));
    cal.a[2] = (int)((c*z + f*zx + i*zy)*(scaling));

    printf("%f %f %f\n", (a*z + b*zx + c*zy),
                 (b*z + e*zx + f*zy),
                 (c*z + f*zx + i*zy));

    /* Get sums for y calibration */
    z = zx = zy = 0;
    for (j = 0; j < 5; j++) {
        z += (float)cal.yfb[j];
        zx += (float)(cal.yfb[j]*cal.x[j]);
        zy += (float)(cal.yfb[j]*cal.y[j]);
    }

    /* Now multiply out to get the calibration for framebuffer y coord */
    cal.a[3] = (int)((a*z + b*zx + c*zy)*(scaling));
    cal.a[4] = (int)((b*z + e*zx + f*zy)*(scaling));
    cal.a[5] = (int)((c*z + f*zx + i*zy)*(scaling));

    printf("%f %f %f\n", (a*z + b*zx + c*zy),
                 (b*z + e*zx + f*zy),
                 (c*z + f*zx + i*zy));

    /* If we got here, we're OK, so assign scaling to a[6] and return */
    cal.a[6] = (int)scaling;

    return 1;
}

int CalibThread::sort_by_x(const void *a, const void *b)
{
    return (((struct ts_sample *)a)->x - ((struct ts_sample *)b)->x);
}

int CalibThread::sort_by_y(const void *a, const void *b)
{
    return (((struct ts_sample *)a)->y - ((struct ts_sample *)b)->y);
}

/* Waits for the screen to be touched, averages x and y sample
 * coordinates until the end of contact
 */
void CalibThread::getxy(struct tsdev *ts, int *x, int *y)
{
#define MAX_SAMPLES 128
    struct ts_sample samp[MAX_SAMPLES];
    int index, middle;

    do {
        if (ts_read_raw(ts, &samp[0], 1) < 0) {
            perror("ts_read_raw");
            // ToDo: Notify user!!!
            return;
        }
    } while (samp[0].pressure == 0);

    /* Now collect up to MAX_SAMPLES touches into the samp array. */
    index = 0;
    do {
        if (index < MAX_SAMPLES-1)
            index++;
        if (ts_read_raw(ts, &samp[index], 1) < 0) {
            perror("ts_read_raw");
            // ToDo: Notify user!!!
            return;
        }
    } while (samp[index].pressure > 0);
    printf("Took %d samples...\n", index);

    /*
     * At this point, we have samples in indices zero to (index-1)
     * which means that we have (index) number of samples.  We want
     * to calculate the median of the samples so that wild outliers
     * don't skew the result.  First off, let's assume that arrays
     * are one-based instead of zero-based.  If this were the case
     * and index was odd, we would need sample number ((index+1)/2)
     * of a sorted array; if index was even, we would need the
     * average of sample number (index/2) and sample number
     * ((index/2)+1).  To turn this into something useful for the
     * real world, we just need to subtract one off of the sample
     * numbers.  So for when index is odd, we need sample number
     * (((index+1)/2)-1).  Due to integer division truncation, we
     * can simplify this to just (index/2).  When index is even, we
     * need the average of sample number ((index/2)-1) and sample
     * number (index/2).  Calculate (index/2) now and we'll handle
     * the even odd stuff after we sort.
     */
    middle = index/2;
    if (x) {
        qsort(samp, index, sizeof(struct ts_sample), sort_by_x);
        if (index & 1)
            *x = samp[middle].x;
        else
            *x = (samp[middle-1].x + samp[middle].x) / 2;
    }
    if (y) {
        qsort(samp, index, sizeof(struct ts_sample), sort_by_y);
        if (index & 1)
            *y = samp[middle].y;
        else
            *y = (samp[middle-1].y + samp[middle].y) / 2;
    }
}

void CalibThread::getSample(int index, int x, int y, char *name)
{
//    static int last_x = -1, last_y;

//    if (last_x != -1) {
//#define NR_STEPS 10
//        int dx = ((x - last_x) << 16) / NR_STEPS;
//        int dy = ((y - last_y) << 16) / NR_STEPS;
//        int i;

//        last_x <<= 16;
//        last_y <<= 16;
//        for (i = 0; i < NR_STEPS; i++) {
//            put_cross(last_x >> 16, last_y >> 16, 2 | XORMODE);
//            usleep(1000);
//            put_cross(last_x >> 16, last_y >> 16, 2 | XORMODE);
//            last_x += dx;
//            last_y += dy;
//        }
//    }

//    put_cross(x, y, 2 | XORMODE);
    getxy(ts, &cal.x[index], &cal.y[index]);
//    put_cross(x, y, 2 | XORMODE);

    /*last_x = */cal.xfb[index] = x;
    /*last_y = */cal.yfb[index] = y;

    printf("%s : X = %4d Y = %4d\n", name, cal.x[index], cal.y[index]);
}

bool CalibThread::config()
{
    char *tsdevice = NULL;
    if( (tsdevice = getenv("TSLIB_TSDEVICE")) != NULL ) {
        ts = ts_open(tsdevice,0);
        printf("getenv TSLIB_TSDEVICE: %s",tsdevice);
    } else {
        printf("getenv TSLIB_TSDEVICE NULL");
        return false;
    }

    if (!ts) {
        printf("ts NULL");
        return false;
    }
    if (ts_config(ts)) {
        printf("ts_config error");
        return false;
    }

    xres = qt_screen->deviceWidth();
    yres = qt_screen->deviceHeight();
    printf("ts_config ok, xres: %d, yres: %d", xres, yres);
    return true;
}

void CalibThread::run()
{
    int dx, dy;
    dx = xres * 3 / 10;
    dy = yres * 3 / 10;

    QPoint points[] = {
        QPoint(dx, dy),
        QPoint(xres - dx, dy),
        QPoint(xres - dx, yres - dy),
        QPoint(dx, yres - dy),
        QPoint(xres / 2, yres / 2)
    };
    char* names[] = {
        (char*)"Top left",
        (char*)"Top right",
        (char*)"Bot right",
        (char*)"Bot left",
        (char*)"Center"
    };

    for(int i=0;i<5;i++) {
        QPoint *p = &points[i];
        emit nextPoint(p->x(), p->y());
        getSample ( i, p->x(), p->y(), names[i]);
    }

    emit nextPoint(-1,-1);
}
