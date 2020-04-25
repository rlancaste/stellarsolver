/*  MainWindow for SexySolver Tester Application, developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QUuid>
#include <QDebug>
#include <QImageReader>
#include <QTableWidgetItem>
#include <QPainter>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#ifndef _MSC_VER
#include <sys/time.h>
#include <libgen.h>
#include <getopt.h>
#include <dirent.h>
#endif

#include <time.h>

#include <assert.h>

#include <QThread>
#include <QtConcurrent>
#include <QToolTip>
#include <QtGlobal>
#include "version.h"

MainWindow::MainWindow() :
    QMainWindow(),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);


   this->show();

    //The Options at the top of the Window
    connect(ui->ImageLoad,&QAbstractButton::clicked, this, &MainWindow::imageLoad );
    ui->ImageLoad->setToolTip("Loads an Image into the Viewer");
    connect(ui->zoomIn,&QAbstractButton::clicked, this, &MainWindow::zoomIn );
    ui->zoomIn->setToolTip("Zooms In on the Image");
    connect(ui->zoomOut,&QAbstractButton::clicked, this, &MainWindow::zoomOut );
    ui->zoomOut->setToolTip("Zooms Out on the Image");
    connect(ui->AutoScale,&QAbstractButton::clicked, this, &MainWindow::autoScale );
    ui->AutoScale->setToolTip("Rescales the image based on the available space");

    //The Options at the bottom of the Window
    connect(ui->SextractStars,&QAbstractButton::clicked, this, &MainWindow::sextractImage );
    ui->SextractStars->setToolTip("Sextract the stars in the image using the chosen method and load them into the star table");
    ui->sextractorType->setToolTip("Lets you choose the Internal SexySolver SEP or external Sextractor program");
    connect(ui->SolveImage,&QAbstractButton::clicked, this, &MainWindow::solveImage );
    ui->SolveImage->setToolTip("Solves the image using the method chosen in the dropdown box to the right");
    ui->solverType->setToolTip("Lets you choose how to solve the image");

    connect(ui->Abort,&QAbstractButton::clicked, this, &MainWindow::abort );
    ui->Abort->setToolTip("Aborts the current process if one is running.");
    connect(ui->reset, &QPushButton::clicked, this, &MainWindow::resetOptionsToDefaults);
    ui->reset->setToolTip("Resets all the options in the left option pane to defalt values");
    connect(ui->Clear,&QAbstractButton::clicked, this, &MainWindow::clearAll );
    ui->Clear->setToolTip("Clears the stars from the image, the star table, the results table, and the log at the bottom");
    connect(ui->exportTable,&QAbstractButton::clicked, this, &MainWindow::saveResultsTable);
    ui->exportTable->setToolTip("Exports the log of processes executed during this session to a CSV file for further analysis");

    //Behaviors for the StarTable
    ui->starTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(ui->starTable,&QTableWidget::itemSelectionChanged, this, &MainWindow::starClickedInTable);
    ui->starTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    //Behaviors for the Mouse over the Image to interact with the StartList and the UI
    connect(ui->Image,&ImageLabel::mouseHovered,this, &MainWindow::mouseMovedOverImage);
    connect(ui->Image,&ImageLabel::mouseClicked,this, &MainWindow::mouseClickedOnStar);

    //Hides the panels into the sides and bottom
    ui->splitter->setSizes(QList<int>() << ui->splitter->height() << 0 );
    ui->splitter_2->setSizes(QList<int>() << 100 << ui->splitter_2->width() / 2  << 0 );

    //Settings for the External Sextractor and Solver
    ui->configFilePath->setToolTip("The path to the Astrometry.cfg file used by astrometry.net for configuration.");
    ui->sextractorPath->setToolTip("The path to the external Sextractor executable");
    ui->solverPath->setToolTip("The path to the external Astrometry.net solve-field executable");
    ui->astapPath->setToolTip("The path to the external ASTAP executable");
    ui->basePath->setToolTip("The base path where sextractor and astrometry temporary files are saved on your computer");
    ui->wcsPath->setToolTip("The path to wcsinfo for the external Astrometry.net");
    ui->cleanupTemp->setToolTip("This option allows the program to clean up temporary files created when running various processes");
    ui->generateAstrometryConfig->setToolTip("Determines whether to generate an astrometry.cfg file based on the options in the options panel or to use the external config file above.");

    //SexySolver Tester Options
    ui->calculateHFR->setToolTip("This is a Sextractor option, when checked it will calculate the HFR of the stars.");
    connect(ui->showStars,&QAbstractButton::clicked, this, &MainWindow::updateImage );
    ui->showStars->setToolTip("This toggles the stars circles on and off in the image");
    connect(ui->starOptions,QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateImage);
    ui->starOptions->setToolTip("This allows you to select different types of star circles to put on the stars.  Warning, some require HFR to have been calculated first.");
    connect(ui->showFluxInfo, &QCheckBox::stateChanged, this, [this](){ showFluxInfo = ui->showFluxInfo->isChecked(); updateHiddenStarTableColumns(); });
    ui->showFluxInfo->setToolTip("This toggles whether to show or hide the HFR, peak, Flux columns in the star table after Sextraction.");
    connect(ui->showStarShapeInfo, &QCheckBox::stateChanged, this, [this](){ showStarShapeInfo = ui->showStarShapeInfo->isChecked(); updateHiddenStarTableColumns();});
    ui->showStarShapeInfo->setToolTip("This toggles whether to show or hide the information about each star's semi-major axis, semi-minor axis, and orientation in the star table after sextraction.");
    connect(ui->showSextractorParams, &QCheckBox::stateChanged, this, [this](){ showSextractorParams = ui->showSextractorParams->isChecked(); updateHiddenResultsTableColumns(); });
    ui->showSextractorParams->setToolTip("This toggles whether to show or hide the Sextractor Settings in the Results table at the bottom");
    connect(ui->showAstrometryParams, &QCheckBox::stateChanged, this, [this](){ showAstrometryParams = ui->showAstrometryParams->isChecked(); updateHiddenResultsTableColumns(); });
    ui->showAstrometryParams->setToolTip("This toggles whether to show or hide the Astrometry Settings in the Results table at the bottom");
    connect(ui->showSolutionDetails, &QCheckBox::stateChanged, this, [this](){ showSolutionDetails = ui->showSolutionDetails->isChecked(); updateHiddenResultsTableColumns(); });
    ui->showSolutionDetails->setToolTip("This toggles whether to show or hide the Solution Details in the Results table at the bottom");
    connect(ui->setPathsAutomatically, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int num){
        ExternalSextractorSolver extTemp(stats, m_ImageBuffer, this);

        switch(num)
        {
            case 0:
                extTemp.setLinuxDefaultPaths();
                break;
            case 1:
                extTemp.setLinuxInternalPaths();
                break;
            case 2:
                extTemp.setMacHomebrewPaths();
                break;
            case 3:
                extTemp.setMacInternalPaths();
                break;
            case 4:
                extTemp.setWinANSVRPaths();
                break;
            case 5:
                extTemp.setWinCygwinPaths();
                break;
            default:
                extTemp.setLinuxDefaultPaths();
                break;
        }

        ui->sextractorPath->setText(extTemp.sextractorBinaryPath);
        ui->configFilePath->setText(extTemp.confPath);
        ui->solverPath->setText(extTemp.solverPath);
        ui->astapPath->setText(extTemp.astapBinaryPath);
        ui->wcsPath->setText(extTemp.wcsPath);
    });
    ui->setPathsAutomatically->setToolTip("This allows you to select the default values of typical configurations of paths to external files/programs on different systems from a dropdown");

    //Sextractor Settings

    ui->apertureShape->setToolTip("This selects whether to instruct the Sextractor to use Ellipses or Circles for flux calculations");
    ui->kron_fact->setToolTip("This sets the Kron Factor for use with the kron radius for flux calculations.");
    ui->subpix->setToolTip("The subpix setting.  The instructions say to make it 5");
    ui->detect_thresh->setToolTip("Currently disabled parameter for detection/analysis threshold.  We are using a different method.");
    ui->r_min->setToolTip("The minimum radius for stars for flux calculations.");
    //no inflags???;
    ui->magzero->setToolTip("This is the 'zero' magnitude used for settting the magnitude scale for the stars in the image during sextraction.");
    ui->minarea->setToolTip("This is the minimum area in pixels for a star detection, smaller stars are ignored.");
    ui->deblend_thresh->setToolTip("The number of thresholds the intensity range is divided up into");
    ui->deblend_contrast->setToolTip("The percentage of flux a separate peak must # have to be considered a separate object");

    ui->cleanCheckBox->setToolTip("Attempts to 'clean' the image to remove artifacts caused by bright objects");
    ui->clean_param->setToolTip("The cleaning parameter, not sure what it does.");

    //This generates an array that can be used as a convFilter based on the desired FWHM
    ui->fwhm->setToolTip("A function that I made that creates a convolution filter based upon the desired FWHM for better star detection of that star size and shape");

    //Star Filter Settings
    connect(ui->resortQT, &QCheckBox::stateChanged, this, [this](){ ui->resort->setChecked(ui->resortQT->isChecked());});
    ui->resortQT->setToolTip("This resorts the stars based on magnitude.  It MUST be checked for the next couple of filters to be enabled.");
    ui->maxEllipse->setToolTip("Stars are typically round, this filter divides stars' semi major and minor axes and rejects stars with distorted shapes greater than this number (1 is perfectly round)");
    ui->brightestPercent->setToolTip("Removes the brightest % of stars from the image");
    ui->dimmestPercent->setToolTip("Removes the dimmest % of stars from the image");
    ui->saturationLimit->setToolTip("Removes stars above a certain % of the saturation limit of an image of this data type");

    //Astrometry Settings
    ui->inParallel->setToolTip("Loads the Astrometry index files in parallel.  This can speed it up, but uses more resources");
    ui->solverTimeLimit->setToolTip("This is the maximum time the Astrometry.net solver should spend on the image before giving up");
    ui->minWidth->setToolTip("Sets a the minimum degree limit in the scales for Astrometry to search if the scale parameter isn't set");
    ui->maxWidth->setToolTip("Sets a the maximum degree limit in the scales for Astrometry to search if the scale parameter isn't set");

    connect(ui->resort, &QCheckBox::stateChanged, this, [this](){ ui->resortQT->setChecked(ui->resort->isChecked()); });
    ui->downsample->setToolTip("This downsamples or bins the image to hopefully make it solve faster.");
    ui->resort->setToolTip("This resorts the stars based on magnitude. It usually makes it solve faster.");
    ui->use_scale->setToolTip("Whether or not to use the estimated image scale below to try to speed up the solve");
    ui->scale_low->setToolTip("The minimum size for the estimated image scale");
    ui->scale_high->setToolTip("The maximum size for the estimated image scale");
    ui->units->setToolTip("The units for the estimated image scale");

    ui->use_position->setToolTip("Whether or not to use the estimated position below to try to speed up the solve");
    ui->ra->setToolTip("The estimated RA of the object in decimal form in hours not degrees");
    ui->dec->setToolTip("The estimated DEC of the object in decimal form in degrees");
    ui->radius->setToolTip("The search radius (degrees) of a circle centered on this position for astrometry.net to search for solutions");

    ui->oddsToKeep->setToolTip("The Astrometry oddsToKeep Parameter.  This may need to be changed or removed");
    ui->oddsToSolve->setToolTip("The Astrometry oddsToSolve Parameter.  This may need to be changed or removed");
    ui->oddsToTune->setToolTip("The Astrometry oddsToTune Parameter.  This may need to be changed or removed");

    ui->logToFile->setToolTip("Whether or not the internal solver should log its output to a file for analysis");
    ui->logLevel->setToolTip("The verbosity level of the log to be saved for the internal solver.");

    connect(ui->indexFolderPaths, &QComboBox::currentTextChanged, this, [this](QString item){ loadIndexFilesList(); });
    ui->indexFolderPaths->setToolTip("The paths on your compute to search for index files.  To add another, just start typing in the box.  To select one to look at, use the drop down.");
    connect(ui->removeIndexPath, &QPushButton::clicked, this, [this](){ ui->indexFolderPaths->removeItem( ui->indexFolderPaths->currentIndex()); });
    ui->removeIndexPath->setToolTip("Removes the selected path in the index folder paths dropdown so that it won't get passed to the solver");
    connect(ui->addIndexPath, &QPushButton::clicked, this, [this](){
        QString dir = QFileDialog::getExistingDirectory(this, "Load Index File Directory",
                                                        QDir::homePath(),
                                                        QFileDialog::ShowDirsOnly
                                                        | QFileDialog::DontResolveSymlinks);
        if (dir.isEmpty())
            return;
        ui->indexFolderPaths->addItem( dir );
        ui->indexFolderPaths->setCurrentIndex(ui->indexFolderPaths->count() - 1);
    });
    ui->addIndexPath->setToolTip("Adds a path the user selects to the list of index folder paths");

    resetOptionsToDefaults();

    setupResultsTable();
    ui->resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->resultsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;

    setWindowTitle(QString("SexySolver Library Tester %1, build: %2").arg(SexySolver_VERSION).arg(SexySolver_BUILD_TS));

    ui->progressBar->setTextVisible(false);
    timerMonitor.setInterval(1000); //1 sec intervals
    connect(&timerMonitor, &QTimer::timeout, this, [this](){
        ui->status->setText(QString("Processing: %1 s").arg((int)processTimer.elapsed()/1000) + 1);
    });

    setWindowIcon(QIcon(":/SexySolverIcon.png"));
}

void MainWindow::resetOptionsToDefaults()
{

#if defined(Q_OS_OSX)
    if(QFile("/usr/local/bin/solve-field").exists())
        ui->setPathsAutomatically->setCurrentIndex(2);
    else
        ui->setPathsAutomatically->setCurrentIndex(3);
#elif defined(Q_OS_LINUX)
    ui->setPathsAutomatically->setCurrentIndex(0);
#else //Windows
    ui->setPathsAutomatically->setCurrentIndex(4);
#endif

    ExternalSextractorSolver extTemp(stats, m_ImageBuffer, this);

        ui->sextractorPath->setText(extTemp.sextractorBinaryPath);
        ui->configFilePath->setText(extTemp.confPath);
        ui->solverPath->setText(extTemp.solverPath);
        ui->astapPath->setText(extTemp.astapBinaryPath);
        ui->wcsPath->setText(extTemp.wcsPath);
        ui->cleanupTemp->setChecked(extTemp.cleanupTemporaryFiles);
        ui->generateAstrometryConfig->setChecked(extTemp.autoGenerateAstroConfig);

    SexySolver temp(stats, m_ImageBuffer, this);

    //Basic Settings
    QString basePath = temp.basePath;

        ui->showStars->setChecked(true);
        ui->basePath->setText(basePath);

    //Sextractor Settings

        ui->calculateHFR->setChecked(temp.calculateHFR);
        ui->apertureShape->setCurrentIndex(temp.apertureShape);
        ui->kron_fact->setText(QString::number(temp.kron_fact));
        ui->subpix->setText(QString::number(temp.subpix));
        ui->r_min->setText(QString::number(temp.r_min));

        ui->magzero->setText(QString::number(temp.magzero));
        ui->minarea->setText(QString::number(temp.minarea));
        ui->deblend_thresh->setText(QString::number(temp.deblend_thresh));
        ui->deblend_contrast->setText(QString::number(temp.deblend_contrast));
        ui->cleanCheckBox->setChecked(temp.clean == 1);
        ui->clean_param->setText(QString::number(temp.clean_param));
        ui->fwhm->setText(QString::number(temp.fwhm));

    //Star Filter Settings
        ui->maxEllipse->setText(QString::number(temp.maxEllipse));
        ui->brightestPercent->setText(QString::number(temp.removeBrightest));
        ui->dimmestPercent->setText(QString::number(temp.removeDimmest));
        ui->saturationLimit->setText(QString::number(temp.saturationLimit));

    //Astrometry Settings

        ui->downsample->setValue(temp.downsample);
        ui->inParallel->setChecked(temp.inParallel);
        ui->solverTimeLimit->setText(QString::number(temp.solverTimeLimit));
        ui->minWidth->setText(QString::number(temp.minwidth));
        ui->maxWidth->setText(QString::number(temp.maxwidth));
        ui->radius->setText(QString::number(temp.search_radius));

    clearAstrometrySettings(); //Resets the Position and Scale settings

        ui->resort->setChecked(temp.resort);

    //Astrometry Log Ratio Settings

        ui->oddsToKeep->setText(QString::number(temp.logratio_tokeep));
        ui->oddsToSolve->setText(QString::number(temp.logratio_tosolve));
        ui->oddsToTune->setText(QString::number(temp.logratio_totune));

    //Astrometry Logging Settings

        ui->logToFile->setChecked(temp.logToFile);
        ui->logLevel->setCurrentIndex(temp.logLevel);

    //Astrometry Index File Paths to Search
    ui->indexFolderPaths->clear();

    QStringList indexFilePaths = temp.getDefaultIndexFolderPaths();

    foreach(QString pathName, indexFilePaths)
    {
        ui->indexFolderPaths->addItem(pathName);
    }

    loadIndexFilesList();

}

MainWindow::~MainWindow()
{
    delete ui;
}

//This method clears the tables and displays when the user requests it.
void MainWindow::clearAll()
{
    ui->logDisplay->clear();
    ui->starTable->clearContents();
    ui->resultsTable->clearContents();
    ui->resultsTable->setRowCount(0);
    stars.clear();
    updateImage();
}

//These methods are for the logging of information to the textfield at the bottom of the window.
void MainWindow::logOutput(QString text)
{
     ui->logDisplay->append(text);
     ui->logDisplay->show();
}

void MainWindow::startProcessMonitor()
{
    ui->status->setText("Processing");
    ui->progressBar->setRange(0,0);
    timerMonitor.start();
    processTimer.start();
}

void MainWindow::stopProcessMonitor()
{
    elapsed = processTimer.elapsed()/1000.0;
    timerMonitor.stop();
    ui->progressBar->setRange(0,10);
    ui->status->setText("No Process Running");
}

//I wrote this method because we really want to do this before all 4 processes
//It was getting repetitive.
bool MainWindow::prepareForProcesses()
{
    if(ui->splitter->sizes().last() < 10)
         ui->splitter->setSizes(QList<int>() << ui->splitter->height() /2 << 100 );
    ui->logDisplay->verticalScrollBar()->setValue(ui->logDisplay->verticalScrollBar()->maximum());

    if(!imageLoaded)
    {
        logOutput("Please Load an Image First");
        return false;
    }
    bool running = false;
    if(!sexySolver.isNull() && sexySolver->isRunning())
        running = true;
    if(!extSolver.isNull() && extSolver->isRunning())
        running = true;
    if(running)
    {
        int r = QMessageBox::question(this, "Abort?", "A Process is currently running. Abort it?");
        if (r == QMessageBox::No)
            return false;
        sexySolver->abort();
    }
    sexySolver.clear();
    extSolver.clear();
    return true;
}

//I wrote this method to display the table after sextraction has occured.
void MainWindow::displayTable()
{
    sortStars();
    updateStarTableFromList();

    if(ui->splitter_2->sizes().last() < 10)
        ui->splitter_2->setSizes(QList<int>() << ui->optionsBox->width() << ui->splitter_2->width() / 2 << 200 );
    updateImage();
}

//This method is intended to load a list of the index files to display as feedback to the user.
void MainWindow::loadIndexFilesList()
{
    QString currentPath = ui->indexFolderPaths->currentText();
    QDir dir(currentPath);
    ui->indexFiles->clear();
    if(dir.exists())
    {
        dir.setNameFilters(QStringList()<<"*.fits"<<"*.fit");
        if(dir.entryList().count() == 0)
            ui->indexFiles->addItem("No index files in Folder");
        ui->indexFiles->addItems(dir.entryList());
    }
    else
        ui->indexFiles->addItem("Invalid Folder");
}



//The following methods are meant for starting the sextractor and image solving.
//The methods run when the buttons are clicked.  They call the methods inside SexySolver and ExternalSextractorSovler

//This method responds when the user clicks the Sextract Button
bool MainWindow::sextractImage()
{
    QString sextractorBinaryPath = ui->sextractorPath->text();

    if(!prepareForProcesses())
        return false;

    if(ui->sextractorType->currentIndex() == 0) //Internal
        return sextractInternally();
    else
    {
        #ifdef _WIN32
        logOutput("An External Sextractor is not easily installed on Windows, try the Internal Sextractor please.");
        return false;
        #endif

        if(!QFileInfo(sextractorBinaryPath).exists())
        {
            logOutput("There is no sextractor at " + sextractorBinaryPath + ", Aborting");
            return false;
        }

        sextractExternally();
        return true;
    }
}

//This method runs when the user clicks the Sextract and Solve buttton
bool MainWindow::solveImage()
{
    QString sextractorBinaryPath = ui->sextractorPath->text();
    QString solverPath = ui->solverPath->text();
    QString astapPath = ui->astapPath->text();

    if(!prepareForProcesses())
        return false;

    //Select the type of Solve
    switch(ui->solverType->currentIndex())
    {
        case 0: //Internal SexySolver
            return sextractAndSolveInternally();
        break;

        case 1: //External Sextractor and Solver

            #ifdef _WIN32
                logOutput("Sextractor is not easily installed on windows. Please select the Internal Sextractor and External Solver.");
            #endif

                if(!QFileInfo(sextractorBinaryPath).exists())
                {
                    logOutput("There is no sextractor at " + sextractorBinaryPath + ", Aborting");
                    return false;
                }

                if(!QFileInfo(solverPath).exists())
                {
                    logOutput("There is no astrometry solver at " + solverPath + ", Aborting");
                    return false;
                }

            sextractAndSolveExternally();
            return true;

        break;

        case 2: //Int. SEP Ext. Solver

                if(!QFileInfo(solverPath).exists())
                {
                    logOutput("There is no astrometry solver at " + solverPath + ", Aborting");
                    return false;
                }

            return SEPAndSolveExternally();

        break;

        case 3: //Classic Astrometry.net

                if(!QFileInfo(solverPath).exists())
                {
                    logOutput("There is no astrometry solver at " + solverPath + ", Aborting");
                    return false;
                }

            return classicSolve();

        break;

        case 4: //ASTAP Solver
            if(!QFileInfo(astapPath).exists())
            {
                logOutput("There is no ASTAP solver at " + astapPath + ", Aborting");
                return false;
            }
             astapSolve();
             return true;
        break;

    }
}

//This method runs when the user selects the Classic Solve Option
//It is meant to run the traditional solve KStars used to do using python and astrometry.net
bool MainWindow::classicSolve()
{
    if(!prepareForProcesses())
        return false;

    setupExternalSextractorSolver();
    setSolverSettings(); 

    #ifdef _WIN32
        if(ui->inParallel->isChecked())
        {
            logOutput("The external ANSVR solver on windows does not handle the inparallel option well, disabling it for this run.");
            sexySolver.inParallel = false;
        }
    #endif

    connect(sexySolver, &SexySolver::finished, this, &MainWindow::solverComplete);

    startProcessMonitor();
    extSolver->classicSolve();

    return true;
}

//This method runs when the user selects the Classic Solve Option
//It is meant to run the traditional solve KStars used to do using python and astrometry.net
bool MainWindow::astapSolve()
{
    if(!prepareForProcesses())
        return false;

    setupExternalSextractorSolver();
    setSolverSettings();

    connect(sexySolver, &SexySolver::finished, this, &MainWindow::solverComplete);

    startProcessMonitor();
    extSolver->astapSolve();

    return true;
}

//I wrote this method to call the external sextractor program.
//It then will load the results into a table to the right of the image
//It times the entire process and prints out how long it took
bool MainWindow::sextractExternally()
{
    setupExternalSextractorSolver();

    setSextractorSettings();

    connect(sexySolver, &SexySolver::finished, this, &MainWindow::sextractorComplete);

    startProcessMonitor();
    extSolver->sextract();

    return true;
}

//I wrote this method to call the external sextractor and solver program.
//It times the entire process and prints out how long it took
bool MainWindow::sextractAndSolveExternally()
{
    setupExternalSextractorSolver();

    setSextractorSettings();
    setSolverSettings();

    connect(sexySolver, &SexySolver::finished, this, &MainWindow::solverComplete);

    startProcessMonitor();
    extSolver->sextractAndSolve();
    return true;
}

//I wrote this method to call the internal sextractor and external solver program.
//It times the entire process and prints out how long it took
bool MainWindow::SEPAndSolveExternally()
{
    setupExternalSextractorSolver();

    setSextractorSettings();
    setSolverSettings();

    #ifdef _WIN32
        if(ui->inParallel->isChecked())
        {
            logOutput("The external ANSVR solver on windows does not handle the inparallel option well, disabling it for this run.");
            sexySolver.inParallel = false;
        }
    #endif

    connect(sexySolver, &SexySolver::finished, this, &MainWindow::solverComplete);

    startProcessMonitor();
    extSolver->SEPAndSolve();
    return true;
}

//I wrote this method to call the internal sextractor in the SexySolver.
//It runs in a separate thread so that it is nonblocking
//It then will load the results into a table to the right of the image
//It times the entire process and prints out how long it took
bool MainWindow::sextractInternally()
{
    setupInternalSexySolver();

    setSextractorSettings();

    connect(sexySolver, &SexySolver::finished, this, &MainWindow::sextractorComplete);

    startProcessMonitor();
    sexySolver->sextract();
    return true;
}

//I wrote this method to start the internal solver in the SexySolver.
//It runs in a separate thread so that it is nonblocking
//It times the entire process and prints out how long it took
bool MainWindow::sextractAndSolveInternally()
{
      setupInternalSexySolver();

      setSextractorSettings();
      setSolverSettings();

      connect(sexySolver, &SexySolver::finished, this, &MainWindow::solverComplete);

      startProcessMonitor();
      sexySolver->sextractAndSolve();
      return true;
}

//This sets up the External Sextractor and Solver and sets settings specific to them
void MainWindow::setupExternalSextractorSolver()
{
    //Creates the External Sextractor/Solver Object
    extSolver = new ExternalSextractorSolver(stats, m_ImageBuffer, this);
    sexySolver = extSolver;

    //Connects the External Sextractor/Solver Object for logging
    connect(sexySolver, &SexySolver::logNeedsUpdating, this, &MainWindow::logOutput);

    //Sets the file settings and other items specific to the external solver programs
    extSolver->fileToProcess = fileToProcess;
    extSolver->basePath = ui->basePath->text();
    extSolver->sextractorBinaryPath = ui->sextractorPath->text();
    extSolver->confPath = ui->configFilePath->text();
    extSolver->solverPath = ui->solverPath->text();
    extSolver->astapBinaryPath = ui->astapPath->text();
    extSolver->wcsPath = ui->wcsPath->text();
    extSolver->cleanupTemporaryFiles = ui->cleanupTemp->isChecked();
    extSolver->autoGenerateAstroConfig = ui->generateAstrometryConfig->isChecked();

    extSolver->command = ui->solverType->currentText();
}

//This sets up the Internal SexySolver and sets settings specific to it
void MainWindow::setupInternalSexySolver()
{
    //Creates the SexySolver
    sexySolver = new SexySolver(stats ,m_ImageBuffer, this);

    //Connects the SexySolver for logging
    connect(sexySolver, &SexySolver::logNeedsUpdating, this, &MainWindow::logOutput, Qt::QueuedConnection);

    sexySolver->command = ui->solverType->currentText();

}

//This sets all the settings for either the internal or external sextractor
//based on the requested settings in the mainwindow interface.
//If you are implementing the SexySolver Library in your progra, you may choose to change some or all of these settings or use the defaults.
void MainWindow::setSextractorSettings()
{ 
    //These are to pass the parameters to the internal sextractor
    sexySolver->calculateHFR = ui->calculateHFR->isChecked();
    sexySolver->apertureShape = (Shape) ui->apertureShape->currentIndex();
    sexySolver->kron_fact = ui->kron_fact->text().toDouble();
    sexySolver->subpix = ui->subpix->text().toInt() ;
    sexySolver->r_min = ui->r_min->text().toFloat();
    //sexySolver->inflags
    sexySolver->magzero = ui->magzero->text().toFloat();
    sexySolver->minarea = ui->minarea->text().toFloat();
    sexySolver->deblend_thresh = ui->deblend_thresh->text().toInt();
    sexySolver->deblend_contrast = ui->deblend_contrast->text().toFloat();
    sexySolver->clean = (ui->cleanCheckBox->isChecked()) ? 1 : 0;
    sexySolver->clean_param = ui->clean_param->text().toDouble();
    sexySolver->createConvFilterFromFWHM(ui->fwhm->text().toDouble());

    //Star Filter Settings
    sexySolver->resort = ui->resort->isChecked();
    sexySolver->removeBrightest = ui->brightestPercent->text().toDouble();;
    sexySolver->removeDimmest = ui->dimmestPercent->text().toDouble();;
    sexySolver->maxEllipse = ui->maxEllipse->text().toDouble();;
    sexySolver->saturationLimit = ui->saturationLimit->text().toDouble();;
}

//This sets all the settings for either the internal or external astrometry.net solver
//based on the requested settings in the mainwindow interface.
//If you are implementing the SexySolver Library in your progra, you may choose to change some or all of these settings or use the defaults.
void MainWindow::setSolverSettings()
{
    //Settings that usually get set by the config file
    QStringList indexFilePaths;
    for(int i = 0; i < ui->indexFolderPaths->count(); i++)
    {
        indexFilePaths << ui->indexFolderPaths->itemText(i);
    }
    sexySolver->setIndexFolderPaths(indexFilePaths);
    sexySolver->maxwidth = ui->maxWidth->text().toDouble();
    sexySolver->minwidth = ui->minWidth->text().toDouble();
    sexySolver->inParallel = ui->inParallel->isChecked();
    sexySolver->solverTimeLimit = ui->solverTimeLimit->text().toDouble();

    sexySolver->resort = ui->resort->isChecked();
    sexySolver->downsample = ui->downsample->value();

    //Setting the scale settings
    if(ui->use_scale->isChecked())
        sexySolver->setSearchScale(ui->scale_low->text().toDouble(), ui->scale_high->text().toDouble(), ui->units->currentText());

    //Setting the initial search location settings
    if(ui->use_position->isChecked())
        sexySolver->setSearchPosition(ui->ra->text().toDouble(), ui->dec->text().toDouble(), ui->radius->text().toDouble());

    //Setting the settings to know when to stop or keep searching for solutions
    sexySolver->logratio_tokeep = ui->oddsToKeep->text().toDouble();
    sexySolver->logratio_totune = ui->oddsToTune->text().toDouble();
    sexySolver->logratio_tosolve = ui->oddsToSolve->text().toDouble();

    //Setting the logging settings
    sexySolver->logToFile = ui->logToFile->isChecked();
    sexySolver->logLevel = ui->logLevel->currentIndex();
}

//This runs when the sextractor is complete.
//It reports the time taken, prints a message, loads the sextraction stars to the startable, and adds the sextraction stats to the results table.
bool MainWindow::sextractorComplete(int error)
{
    stopProcessMonitor();
    if(error == 0)
    {
        stars = sexySolver->getStarTable();
        displayTable();
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString("Successfully sextracted %1 stars.").arg(stars.size()));
        logOutput(QString("Sextraction took a total of: %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        ui->resultsTable->insertRow(ui->resultsTable->rowCount());
        addSextractionToTable();
        QTimer::singleShot(100 , [this](){ui->resultsTable->verticalScrollBar()->setValue(ui->resultsTable->verticalScrollBar()->maximum());});
        return true;
    }
    else
    {
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString("Sextractor failed after %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        return false;
    }
}

//This runs when the solver is complete.  It reports the time taken, prints a message, and adds the solution to the results table.
bool MainWindow::solverComplete(int error)
{
    stopProcessMonitor();
    if(error == 0)
    {
        ui->progressBar->setRange(0,10);
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString(sexySolver->command + " took a total of: %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        ui->resultsTable->insertRow(ui->resultsTable->rowCount());
        addSextractionToTable();
        addSolutionToTable(sexySolver->solution);
        QTimer::singleShot(100 , [this](){ui->resultsTable->verticalScrollBar()->setValue(ui->resultsTable->verticalScrollBar()->maximum());});
        return true;
    }
    else
    {
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString(sexySolver->command + "failed after %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        return false;
    }
}

//This method will attempt to abort the sextractor, sovler, and any other processes currently being run, no matter which type
void MainWindow::abort()
{
    if(!sexySolver.isNull() && sexySolver->isRunning())
    {
        sexySolver->abort();
        logOutput("Aborting Internal Process. . .");
    }
    else if(!extSolver.isNull() && extSolver->isRunning())
    {
        extSolver->abort();
        logOutput("Aborting External Process. . .");
    }
    else
        logOutput("No Processes Running.");

}

//This method is meant to clear out the Astrometry settings that should change with each image
//They might be loaded from a fits file, but if the file doesn't contain them, they should be cleared.
void MainWindow::clearAstrometrySettings()
{
    //Note that due to connections, it automatically sets the variable as well.
    ui->use_scale->setChecked(false);
    ui->scale_low->setText("");
    ui->scale_high->setText("");
    ui->use_position->setChecked(false);
    ui->ra->setText("");
    ui->dec->setText("");
}





//The following methods deal with the loading and displaying of the image

//I wrote this method to select the file name for the image and call the load methods below to load it
bool MainWindow::imageLoad()
{
    QString fileURL = QFileDialog::getOpenFileName(nullptr, "Load Image", dirPath,
                                               "Images (*.fits *.fit *.bmp *.gif *.jpg *.jpeg *.tif *.tiff)");
    if (fileURL.isEmpty())
        return false;
    QFileInfo fileInfo(fileURL);
    if(!fileInfo.exists())
        return false;
    QString newFileURL=ui->basePath->text() + "/" + fileInfo.fileName().remove(" ");
    QFile::copy(fileURL, newFileURL);
    QFileInfo newFileInfo(newFileURL);
    dirPath = fileInfo.absolutePath();
    fileToProcess = newFileURL;

    clearAstrometrySettings();

    bool loadSuccess;
    if(newFileInfo.suffix()=="fits"||newFileInfo.suffix()=="fit")
        loadSuccess = loadFits();
    else
        loadSuccess = loadOtherFormat();

    if(loadSuccess)
    {
        imageLoaded = true;
        ui->starTable->clear();
        stars.clear();
        selectedStar = 0;
        ui->splitter_2->setSizes(QList<int>() << ui->optionsBox->width() << ui->splitter_2->width() << 0 );
        ui->fileNameDisplay->setText("Image: " + fileURL);
        initDisplayImage();
        return true;
    }
    return false;
}

//This method was copied and pasted and modified from the method privateLoad in fitsdata in KStars
//It loads a FITS file, reads the FITS Headers, and loads the data from the image
bool MainWindow::loadFits()
{

    int status = 0, anynullptr = 0;
    long naxes[3];
    QString errMessage;

        // Use open diskfile as it does not use extended file names which has problems opening
        // files with [ ] or ( ) in their names.
    if (fits_open_diskfile(&fptr, fileToProcess.toLatin1(), READONLY, &status))
    {
        logOutput(QString("Error opening fits file %1").arg(fileToProcess));
        return false;
    }
    else
        stats.size = QFile(fileToProcess).size();

    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
    {
        logOutput(QString("Could not locate image HDU."));
        return false;
    }

    int fitsBitPix = 0;
    if (fits_get_img_param(fptr, 3, &fitsBitPix, &(stats.ndim), naxes, &status))
    {
        logOutput(QString("FITS file open error (fits_get_img_param)."));
        return false;
    }

    if (stats.ndim < 2)
    {
        errMessage = "1D FITS images are not supported.";
        QMessageBox::critical(nullptr,"Message",errMessage);
        logOutput(errMessage);
        return false;
    }

    switch (fitsBitPix)
    {
        case BYTE_IMG:
            stats.dataType      = SEP_TBYTE;
            stats.bytesPerPixel = sizeof(uint8_t);
            break;
        case SHORT_IMG:
            // Read SHORT image as USHORT
            stats.dataType      = TUSHORT;
            stats.bytesPerPixel = sizeof(int16_t);
            break;
        case USHORT_IMG:
            stats.dataType      = TUSHORT;
            stats.bytesPerPixel = sizeof(uint16_t);
            break;
        case LONG_IMG:
            // Read LONG image as ULONG
            stats.dataType      = TULONG;
            stats.bytesPerPixel = sizeof(int32_t);
            break;
        case ULONG_IMG:
            stats.dataType      = TULONG;
            stats.bytesPerPixel = sizeof(uint32_t);
            break;
        case FLOAT_IMG:
            stats.dataType      = TFLOAT;
            stats.bytesPerPixel = sizeof(float);
            break;
        case LONGLONG_IMG:
            stats.dataType      = TLONGLONG;
            stats.bytesPerPixel = sizeof(int64_t);
            break;
        case DOUBLE_IMG:
            stats.dataType      = TDOUBLE;
            stats.bytesPerPixel = sizeof(double);
            break;
        default:
            errMessage = QString("Bit depth %1 is not supported.").arg(fitsBitPix);
            QMessageBox::critical(nullptr,"Message",errMessage);
            logOutput(errMessage);
            return false;
    }

    if (stats.ndim < 3)
        naxes[2] = 1;

    if (naxes[0] == 0 || naxes[1] == 0)
    {
        errMessage = QString("Image has invalid dimensions %1x%2").arg(naxes[0]).arg(naxes[1]);
        QMessageBox::critical(nullptr,"Message",errMessage);
        logOutput(errMessage);
    }

    stats.width               = static_cast<uint16_t>(naxes[0]);
    stats.height              = static_cast<uint16_t>(naxes[1]);
    stats.samples_per_channel = stats.width * stats.height;

    clearImageBuffers();

    m_Channels = static_cast<uint8_t>(naxes[2]);

    m_ImageBufferSize = stats.samples_per_channel * m_Channels * static_cast<uint16_t>(stats.bytesPerPixel);
    m_ImageBuffer = new uint8_t[m_ImageBufferSize];
    if (m_ImageBuffer == nullptr)
    {
        logOutput(QString("FITSData: Not enough memory for image_buffer channel. Requested: %1 bytes ").arg(m_ImageBufferSize));
        clearImageBuffers();
        return false;
    }

    long nelements = stats.samples_per_channel * m_Channels;

    if (fits_read_img(fptr, static_cast<uint16_t>(stats.dataType), 1, nelements, nullptr, m_ImageBuffer, &anynullptr, &status))
    {
        errMessage = "Error reading image.";
        QMessageBox::critical(nullptr,"Message",errMessage);
        logOutput(errMessage);
        return false;
    }

    if(checkDebayer())
        debayer();

    getSolverOptionsFromFITS();

    return true;
}

//This method I wrote combining code from the fits loading method above, the fits debayering method below, and QT
//I also consulted the ImageToFITS method in fitsdata in KStars
//The goal of this method is to load the data from a file that is not FITS format
bool MainWindow::loadOtherFormat()
{
    QImageReader fileReader(fileToProcess.toLatin1());

    if (QImageReader::supportedImageFormats().contains(fileReader.format()) == false)
    {
        logOutput("Failed to convert" + fileToProcess + "to FITS since format, " + fileReader.format() + ", is not supported in Qt");
        return false;
    }

    QString errMessage;
    QImage imageFromFile;
    if(!imageFromFile.load(fileToProcess.toLatin1()))
    {
        logOutput("Failed to open image.");
        return false;
    }

    imageFromFile = imageFromFile.convertToFormat(QImage::Format_RGB32);

    int fitsBitPix = 8; //Note: This will need to be changed.  I think QT only loads 8 bpp images.  Also the depth method gives the total bits per pixel in the image not just the bits per pixel in each channel.
     switch (fitsBitPix)
        {
            case BYTE_IMG:
                stats.dataType      = SEP_TBYTE;
                stats.bytesPerPixel = sizeof(uint8_t);
                break;
            case SHORT_IMG:
                // Read SHORT image as USHORT
                stats.dataType      = TUSHORT;
                stats.bytesPerPixel = sizeof(int16_t);
                break;
            case USHORT_IMG:
                stats.dataType      = TUSHORT;
                stats.bytesPerPixel = sizeof(uint16_t);
                break;
            case LONG_IMG:
                // Read LONG image as ULONG
                stats.dataType      = TULONG;
                stats.bytesPerPixel = sizeof(int32_t);
                break;
            case ULONG_IMG:
                stats.dataType      = TULONG;
                stats.bytesPerPixel = sizeof(uint32_t);
                break;
            case FLOAT_IMG:
                stats.dataType      = TFLOAT;
                stats.bytesPerPixel = sizeof(float);
                break;
            case LONGLONG_IMG:
                stats.dataType      = TLONGLONG;
                stats.bytesPerPixel = sizeof(int64_t);
                break;
            case DOUBLE_IMG:
                stats.dataType      = TDOUBLE;
                stats.bytesPerPixel = sizeof(double);
                break;
            default:
                errMessage = QString("Bit depth %1 is not supported.").arg(fitsBitPix);
                QMessageBox::critical(nullptr,"Message",errMessage);
                logOutput(errMessage);
                return false;
        }

    stats.width = static_cast<uint16_t>(imageFromFile.width());
    stats.height = static_cast<uint16_t>(imageFromFile.height());
    m_Channels = 3;
    stats.samples_per_channel = stats.width * stats.height;
    clearImageBuffers();
    m_ImageBufferSize = stats.samples_per_channel * m_Channels * static_cast<uint16_t>(stats.bytesPerPixel);
    m_ImageBuffer = new uint8_t[m_ImageBufferSize];
    if (m_ImageBuffer == nullptr)
    {
        logOutput(QString("FITSData: Not enough memory for image_buffer channel. Requested: %1 bytes ").arg(m_ImageBufferSize));
        clearImageBuffers();
        return false;
    }

    auto debayered_buffer = reinterpret_cast<uint8_t *>(m_ImageBuffer);
    auto * original_bayered_buffer = reinterpret_cast<uint8_t *>(imageFromFile.bits());

    // Data in RGB32, with bytes in the order of B,G,R,A, we need to copy them into 3 layers for FITS

    uint8_t * rBuff = debayered_buffer;
    uint8_t * gBuff = debayered_buffer + (stats.width * stats.height);
    uint8_t * bBuff = debayered_buffer + (stats.width * stats.height * 2);

    int imax = stats.samples_per_channel * 4 - 4;
    for (int i = 0; i <= imax; i += 4)
    {
        *rBuff++ = original_bayered_buffer[i + 2];
        *gBuff++ = original_bayered_buffer[i + 1];
        *bBuff++ = original_bayered_buffer[i + 0];
    }

    return true;
}

//This method was copied and pasted from Fitsdata in KStars
//It gets the bayer pattern information from the FITS header
bool MainWindow::checkDebayer()
{
    int status = 0;
    char bayerPattern[64];

    // Let's search for BAYERPAT keyword, if it's not found we return as there is no bayer pattern in this image
    if (fits_read_keyword(fptr, "BAYERPAT", bayerPattern, nullptr, &status))
        return false;

    if (stats.dataType != TUSHORT && stats.dataType != SEP_TBYTE)
    {
        logOutput("Only 8 and 16 bits bayered images supported.");
        return false;
    }
    QString pattern(bayerPattern);
    pattern = pattern.remove('\'').trimmed();

    if (pattern == "RGGB")
        debayerParams.filter = DC1394_COLOR_FILTER_RGGB;
    else if (pattern == "GBRG")
        debayerParams.filter = DC1394_COLOR_FILTER_GBRG;
    else if (pattern == "GRBG")
        debayerParams.filter = DC1394_COLOR_FILTER_GRBG;
    else if (pattern == "BGGR")
        debayerParams.filter = DC1394_COLOR_FILTER_BGGR;
    // We return unless we find a valid pattern
    else
    {
        logOutput(QString("Unsupported bayer pattern %1.").arg(pattern));
        return false;
    }

    fits_read_key(fptr, TINT, "XBAYROFF", &debayerParams.offsetX, nullptr, &status);
    fits_read_key(fptr, TINT, "YBAYROFF", &debayerParams.offsetY, nullptr, &status);

    //HasDebayer = true;

    return true;
}

//This method was copied and pasted from Fitsdata in KStars
//It debayers the image using the methods below
bool MainWindow::debayer()
{
    switch (stats.dataType)
    {
        case SEP_TBYTE:
            return debayer_8bit();

        case TUSHORT:
            return debayer_16bit();

        default:
            return false;
    }
}

//This method was copied and pasted from Fitsdata in KStars
//This method debayers 8 bit images
bool MainWindow::debayer_8bit()
{
    dc1394error_t error_code;

    uint32_t rgb_size = stats.samples_per_channel * 3 * stats.bytesPerPixel;
    auto * destinationBuffer = new uint8_t[rgb_size];

    auto * bayer_source_buffer      = reinterpret_cast<uint8_t *>(m_ImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<uint8_t *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        logOutput("Unable to allocate memory for temporary bayer buffer.");
        return false;
    }

    int ds1394_height = stats.height;
    auto dc1394_source = bayer_source_buffer;

    if (debayerParams.offsetY == 1)
    {
        dc1394_source += stats.width;
        ds1394_height--;
    }

    if (debayerParams.offsetX == 1)
    {
        dc1394_source++;
    }

    error_code = dc1394_bayer_decoding_8bit(dc1394_source, bayer_destination_buffer, stats.width, ds1394_height, debayerParams.filter,
                                            debayerParams.method);

    if (error_code != DC1394_SUCCESS)
    {
        logOutput(QString("Debayer failed (%1)").arg(error_code));
        m_Channels = 1;
        delete[] destinationBuffer;
        return false;
    }

    if (m_ImageBufferSize != rgb_size)
    {
        delete[] m_ImageBuffer;
        m_ImageBuffer = new uint8_t[rgb_size];

        if (m_ImageBuffer == nullptr)
        {
            delete[] destinationBuffer;
            logOutput("Unable to allocate memory for temporary bayer buffer.");
            return false;
        }

        m_ImageBufferSize = rgb_size;
    }

    auto bayered_buffer = reinterpret_cast<uint8_t *>(m_ImageBuffer);

    // Data in R1G1B1, we need to copy them into 3 layers for FITS

    uint8_t * rBuff = bayered_buffer;
    uint8_t * gBuff = bayered_buffer + (stats.width * stats.height);
    uint8_t * bBuff = bayered_buffer + (stats.width * stats.height * 2);

    int imax = stats.samples_per_channel * 3 - 3;
    for (int i = 0; i <= imax; i += 3)
    {
        *rBuff++ = bayer_destination_buffer[i];
        *gBuff++ = bayer_destination_buffer[i + 1];
        *bBuff++ = bayer_destination_buffer[i + 2];
    }

    delete[] destinationBuffer;
    return true;
}

//This method was copied and pasted from Fitsdata in KStars
//This method debayers 16 bit images
bool MainWindow::debayer_16bit()
{
    dc1394error_t error_code;

    uint32_t rgb_size = stats.samples_per_channel * 3 * stats.bytesPerPixel;
    auto * destinationBuffer = new uint8_t[rgb_size];

    auto * bayer_source_buffer      = reinterpret_cast<uint16_t *>(m_ImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<uint16_t *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        logOutput("Unable to allocate memory for temporary bayer buffer.");
        return false;
    }

    int ds1394_height = stats.height;
    auto dc1394_source = bayer_source_buffer;

    if (debayerParams.offsetY == 1)
    {
        dc1394_source += stats.width;
        ds1394_height--;
    }

    if (debayerParams.offsetX == 1)
    {
        dc1394_source++;
    }

    error_code = dc1394_bayer_decoding_16bit(dc1394_source, bayer_destination_buffer, stats.width, ds1394_height, debayerParams.filter,
                 debayerParams.method, 16);

    if (error_code != DC1394_SUCCESS)
    {
        logOutput(QString("Debayer failed (%1)").arg(error_code));
        m_Channels = 1;
        delete[] destinationBuffer;
        return false;
    }

    if (m_ImageBufferSize != rgb_size)
    {
        delete[] m_ImageBuffer;
        m_ImageBuffer = new uint8_t[rgb_size];

        if (m_ImageBuffer == nullptr)
        {
            delete[] destinationBuffer;
            logOutput("Unable to allocate memory for temporary bayer buffer.");
            return false;
        }

        m_ImageBufferSize = rgb_size;
    }

    auto bayered_buffer = reinterpret_cast<uint16_t *>(m_ImageBuffer);

    // Data in R1G1B1, we need to copy them into 3 layers for FITS

    uint16_t * rBuff = bayered_buffer;
    uint16_t * gBuff = bayered_buffer + (stats.width * stats.height);
    uint16_t * bBuff = bayered_buffer + (stats.width * stats.height * 2);

    int imax = stats.samples_per_channel * 3 - 3;
    for (int i = 0; i <= imax; i += 3)
    {
        *rBuff++ = bayer_destination_buffer[i];
        *gBuff++ = bayer_destination_buffer[i + 1];
        *bBuff++ = bayer_destination_buffer[i + 2];
    }

    delete[] destinationBuffer;
    return true;
}

//This method was copied and pasted from Fitsview in KStars
//It sets up the image that will be displayed on the screen
void MainWindow::initDisplayImage()
{
    // Account for leftover when sampling. Thus a 5-wide image sampled by 2
    // would result in a width of 3 (samples 0, 2 and 4).

    int w = (stats.width + sampling - 1) / sampling;
    int h = (stats.height + sampling - 1) / sampling;

    if (m_Channels == 1)
    {
        rawImage = QImage(w, h, QImage::Format_Indexed8);

        rawImage.setColorCount(256);
        for (int i = 0; i < 256; i++)
            rawImage.setColor(i, qRgb(i, i, i));
    }
    else
    {
        rawImage = QImage(w, h, QImage::Format_RGB32);
    }

    autoScale();

}

//This method reacts when the user clicks the zoom in button
void MainWindow::zoomIn()
{
    if(!imageLoaded)
        return;

    currentZoom *= 1.5;
    updateImage();
}

//This method reacts when the user clicks the zoom out button
void MainWindow::zoomOut()
{
    if(!imageLoaded)
        return;

    currentZoom /= 1.5;
    updateImage();
}

//This code was copied and pasted and modified from rescale in Fitsview in KStars
//This method reacts when the user clicks the autoscale button and is called when the image is first loaded.
void MainWindow::autoScale()
{
    if(!imageLoaded)
        return;

    int w = (stats.width + sampling - 1) / sampling;
    int h = (stats.height + sampling - 1) / sampling;

    double width = ui->imageScrollArea->rect().width();
    double height = ui->imageScrollArea->rect().height() - 30; //The minus 30 is due to the image filepath label

    // Find the zoom level which will enclose the current FITS in the current window size
    double zoomX                  = ( width / w);
    double zoomY                  = ( height / h);
    (zoomX < zoomY) ? currentZoom = zoomX : currentZoom = zoomY;

    updateImage();
}


//This method is intended to get the position and size of the star for rendering purposes
//It is used to draw circles/ellipses for the stars and to detect when the mouse is over a star
QRect MainWindow::getStarSizeInImage(Star star)
{
    double width = 0;
    double height = 0;
    switch(ui->starOptions->currentIndex())
    {
        case 0: //Ellipse from Sextraction
            width = 2 * star.a ;
            height = 2 * star.b;
            break;

        case 1: //Circle from Sextraction
            {
                double size = 2 * sqrt( pow(star.a, 2) + pow(star.b, 2) );
                width = size;
                height = size;
            }
            break;

        case 2: //HFD Size, based on HFR, 2 x radius is the diameter
            width = 2 * star.HFR;
            height = 2 * star.HFR;
            break;

        case 3: //2 x HFD size, based on HFR, 4 x radius is 2 x the diameter
            width = 4 * star.HFR;
            height = 4 * star.HFR;
            break;
    }

    double starx = star.x * currentWidth / stats.width ;
    double stary = star.y * currentHeight / stats.height;
    double starw = width * currentWidth / stats.width;
    double starh = height * currentHeight / stats.height;
    return QRect(starx - starw, stary - starh , starw*2, starh*2);
}

//This method is very loosely based on updateFrame in Fitsview in Kstars
//It will redraw the image when the user loads an image, zooms in, zooms out, or autoscales
//It will also redraw the image when a change needs to be made in how the circles for the stars are displayed such as highlighting one star
void MainWindow::updateImage()
{
    if(!imageLoaded)
        return;

    int w = (stats.width + sampling - 1) / sampling;
    int h = (stats.height + sampling - 1) / sampling;
    currentWidth  = static_cast<int> (w * (currentZoom));
    currentHeight = static_cast<int> (h * (currentZoom));
    doStretch(&rawImage);

    scaledImage = rawImage.scaled(currentWidth, currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap renderedImage = QPixmap::fromImage(scaledImage);
    if(ui->showStars->isChecked())
    {
        QPainter p(&renderedImage);
        for(int starnum = 0 ; starnum < stars.size() ; starnum++)
        {
            Star star = stars.at(starnum);
            QRect starInImage = getStarSizeInImage(star);
            p.save();
            p.translate(starInImage.center());
            p.rotate(star.theta);
            p.translate(-starInImage.center());

            if(starnum == selectedStar)
            {
                QPen highlighter(QColor("yellow"));
                highlighter.setWidth(4);
                p.setPen(highlighter);
                p.setOpacity(1);
                p.drawEllipse(starInImage);
            }
            else
            {
                p.setPen(QColor("red"));
                p.setOpacity(0.6);
                p.drawEllipse(starInImage);
            }
            p.restore();
        }
        p.end();
    }

    ui->Image->setPixmap(renderedImage);
    ui->Image->setFixedSize(renderedImage.size());
}

//This code is copied and pasted from FITSView in KStars
//It handles the stretch of the image
void MainWindow::doStretch(QImage *outputImage)
{
    if (outputImage->isNull())
        return;
    Stretch stretch(static_cast<int>(stats.width),
                    static_cast<int>(stats.height),
                    m_Channels, static_cast<uint16_t>(stats.dataType));

   // Compute new auto-stretch params.
    stretchParams = stretch.computeParams(m_ImageBuffer);

    stretch.setParams(stretchParams);
    stretch.run(m_ImageBuffer, outputImage, sampling);
}

//This method was copied and pasted from Fitsdata in KStars
//It clears the image buffer out.
void MainWindow::clearImageBuffers()
{
    delete[] m_ImageBuffer;
    m_ImageBuffer = nullptr;
    //m_BayerBuffer = nullptr;
}

//I wrote this method to respond when the user's mouse is over a star
//It displays details about that particular star in a tooltip and highlights it in the image
//It also displays the x and y position of the mouse in the image and the pixel value at that spot now.
void MainWindow::mouseMovedOverImage(QPoint location)
{
    if(imageLoaded)
    {
        double x = location.x() * stats.width / currentWidth;
        double y = location.y() * stats.height / currentHeight;

        ui->mouseInfo->setText(QString("X: %1, Y: %2, Value: %3").arg(x).arg(y).arg(getValue(x,y)));

        bool starFound = false;
        for(int i = 0 ; i < stars.size() ; i ++)
        {
            Star star = stars.at(i);
            QRect starInImage = getStarSizeInImage(star);
            if(starInImage.contains(location))
            {
                QString text;
                if(ui->calculateHFR->isChecked())
                    text = QString("Star: %1, x: %2, y: %3, mag: %4, flux: %5, peak:%6, HFR: %7").arg(i + 1).arg(star.x).arg(star.y).arg(star.mag).arg(star.flux).arg(star.peak).arg(star.HFR);
                 else
                    text = QString("Star: %1, x: %2, y: %3, mag: %4, flux: %5, peak:%6").arg(i + 1).arg(star.x).arg(star.y).arg(star.mag).arg(star.flux).arg(star.peak);
                QToolTip::showText(QCursor::pos(), text, ui->Image);
                selectedStar = i;
                starFound = true;
                updateImage();
            }
        }
        if(!starFound)
            QToolTip::hideText();
    }
}



//This function is based upon code in the method mouseMoveEvent in FITSLabel in KStars
//It is meant to get the value from the image buffer at a particular pixel location for the display
//when the mouse is over a certain pixel
QString MainWindow::getValue(int x, int y)
{
    if (m_ImageBuffer == nullptr)
        return "";

    int index = y * stats.width + x;
    QString stringValue;

    switch (stats.dataType)
    {
        case TBYTE:
            stringValue = QLocale().toString(m_ImageBuffer[index]);
            break;

        case TSHORT:
            stringValue = QLocale().toString((reinterpret_cast<int16_t *>(m_ImageBuffer))[index]);
            break;

        case TUSHORT:
            stringValue = QLocale().toString((reinterpret_cast<uint16_t *>(m_ImageBuffer))[index]);
            break;

        case TLONG:
            stringValue = QLocale().toString((reinterpret_cast<int32_t *>(m_ImageBuffer))[index]);
            break;

        case TULONG:
            stringValue = QLocale().toString((reinterpret_cast<uint32_t *>(m_ImageBuffer))[index]);
            break;

        case TFLOAT:
            stringValue = QLocale().toString((reinterpret_cast<float *>(m_ImageBuffer))[index], 'f', 5);
            break;

        case TLONGLONG:
            stringValue = QLocale().toString(static_cast<int>((reinterpret_cast<int64_t *>(m_ImageBuffer))[index]));
            break;

        case TDOUBLE:
            stringValue = QLocale().toString((reinterpret_cast<float *>(m_ImageBuffer))[index], 'f', 5);

            break;

        default:
            break;
    }
    return stringValue;
}

//I wrote the is method to respond when the user clicks on a star
//It highlights the row in the star table that corresponds to that star
void MainWindow::mouseClickedOnStar(QPoint location)
{
    for(int i = 0 ; i < stars.size() ; i ++)
    {
        QRect starInImage = getStarSizeInImage(stars.at(i));
        if(starInImage.contains(location))
            ui->starTable->selectRow(i);
    }
}


//THis method responds to row selections in the table and higlights the star you select in the image
void MainWindow::starClickedInTable()
{
    if(ui->starTable->selectedItems().count() > 0)
    {
        QTableWidgetItem *currentItem = ui->starTable->selectedItems().first();
        selectedStar = ui->starTable->row(currentItem);
        Star star = stars.at(selectedStar);
        double starx = star.x * currentWidth / stats.width ;
        double stary = star.y * currentHeight / stats.height;
        updateImage();
        ui->imageScrollArea->ensureVisible(starx,stary);
    }
}

//This sorts the stars by magnitude for display purposes
void MainWindow::sortStars()
{
    if(stars.size() > 1)
    {
        //Note that a star is dimmer when the mag is greater!
        //We want to sort in decreasing order though!
        std::sort(stars.begin(), stars.end(), [](const Star &s1, const Star &s2)
        {
            return s1.mag < s2.mag;
        });
    }
    updateStarTableFromList();
}

//This is a helper function that I wrote for the methods below
//It add a column with a particular name to the specified table
void addColumnToTable(QTableWidget *table, QString heading)
{
    int colNum = table->columnCount();
    table->insertColumn(colNum);
    table->setHorizontalHeaderItem(colNum,new QTableWidgetItem(heading));
}

//This is a method I wrote to hide the desired columns in a table based on their name
void setColumnHidden(QTableWidget *table, QString colName, bool hidden)
{
    for(int c = 0; c < table->columnCount() ; c ++)
    {
        if(table->horizontalHeaderItem(c)->text() == colName)
            table->setColumnHidden(c, hidden);
    }
}

//This is a helper function that I wrote for the methods below
//It sets the value of a cell in the column of the specified name in the last row in the table
bool setItemInColumn(QTableWidget *table, QString colName, QString value)
{
    int row = table->rowCount() - 1;
    for(int c = 0; c < table->columnCount() ; c ++)
    {
        if(table->horizontalHeaderItem(c)->text() == colName)
        {
            table->setItem(row,c, new QTableWidgetItem(value));
            return true;
        }
    }
    return false;
}

//This copies the stars into the table
void MainWindow::updateStarTableFromList()
{
    QTableWidget *table = ui->starTable;
    table->clearContents();
    table->setRowCount(0);
    table->setColumnCount(0);
    selectedStar = 0;
    addColumnToTable(table,"MAG_AUTO");
    addColumnToTable(table,"X_IMAGE");
    addColumnToTable(table,"Y_IMAGE");

    addColumnToTable(table,"FLUX_AUTO");
    addColumnToTable(table,"PEAK");
    if(!sexySolver.isNull() && sexySolver->calculateHFR)
        addColumnToTable(table,"HFR");

    addColumnToTable(table,"a");
    addColumnToTable(table,"b");
    addColumnToTable(table,"theta");

    for(int i = 0; i < stars.size(); i ++)
    {
        table->insertRow(table->rowCount());
        Star star = stars.at(i);

        setItemInColumn(table, "MAG_AUTO", QString::number(star.mag));
        setItemInColumn(table, "X_IMAGE", QString::number(star.x));
        setItemInColumn(table, "Y_IMAGE", QString::number(star.y));

        setItemInColumn(table, "FLUX_AUTO", QString::number(star.flux));
        setItemInColumn(table, "PEAK", QString::number(star.peak));
        if(!sexySolver.isNull() && sexySolver->calculateHFR)
            setItemInColumn(table, "HFR", QString::number(star.HFR));

        setItemInColumn(table, "a", QString::number(star.a));
        setItemInColumn(table, "b", QString::number(star.b));
        setItemInColumn(table, "theta", QString::number(star.theta));
    }
    updateHiddenStarTableColumns();
}

void MainWindow::updateHiddenStarTableColumns()
{
    QTableWidget *table = ui->starTable;

    setColumnHidden(table,"FLUX_AUTO", !showFluxInfo);
    setColumnHidden(table,"PEAK", !showFluxInfo);
    setColumnHidden(table,"HFR", !showFluxInfo);

    setColumnHidden(table,"a", !showStarShapeInfo);
    setColumnHidden(table,"b", !showStarShapeInfo);
    setColumnHidden(table,"theta", !showStarShapeInfo);
}

//This method is copied and pasted and modified from getSolverOptionsFromFITS in Align in KStars
//Then it was split in two parts, the other part was sent to the ExternalSextractorSolver class since the internal solver doesn't need it
//This part extracts the options from the FITS file and prepares them for use by the internal or external solver
bool MainWindow::getSolverOptionsFromFITS()
{
    clearAstrometrySettings();

    double ra, dec;

    int status = 0, fits_ccd_width, fits_ccd_height, fits_binx = 1, fits_biny = 1;
    char comment[128], error_status[512];
    fitsfile *fptr = nullptr;

    double fits_fov_x, fits_fov_y, fov_lower, fov_upper, fits_ccd_hor_pixel = -1,
           fits_ccd_ver_pixel = -1, fits_focal_length = -1;

    status = 0;

    // Use open diskfile as it does not use extended file names which has problems opening
    // files with [ ] or ( ) in their names.
    if (fits_open_diskfile(&fptr, fileToProcess.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logOutput(QString::fromUtf8(error_status));
        return false;
    }

    status = 0;
    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logOutput(QString::fromUtf8(error_status));
        return false;
    }

    status = 0;
    if (fits_read_key(fptr, TINT, "NAXIS1", &fits_ccd_width, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logOutput("FITS header: cannot find NAXIS1.");
        return false;
    }

    status = 0;
    if (fits_read_key(fptr, TINT, "NAXIS2", &fits_ccd_height, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logOutput("FITS header: cannot find NAXIS2.");
        return false;
    }

    bool coord_ok = true;

    status = 0;
    char objectra_str[32];
    if (fits_read_key(fptr, TSTRING, "OBJCTRA", objectra_str, comment, &status))
    {
        if (fits_read_key(fptr, TDOUBLE, "RA", &ra, comment, &status))
        {
            fits_report_error(stderr, status);
            fits_get_errstatus(status, error_status);
            coord_ok = false;
            logOutput(QString("FITS header: cannot find OBJCTRA (%1).").arg(QString(error_status)));
        }
        else
            // Degrees to hours
            ra /= 15;
    }
    else
    {
        dms raDMS = dms::fromString(objectra_str, false);
        ra        = raDMS.Hours();
    }

    status = 0;
    char objectde_str[32];
    if (coord_ok && fits_read_key(fptr, TSTRING, "OBJCTDEC", objectde_str, comment, &status))
    {
        if (fits_read_key(fptr, TDOUBLE, "DEC", &dec, comment, &status))
        {
            fits_report_error(stderr, status);
            fits_get_errstatus(status, error_status);
            coord_ok = false;
            logOutput(QString("FITS header: cannot find OBJCTDEC (%1).").arg(QString(error_status)));
        }
    }
    else
    {
        dms deDMS = dms::fromString(objectde_str, true);
        dec       = deDMS.Degrees();
    }

    if(coord_ok)
    {
        ui->use_position->setChecked(true);
        ui->ra->setText(QString::number(ra));
        ui->dec->setText(QString::number(dec));

    }

    status = 0;
    double pixelScale = 0;
    // If we have pixel scale in arcsecs per pixel then lets use that directly
    // instead of calculating it from FOCAL length and other information
    if (fits_read_key(fptr, TDOUBLE, "SCALE", &pixelScale, comment, &status) == 0)
    {
        double fov_low  = 0.9 * pixelScale;
        double fov_high = 1.1 * pixelScale;

        ui->scale_low->setText(QString::number(fov_low));
        ui->scale_high->setText(QString::number(fov_high));
        ui->units->setCurrentText("app");
        ui->use_scale->setChecked(true);

        return true;
    }

    if (fits_read_key(fptr, TDOUBLE, "FOCALLEN", &fits_focal_length, comment, &status))
    {
        int integer_focal_length = -1;
        if (fits_read_key(fptr, TINT, "FOCALLEN", &integer_focal_length, comment, &status))
        {
            fits_report_error(stderr, status);
            fits_get_errstatus(status, error_status);
            logOutput(QString("FITS header: cannot find FOCALLEN: (%1).").arg(QString(error_status)));
            return false;
        }
        else
            fits_focal_length = integer_focal_length;
    }

    status = 0;
    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE1", &fits_ccd_hor_pixel, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logOutput(QString("FITS header: cannot find PIXSIZE1 (%1).").arg(QString(error_status)));
        return false;
    }

    status = 0;
    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE2", &fits_ccd_ver_pixel, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logOutput(QString("FITS header: cannot find PIXSIZE2 (%1).").arg(QString(error_status)));
        return false;
    }

    status = 0;
    fits_read_key(fptr, TINT, "XBINNING", &fits_binx, comment, &status);
    status = 0;
    fits_read_key(fptr, TINT, "YBINNING", &fits_biny, comment, &status);

    // Calculate FOV
    fits_fov_x = 206264.8062470963552 * fits_ccd_width * fits_ccd_hor_pixel / 1000.0 / fits_focal_length * fits_binx;
    fits_fov_y = 206264.8062470963552 * fits_ccd_height * fits_ccd_ver_pixel / 1000.0 / fits_focal_length * fits_biny;

    fits_fov_x /= 60.0;
    fits_fov_y /= 60.0;

    // let's stretch the boundaries by 10%
    fov_lower = qMin(fits_fov_x, fits_fov_y);
    fov_upper = qMax(fits_fov_x, fits_fov_y);

    fov_lower *= 0.90;
    fov_upper *= 1.10;

    //Final Options that get stored.

    ui->scale_low->setText(QString::number(fov_lower));
    ui->scale_high->setText(QString::number(fov_upper));
    ui->units->setCurrentText("aw");
    ui->use_scale->setChecked(true);

    return true;
}


//Note: The next 3 functions are designed to work in an easily editable way.
//To add new columns to this table, just add them to the first function
//To have it fill the column when a Sextraction or Solve is complete, add it to one or both of the next two functions
//So that the column gets setup and then gets filled in.

//This method sets up the results table to start with.
void MainWindow::setupResultsTable()
{

     QTableWidget *table = ui->resultsTable;

    //These are in the order that they will appear in the table.

    addColumnToTable(table,"Time");
    addColumnToTable(table,"Int?");
    addColumnToTable(table,"Command");
    addColumnToTable(table,"Stars");
    //Sextractor Parameters
    addColumnToTable(table,"doHFR");
    addColumnToTable(table,"Shape");
    addColumnToTable(table,"Kron");
    addColumnToTable(table,"Subpix");
    addColumnToTable(table,"r_min");
    addColumnToTable(table,"minarea");
    addColumnToTable(table,"d_thresh");
    addColumnToTable(table,"d_cont");
    addColumnToTable(table,"clean");
    addColumnToTable(table,"clean param");
    addColumnToTable(table,"fwhm");
    //Star Filtering Parameters
    addColumnToTable(table,"Max Ell");
    addColumnToTable(table,"Cut Bri");
    addColumnToTable(table,"Cut Dim");
    addColumnToTable(table,"Sat Lim");
    //Astrometry Parameters
    addColumnToTable(table,"Pos?");
    addColumnToTable(table,"Scale?");
    addColumnToTable(table,"Resort?");
    addColumnToTable(table,"Down");
    addColumnToTable(table,"in ||");
    //Results
    addColumnToTable(table,"RA");
    addColumnToTable(table,"DEC");
    addColumnToTable(table,"Orientation");
    addColumnToTable(table,"Field Width");
    addColumnToTable(table,"Field Height");
    addColumnToTable(table,"PixScale");
    addColumnToTable(table,"Parity");
    addColumnToTable(table,"Field");

    updateHiddenResultsTableColumns();
}

//This adds a Sextraction to the Results Table
//To add, remove, or change the way certain columns are filled when a sextraction is finished, edit them here.
void MainWindow::addSextractionToTable()
{
    QTableWidget *table = ui->resultsTable;

    setItemInColumn(table, "Time", QString::number(elapsed));
    if(extSolver.isNull())
        setItemInColumn(table, "Int?", "Internal");
    else
        setItemInColumn(table, "Int?", "External");
    setItemInColumn(table, "Command", "Sextract");
    setItemInColumn(table, "Stars", QString::number(sexySolver->getNumStarsFound()));
    //Sextractor Parameters
    setItemInColumn(table,"doHFR", QVariant(sexySolver->calculateHFR).toString());
    QString shapeName="Circle";
    switch(sexySolver->apertureShape)
    {
        case SHAPE_AUTO:
            shapeName = "Auto";
        break;

        case SHAPE_CIRCLE:
           shapeName = "Circle";
        break;

        case SHAPE_ELLIPSE:
            shapeName = "Ellipse";
        break;
    }

    setItemInColumn(table,"Shape", shapeName);
    setItemInColumn(table,"Kron", QString::number(sexySolver->kron_fact));
    setItemInColumn(table,"Subpix", QString::number(sexySolver->subpix));
    setItemInColumn(table,"r_min", QString::number(sexySolver->r_min));
    setItemInColumn(table,"minarea", QString::number(sexySolver->minarea));
    setItemInColumn(table,"d_thresh", QString::number(sexySolver->deblend_thresh));
    setItemInColumn(table,"d_cont", QString::number(sexySolver->deblend_contrast));
    setItemInColumn(table,"clean", QString::number(sexySolver->clean));
    setItemInColumn(table,"clean param", QString::number(sexySolver->clean_param));
    setItemInColumn(table,"fwhm", QString::number(sexySolver->fwhm));
    setItemInColumn(table, "Field", ui->fileNameDisplay->text());

    //StarFilter Parameters
    setItemInColumn(table,"Max Ell", QString::number(sexySolver->maxEllipse));
    setItemInColumn(table,"Cut Bri", QString::number(sexySolver->removeBrightest));
    setItemInColumn(table,"Cut Dim", QString::number(sexySolver->removeDimmest));
    setItemInColumn(table,"Sat Lim", QString::number(sexySolver->saturationLimit));

}

//This adds a solution to the Results Table
//To add, remove, or change the way certain columns are filled when a solve is finished, edit them here.
void MainWindow::addSolutionToTable(Solution solution)
{
    QTableWidget *table = ui->resultsTable;
    setItemInColumn(table, "Time", QString::number(elapsed));
    if(extSolver.isNull())
        setItemInColumn(table, "Int?", "Internal");
    else
        setItemInColumn(table, "Int?", "External");

    setItemInColumn(table, "Command", sexySolver->command);

    //Astrometry Parameters
    setItemInColumn(table, "Pos?", QVariant(sexySolver->use_position).toString());
    setItemInColumn(table, "Scale?", QVariant(sexySolver->use_scale).toString());
    setItemInColumn(table, "Resort?", QVariant(sexySolver->resort).toString());
    setItemInColumn(table, "Down", QVariant(sexySolver->downsample).toString());
    setItemInColumn(table, "in ||", QVariant(sexySolver->inParallel).toString());

    //Results
    setItemInColumn(table, "RA", solution.rastr);
    setItemInColumn(table, "DEC", solution.decstr);
    setItemInColumn(table, "Orientation", QString::number(solution.orientation));
    setItemInColumn(table, "Field Width", QString::number(solution.fieldWidth));
    setItemInColumn(table, "Field Height", QString::number(solution.fieldHeight));
    setItemInColumn(table, "PixScale", QString::number(solution.pixscale));
    setItemInColumn(table, "Parity", solution.parity);
    setItemInColumn(table, "Field", ui->fileNameDisplay->text());
}

//I wrote this method to hide certain columns in the Results Table if the user wants to reduce clutter in the table.
void MainWindow::updateHiddenResultsTableColumns()
{
    QTableWidget *table = ui->resultsTable;
    //Sextractor Params
    setColumnHidden(table,"doHFR", !showSextractorParams);
    setColumnHidden(table,"Shape", !showSextractorParams);
    setColumnHidden(table,"Kron", !showSextractorParams);
    setColumnHidden(table,"Subpix", !showSextractorParams);
    setColumnHidden(table,"r_min", !showSextractorParams);
    setColumnHidden(table,"minarea", !showSextractorParams);
    setColumnHidden(table,"d_thresh", !showSextractorParams);
    setColumnHidden(table,"d_cont", !showSextractorParams);
    setColumnHidden(table,"clean", !showSextractorParams);
    setColumnHidden(table,"clean param", !showSextractorParams);
    setColumnHidden(table,"fwhm", !showSextractorParams);
    //Star Filtering Parameters
    setColumnHidden(table,"Max Ell", !showSextractorParams);
    setColumnHidden(table,"Cut Bri", !showSextractorParams);
    setColumnHidden(table,"Cut Dim", !showSextractorParams);
    setColumnHidden(table,"Sat Lim", !showSextractorParams);
    //Astrometry Parameters
    setColumnHidden(table,"Pos?", !showAstrometryParams);
    setColumnHidden(table,"Scale?", !showAstrometryParams);
    setColumnHidden(table,"Resort?", !showAstrometryParams);
    setColumnHidden(table,"Down", !showAstrometryParams);
    setColumnHidden(table,"in ||", !showAstrometryParams);
    //Results
    setColumnHidden(table,"RA", !showSolutionDetails);
    setColumnHidden(table,"DEC", !showSolutionDetails);
    setColumnHidden(table,"Orientation", !showSolutionDetails);
    setColumnHidden(table,"Field Width", !showSolutionDetails);
    setColumnHidden(table,"Field Height", !showSolutionDetails);
    setColumnHidden(table,"PixScale", !showSolutionDetails);
    setColumnHidden(table,"Parity", !showSolutionDetails);
}

//This will write the Results table to a csv file if the user desires
//Then the user can analyze the solution information in more detail to try to perfect sextractor and solver parameters
void MainWindow::saveResultsTable()
{
    if (ui->resultsTable->rowCount() == 0)
        return;

    QUrl exportFile = QFileDialog::getSaveFileUrl(this, "Export Results Table", dirPath,
                      "CSV File (*.csv)");
    if (exportFile.isEmpty()) // if user presses cancel
        return;
    if (exportFile.toLocalFile().endsWith(QLatin1String(".csv")) == false)
        exportFile.setPath(exportFile.toLocalFile() + ".csv");

    QString path = exportFile.toLocalFile();

    if (QFile::exists(path))
    {
        int r = QMessageBox::question(this, "Overwrite it?",
                QString("A file named \"%1\" already exists. Do you want to overwrite it?").arg(exportFile.fileName()));
        if (r == QMessageBox::No)
            return;
    }

    if (!exportFile.isValid())
    {
        QString message = QString("Invalid URL: %1").arg(exportFile.url());
        QMessageBox::warning(this, "Invalid URL", message);
        return;
    }

    QFile file;
    file.setFileName(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        QString message = QString("Unable to write to file %1").arg(path);
        QMessageBox::warning(this, "Could Not Open File", message);
        return;
    }

    QTextStream outstream(&file);

    for (int c = 0; c < ui->resultsTable->columnCount(); c++)
    {
        outstream << ui->resultsTable->horizontalHeaderItem(c)->text() << ',';
    }
    outstream << "\n";

    for (int r = 0; r < ui->resultsTable->rowCount(); r++)
    {
        for (int c = 0; c < ui->resultsTable->columnCount(); c++)
        {
            QTableWidgetItem *cell = ui->resultsTable->item(r, c);

            if (cell)
                outstream << cell->text() << ',';
            else
                outstream << " " << ',';
        }
        outstream << endl;
    }
    QMessageBox::information(this, "Message", QString("Solution Points Saved as: %1").arg(path));
    file.close();
}











