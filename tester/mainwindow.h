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
#include "externalextractorsolver.h"
#include "onlinesolver.h"
#include "testerutils/fileio.h"

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
#include <QTableWidget>

//CFitsio Includes
#include "longnam.h"
#include "fitsio.h"

//KStars related includes
#include "testerutils/stretch.h"
#include "math.h"
#include "testerutils/dms.h"
#include "testerutils/bayer.h"

//Astrometry.net includes
extern "C"{
#include "tic.h"
#include "os-features.h"
//#include "fileutils.h" //# Modified by Robert Lancaster for the StellarSolver Internal Library, removed includes
#include "ioutils.h"
#include "bl.h"
#include "an-bool.h"
#include "solver.h"
#include "fitsioutils.h"
#include "astrometry/blind.h"
#include "log.h"
#include "engine.h"
#include "gslutils.h"
#include "anqfits.h"
#include "ioutils.h"
#include "fitsioutils.h"
#include "permutedsort.h"
#include "fitstable.h"
//#include "sip_qfits.h" //# Modified by Robert Lancaster for the StellarSolver Internal Library, removed includes
#include "sip-utils.h"
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

    StellarSolver stellarSolver;
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
    bool showExtractorParams = false;
    bool showAstrometryParams = false;
    bool showSolutionDetails = true;
    bool settingSubframe = false;

    bool useSubframe = false;
    QRect subframe;

    SSolver::ProcessType processType;
    SSolver::ExtractorType m_ExtractorType;
    SSolver::SolverType solverType;
    int profileSelection;

    //This allows for averaging over multiple trials
    int currentTrial = 0;
    int numberOfTrials = 0;
    double totalTime = 0;

    //Data about the image
    bool imageLoaded = false;
    FITSImage::Statistic stats;
    QList<fileio::Record> m_HeaderRecords;
    QImage rawImage;
    QImage scaledImage;
    int currentWidth;
    int currentHeight;
    double currentZoom;
    int sampling = 2;
    /// Generic data image buffer
    uint8_t *m_ImageBuffer { nullptr };
    /// Above buffer size in bytes
    uint32_t m_ImageBufferSize { 0 };

    QElapsedTimer processTimer;
    QTimer timerMonitor;
    double elapsed;

    void setupResultsTable();
    void clearAstrometrySettings();
    void addExtractionToTable();
    FITSImage::Solution lastSolution;
    QStringList indexFileList;
    QString lastIndexNumber = "4";
    QString lastHealpix = "";

    QDialog *convInspector = nullptr;
    QTableWidget *convTable = nullptr;

public slots:

    bool prepareForProcesses();
    void logOutput(QString text);
    void toggleLogDisplay();
    void toggleFullScreen();
    void helpPopup();
    void startProcessMonitor();
    void stopProcessMonitor();
    void clearStars();
    void clearResults();
    void loadOptionsProfile();
    void settingJustChanged();

    void loadIndexFilesListInFolder();
    void loadIndexFilesToUse();
    void setSubframe();

    //These are the functions that run when the bottom buttons are clicked
    void extractButtonClicked();
    void solveButtonClicked();
    void extractImage();
    void solveImage();

    void abort();

    //These functions are for loading and displaying the image
    bool imageLoad();
    bool imageSave();
    void clearImageBuffers();

    void zoomIn();
    void zoomOut();
    void panLeft();
    void panRight();
    void panUp();
    void panDown();
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
    QString getValue(int x, int y, int channel);
    void mouseClickedInImage(QPoint location);
    void mousePressedInImage(QPoint location);
    QRect getStarSizeInImage(FITSImage::Star star, bool &accurate);

    void reloadConvTable();

    //These functions handle the settings for the Star Extractors and Solvers
    SSolver::Parameters getSettingsFromUI();
    void sendSettingsToUI(SSolver::Parameters a);
    void setupExternalExtractorSolverIfNeeded();
    void setupStellarSolverParameters();

    //These functions get called when the star extractor or solver finishes
    bool extractorComplete();
    bool solverComplete();

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
