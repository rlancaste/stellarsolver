#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QObject>
#include <QWidget>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QProcess>
#include <QPointer>
#include <QScrollBar>
#include "longnam.h"
#include "fitsio.h"
#include "stretch.h"
#include "math.h"
#include "dms.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

    /// Stats struct to hold statisical data about the FITS data
    typedef struct
    {
        double min[3] = {0}, max[3] = {0};
        double mean[3] = {0};
        double stddev[3] = {0};
        double median[3] = {0};
        double SNR { 0 };
        int bitpix { 8 };
        int bytesPerPixel { 1 };
        int ndim { 2 };
        int64_t size { 0 };
        uint32_t samples_per_channel { 0 };
        uint16_t width { 0 };
        uint16_t height { 0 };
    } Statistic;
public:
    explicit MainWindow();
    ~MainWindow();


private:
    Ui::MainWindow *ui;
    QString dirPath;
    QPointer<QProcess> solver;
    QPointer<QProcess> sextractorProcess;
    QString fileToSolve;
    QString sextractorFilePath;
    Statistic stats;
    fitsfile *fptr { nullptr };

    QImage rawImage;
    QImage scaledImage;
    int currentWidth;
    int currentHeight;
    double currentZoom;
    int sampling = 2;
    /// FITS image data type (TBYTE, TUSHORT, TINT, TFLOAT, TLONG, TDOUBLE)
    uint32_t m_DataType { 0 };
    /// Number of channels
    uint8_t m_Channels { 1 };
    /// Generic data image buffer
    uint8_t *m_ImageBuffer { nullptr };
    /// Above buffer size in bytes
    uint32_t m_ImageBufferSize { 0 };
    StretchParams stretchParams;
public slots:
    bool loadImage();
    bool solveImage();
    void abort();

    bool loadFits();
    void initDisplayImage();
    void zoomIn();
    void zoomOut();
    void autoScale();

    void doStretch(QImage *outputImage);
    void clearImageBuffers();
    bool sextract();
    QStringList getSolverOptionsFromFITS();
    bool solveField();

    void log(QString text);
    void logSolver();
    void logSextractor();
    void clearLog();
};

#endif // MAINWINDOW_H
