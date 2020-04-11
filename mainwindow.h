#ifndef MAINWINDOW_H
#define MAINWINDOW_H

//Includes for this project
#include "structuredefinitions.h"
#include "sexysolver.h"
#include "externalsextractorsolver.h"

//system includes
#include "math.h"

#ifndef _MSC_VER
#include <sys/mman.h>
#endif

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
#include <QLineEdit>

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

    QPointer<SexySolver> sexySolver;
    QPointer<ExternalSextractorSolver> extSolver;
    QString fileToSolve;
    QList<Star> stars;
    int selectedStar;

    //System File Paths
    QString dirPath = QDir::homePath();
    QString tempPath;
    QStringList indexFilePaths;
    QString sextractorBinaryPath;
    QString confPath;
    QString solverPath;
    QString wcsPath;

    //Parameters for sextracting
    Shape apertureShape;
    double kron_fact;
    int subpix;
    float r_min;
    short inflags;
    float magzero;
    float minarea;
    int deblend_thresh;
    float deblend_contrast;
    int clean;
    double clean_param;
    double fwhm;

    //Star Filter Parameters
    bool resort;
    double maxEllipse;
    double removeBrightest;
    double removeDimmest;

    //Parameters for solving
    bool use_scale = false;
    double fov_low, fov_high;
    QString units;
    bool use_position = false;
    double ra;
    double dec;
    double radius;

    bool inParallel;
    int solverTimeLimit;
    double minwidth;
    double maxwidth;

    bool logToFile;
    QString logFile;
    int logLevel;

    //LogOdds Settings
    double logratio_tosolve;
    double logratio_tokeep;
    double logratio_totune;

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
    /// Number of channels
    uint8_t m_Channels { 1 };
    /// Generic data image buffer
    uint8_t *m_ImageBuffer { nullptr };
    /// Above buffer size in bytes
    uint32_t m_ImageBufferSize { 0 };
    StretchParams stretchParams;
    BayerParams debayerParams;
    bool checkDebayer();

    QTime solverTimer;
    double elapsed;

    void setupSolutionTable();
    void clearAstrometrySettings();
    void addSextractionToTable();

    // This is the xyls file path that sextractor will be saving for Astrometry.net
    // If it is not set, it will be set to a random temporary file
    QString sextractorFilePath;
    QString basePath = QDir::tempPath();

    //These are used for reading and writing the sextractor file
    char* xcol=strdup("X_IMAGE"); //This is the column for the x-coordinates
    char* ycol=strdup("Y_IMAGE"); //This is the column for the y-coordinates
    char* colFormat=strdup("1E"); //This Format means a decimal number
    char* colUnits=strdup("pixels"); //This is the unit for the columns in the file

public slots:

    bool prepareForProcesses();
    void logOutput(QString text);
    void clearAll();
    void resetOptionsToDefaults();

    //These are the functions that run when the bottom buttons are clicked
    bool sextractImage();
    bool solveImage();
    bool sextractExternally(bool justSextract);
    bool solveExternally();
    bool sextractInternally();
    bool solveInternally();
    void abort();

    //These functions are for loading and displaying the image
    bool imageLoad();
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

    void mouseOverStar(QPoint location);
    void mouseClickedOnStar(QPoint location);
    QRect getStarInImage(Star star);

    //This function is for loading and parsing the options
    bool getSolverOptionsFromFITS();

    //This function will write the sextractor table to a xyls file for the solver if desired.
    bool writeSextractorTable();

    //These functions set the settings for the Sextractors and Solvers
    void setSextractorSettings();
    void setSolverSettings();
    void setupExternalSextractorSolver();
    void setupInternalSexySolver();

    //These functions get called when the sextractor or solver finishes
    bool sextractorComplete(int error);
    bool solverComplete(int error);

    //These functions handle the solution table
    void addSolutionToTable(Solution solution);
    void saveSolutionTable();

};

#endif // MAINWINDOW_H
