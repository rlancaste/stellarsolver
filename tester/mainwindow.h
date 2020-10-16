/*  MainWindow for StellarSolver Tester Application, developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "externalsextractorsolver.h"
#include "onlinesolver.h"

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
#include <QThread>
#include <QLineEdit>
#include <QElapsedTimer>
#include <QTimer>

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

    void setStarList(QList<FITSImage::Star> starList){stars = starList;};

private:
    Ui::MainWindow *ui;

    std::unique_ptr<StellarSolver> stellarSolver;
    QString fileToProcess;
    QList<FITSImage::Star> stars;
    int selectedStar;

    QList<SSolver::Parameters> optionsList;
    bool optionsAreSaved = true;

    //Options for StellarSolver Tester
    QString dirPath = QDir::homePath();
    bool hasHFRData = false;
    bool hasWCSData = false;
    bool showFluxInfo = false;
    bool showStarShapeInfo = false;
    bool showSextractorParams = false;
    bool showAstrometryParams = false;
    bool showSolutionDetails = true;
    bool settingSubframe = false;

    bool useSubframe = false;
    QRect subframe;

    SSolver::ProcessType processType;
    SSolver::SextractorType sextractorType;
    SSolver::SolverType solverType;
    int profileSelection;

    //This allows for averaging over multiple trials
    int currentTrial = 0;
    int numberOfTrials = 0;
    double totalTime = 0;

    //Data about the image
    bool imageLoaded = false;
    FITSImage::Statistic stats;
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

    QElapsedTimer processTimer;
    QTimer timerMonitor;
    double elapsed;

    void setupResultsTable();
    void clearAstrometrySettings();
    void addSextractionToTable();
    FITSImage::Solution lastSolution;

    FITSImage::wcs_point *wcs_coord { nullptr };

public slots:

    bool prepareForProcesses();
    void logOutput(QString text);
    void startProcessMonitor();
    void stopProcessMonitor();
    void clearStars();
    void clearResults();
    void loadOptionsProfile();
    void settingJustChanged();

    void loadIndexFilesList();
    void setSubframe();

    //These are the functions that run when the bottom buttons are clicked
    void sextractButtonClicked();
    void solveButtonClicked();
    void sextractImage();
    void solveImage();

    void abort();

    //These functions are for loading and displaying the image
    bool imageLoad();
    bool loadFits();
    bool loadOtherFormat();
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

    //These functions handle the star table
    void displayTable();
    void sortStars();
    void starClickedInTable();
    void updateStarTableFromList();
    void updateHiddenStarTableColumns();
    void saveStarTable();

    void mouseMovedOverImage(QPoint location);
    QString getValue(int x, int y);
    void mouseClickedInImage(QPoint location);
    void mousePressedInImage(QPoint location);
    QRect getStarSizeInImage(FITSImage::Star star, bool &accurate);

    //This function is for loading and parsing the options
    bool getSolverOptionsFromFITS();

    //These functions handle the settings for the Sextractors and Solvers
    SSolver::Parameters getSettingsFromUI();
    void sendSettingsToUI(SSolver::Parameters a);
    void setupExternalSextractorSolverIfNeeded();
    void setupStellarSolverParameters();

    //These functions get called when the sextractor or solver finishes
    bool sextractorComplete();
    bool solverComplete();
    bool loadWCSComplete();

    //These functions handle the solution table
    void addSolutionToTable(FITSImage::Solution solution);
    void updateHiddenResultsTableColumns();
    void saveResultsTable();

    //These functions save and load the settings of the program.
    void saveOptionsProfiles();
    void loadOptionsProfiles();
    void closeEvent(QCloseEvent *event);
signals:
    void readyForStarTable();

};

#endif // MAINWINDOW_H
