#ifndef MAINWINDOW_H
#define MAINWINDOW_H

//Includes for this project
#include "structuredefinitions.h"
#include "internalsolver.h"

//system includes
#include "math.h"
#include <sys/mman.h>

//QT Includes
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
#include <QTime>
#include <QThread>

//CFitsio Includes
#include "longnam.h"
#include "fitsio.h"

//KStars related includes
#include "stretch.h"
#include "math.h"
#include "dms.h"
#include "bayer.h"

//Astrometry.net includes
extern "C"{
#include "tic.h"
#include "os-features.h"
#include "fileutils.h"
#include "ioutils.h"
#include "bl.h"
#include "an-bool.h"
#include "solver.h"
#include "fitsioutils.h"
#include "blindutils.h"
#include "astrometry/blind.h"
#include "log.h"
#include "engine.h"
#include "an-opts.h"
#include "gslutils.h"
#include "augment-xylist.h"
#include "anqfits.h"
#include "ioutils.h"
#include "fitsioutils.h"
#include "permutedsort.h"
#include "fitstable.h"
#include "sip_qfits.h"
#include "sip-utils.h"
#include "tabsort.h"
}

namespace Ui {

class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow();
    ~MainWindow();

    void setStarList(QList<Star> starList){stars = starList;};

private:
    Ui::MainWindow *ui;
    QString dirPath;
    QPointer<QProcess> solver;
    QPointer<InternalSolver> internalSolver;
    QPointer<QProcess> sextractorProcess;
    QString fileToSolve;
    QString sextractorFilePath;
    QList<Star> stars;
    int selectedStar;
    BayerParams debayerParams;
    bool checkDebayer();

    //Parameters for sovling
    bool use_position = false;
    double ra;
    double dec;
    bool use_scale = false;
    QString fov_low, fov_high;
    QString units;

    //Data about the image
    bool imageLoaded = false;
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

    augment_xylist_t theallaxy;
    augment_xylist_t* allaxy = &theallaxy;


    QTime solverTimer;

    void removeTempFile(char * fileName);

    char* charQStr(QString in);

public slots:

    bool prepareForProcesses();
    void logOutput(QString text);
    void logSolver();
    void logSextractor();
    void clearLog();

    //These are the functions that run when the bottom buttons are clicked
    bool sextractImage();
    bool solveImage();
    bool sextractInternally();
    bool solveInternally();
    void abort();

    //These functions are for loading and displaying the table
    bool loadImage();
    bool loadFits();
    bool loadOtherFormat();
    bool saveAsFITS();
    bool debayer();
    bool debayer_8bit();
    bool debayer_16bit();
    void initDisplayImage();
    void doStretch(QImage *outputImage);
    void clearImageBuffers();

    void zoomIn();
    void zoomOut();
    void autoScale();
    void updateImage();

    //These functions are for the display of stars in the table
    void displayTable();
    void sortStars();
    void starClickedInTable();
    void updateStarTableFromList();
    bool getSextractorTable();

    //These functions are for loading and parsing the options
    QStringList getSolverArgsList();
    bool getSolverOptionsFromFITS();

    //These functions are for the external sextractor and solver
    bool sextract();
    bool solveField();

    //These functions are for the internal sextractor and solver
    bool runInnerSextractor();
    bool runInnerSolver();

    //These are for both
    bool sextractorComplete();
    bool solverComplete(int x);
};

#endif // MAINWINDOW_H
