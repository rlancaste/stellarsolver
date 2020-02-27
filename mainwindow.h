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
#include "bayer.h"
#include <QTime>

extern "C"{
#include "tabsort.h"
#include "resort-xylist.h"
#include "os-features.h"
#include "tic.h"
#include "fileutils.h"
#include "ioutils.h"
#include "bl.h"
#include "an-bool.h"
#include "solver.h"
#include "math.h"
#include "fitsioutils.h"
#include "blindutils.h"
#include "blind.h"
#include "log.h"
#include "errors.h"
#include "engine.h"
#include "an-opts.h"
#include "gslutils.h"
#include "augment-xylist.h"


#include "sip_qfits.h"
#include "sip-utils.h"
}


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
    QPointer<QThread> internalSolver;
    QPointer<QProcess> sextractorProcess;
    QString fileToSolve;
    QString sextractorFilePath;
    QList<QPointF> stars;
    BayerParams debayerParams;
    bool checkDebayer();
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

    QTime solverTimer;
    augment_xylist_t theallaxy;
    augment_xylist_t* allaxy = &theallaxy;

    void removeTempFile(char * fileName);

public slots:
    bool loadImage();
    bool solveImage();
    void abort();

    bool loadFits();
    bool loadOtherFormat();
    bool saveAsFITS();
    bool debayer();
    bool debayer_8bit();
    bool debayer_16bit();
    void initDisplayImage();
    void zoomIn();
    void zoomOut();
    void autoScale();
    void updateImage();

    void doStretch(QImage *outputImage);
    void clearImageBuffers();
    bool sextractAndDisplay();
    bool sextract();
    bool getSextractorTable();
    QStringList getSolverOptionsFromFITS();
    bool solveField();

    void logOutput(QString text);
    void logSolver();
    void logSextractor();
    void clearLog();

    bool solveInternally();
    bool augmentXYList();
    int runEngine();

    bool solverComplete(int x);
};

#endif // MAINWINDOW_H
