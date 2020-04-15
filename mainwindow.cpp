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

MainWindow::MainWindow() :
    QMainWindow(),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);


   this->show();

    //The Options at the top of the Window
    connect(ui->ImageLoad,&QAbstractButton::clicked, this, &MainWindow::imageLoad );
    connect(ui->zoomIn,&QAbstractButton::clicked, this, &MainWindow::zoomIn );
    connect(ui->zoomOut,&QAbstractButton::clicked, this, &MainWindow::zoomOut );
    connect(ui->AutoScale,&QAbstractButton::clicked, this, &MainWindow::autoScale );

    //The Options at the bottom of the Window
    connect(ui->SextractStars,&QAbstractButton::clicked, this, &MainWindow::sextractImage );
    connect(ui->SolveImage,&QAbstractButton::clicked, this, &MainWindow::solveImage );
    connect(ui->classicSolve,&QAbstractButton::clicked, this, &MainWindow::classicSolve );

    connect(ui->Abort,&QAbstractButton::clicked, this, &MainWindow::abort );
    connect(ui->reset, &QPushButton::clicked, this, &MainWindow::resetOptionsToDefaults);
    connect(ui->Clear,&QAbstractButton::clicked, this, &MainWindow::clearAll );
    connect(ui->exportTable,&QAbstractButton::clicked, this, &MainWindow::saveResultsTable);

    //Behaviors for the StarList
    ui->starList->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(ui->starList,&QTableWidget::itemSelectionChanged, this, &MainWindow::starClickedInTable);

    //Behaviors for the Mouse over the Image to interact with the StartList and the UI
    connect(ui->Image,&ImageLabel::mouseHovered,this, &MainWindow::mouseMovedOverImage);
    connect(ui->Image,&ImageLabel::mouseClicked,this, &MainWindow::mouseClickedOnStar);

    //Hides the panels into the sides and bottom
    ui->splitter->setSizes(QList<int>() << ui->splitter->height() << 0 );
    ui->splitter_2->setSizes(QList<int>() << 0 << ui->splitter_2->width() << 0 );

    //Settings for the External Sextractor and Solver
    connect(ui->configFilePath, &QLineEdit::textChanged, this, [this](){ confPath = ui->configFilePath->text(); });
    connect(ui->sextractorPath, &QLineEdit::textChanged, this, [this](){ sextractorBinaryPath = ui->sextractorPath->text(); });
    connect(ui->solverPath, &QLineEdit::textChanged, this, [this](){ solverPath = ui->solverPath->text(); });
    connect(ui->tempPath, &QLineEdit::textChanged, this, [this](){ tempPath = ui->tempPath->text(); });
    connect(ui->wcsPath, &QLineEdit::textChanged, this, [this](){ wcsPath = ui->wcsPath->text(); });
    connect(ui->cleanupTemp, &QCheckBox::stateChanged, this, [this](){ cleanupTemporaryFiles = ui->cleanupTemp->isChecked(); });

    //SexySolver Tester Options
    connect(ui->calculateHFR, &QCheckBox::stateChanged, this, [this](){ calculateHFR = ui->calculateHFR->isChecked(); });
    connect(ui->showStars,&QAbstractButton::clicked, this, &MainWindow::updateImage );
    connect(ui->starOptions,QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateImage);
    connect(ui->showFluxInfo, &QCheckBox::stateChanged, this, [this](){ showFluxInfo = ui->showFluxInfo->isChecked(); updateHiddenStarTableColumns(); });
    connect(ui->showStarShapeInfo, &QCheckBox::stateChanged, this, [this](){ showStarShapeInfo = ui->showStarShapeInfo->isChecked(); updateHiddenStarTableColumns();});
    connect(ui->showSextractorParams, &QCheckBox::stateChanged, this, [this](){ showSextractorParams = ui->showSextractorParams->isChecked(); updateHiddenResultsTableColumns(); });
    connect(ui->showAstrometryParams, &QCheckBox::stateChanged, this, [this](){ showAstrometryParams = ui->showAstrometryParams->isChecked(); updateHiddenResultsTableColumns(); });
    connect(ui->showSolutionDetails, &QCheckBox::stateChanged, this, [this](){ showSolutionDetails = ui->showSolutionDetails->isChecked(); updateHiddenResultsTableColumns(); });

    //Sextractor Settings

    connect(ui->apertureShape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int num){ apertureShape = (Shape) num; });
    connect(ui->kron_fact, &QLineEdit::textChanged, this, [this](){ kron_fact = ui->kron_fact->text().toDouble(); });
    connect(ui->subpix, &QLineEdit::textChanged, this, [this](){ subpix = ui->subpix->text().toDouble(); });
    connect(ui->r_min, &QLineEdit::textChanged, this, [this](){ r_min = ui->r_min->text().toDouble(); });
    //no inflags???;
    connect(ui->magzero, &QLineEdit::textChanged, this, [this](){ magzero = ui->magzero->text().toDouble(); });
    connect(ui->minarea, &QLineEdit::textChanged, this, [this](){ minarea = ui->minarea->text().toDouble(); });
    connect(ui->deblend_thresh, &QLineEdit::textChanged, this, [this](){ deblend_thresh = ui->deblend_thresh->text().toDouble(); });
    connect(ui->deblend_contrast, &QLineEdit::textChanged, this, [this](){ deblend_contrast = ui->deblend_contrast->text().toDouble(); });
    connect(ui->cleanCheckBox,&QCheckBox::stateChanged,this,[this](){
        if(ui->cleanCheckBox->isChecked())
            clean = 1;
        else
            clean = 0;
    });
    connect(ui->clean_param, &QLineEdit::textChanged, this, [this](){ clean_param = ui->clean_param->text().toDouble(); });

    //This generates an array that can be used as a convFilter based on the desired FWHM
    connect(ui->fwhm, &QLineEdit::textChanged, this, [this](){ fwhm = ui->fwhm->text().toDouble(); });

    //Star Filter Settings
    connect(ui->maxEllipse, &QLineEdit::textChanged, this, [this](){ maxEllipse = ui->maxEllipse->text().toDouble(); });
    connect(ui->brightestPercent, &QLineEdit::textChanged, this, [this](){ removeBrightest = ui->brightestPercent->text().toDouble(); });
    connect(ui->dimmestPercent, &QLineEdit::textChanged, this, [this](){ removeDimmest = ui->dimmestPercent->text().toDouble(); });
    connect(ui->saturationLimit, &QLineEdit::textChanged, this, [this](){ saturationLimit = ui->saturationLimit->text().toDouble(); });

    //Astrometry Settings
    connect(ui->inParallel, &QCheckBox::stateChanged, this, [this](){ inParallel = ui->inParallel->isChecked(); });
    connect(ui->solverTimeLimit, &QLineEdit::textChanged, this, [this](){ solverTimeLimit = ui->solverTimeLimit->text().toInt(); });
    connect(ui->minWidth, &QLineEdit::textChanged, this, [this](){ minwidth = ui->minWidth->text().toDouble(); });
    connect(ui->maxWidth, &QLineEdit::textChanged, this, [this](){ maxwidth = ui->maxWidth->text().toDouble(); });

    connect(ui->use_scale, &QCheckBox::stateChanged, this, [this](){ use_scale = ui->use_scale->isChecked(); });
    connect(ui->scale_low, &QLineEdit::textChanged, this, [this](){ fov_low = ui->scale_low->text().toDouble(); });
    connect(ui->scale_high, &QLineEdit::textChanged, this, [this](){ fov_high = ui->scale_high->text().toDouble(); });
    connect(ui->units, &QComboBox::currentTextChanged, this, [this](QString text){ units = text; });

    connect(ui->use_position, &QCheckBox::stateChanged, this, [this](){ use_position= ui->use_position->isChecked(); });
    connect(ui->ra, &QLineEdit::textChanged, this, [this](){ ra = ui->ra->text().toDouble(); });
    connect(ui->dec, &QLineEdit::textChanged, this, [this](){ dec = ui->dec->text().toDouble(); });
    connect(ui->radius, &QLineEdit::textChanged, this, [this](){ radius = ui->radius->text().toDouble(); });

    connect(ui->oddsToKeep, &QLineEdit::textChanged, this, [this](){ logratio_tokeep = ui->oddsToKeep->text().toDouble(); });
    connect(ui->oddsToSolve, &QLineEdit::textChanged, this, [this](){ logratio_tosolve = ui->oddsToSolve->text().toDouble(); });
    connect(ui->oddsToTune, &QLineEdit::textChanged, this, [this](){ logratio_totune = ui->oddsToTune->text().toDouble(); });

    connect(ui->logToFile, &QCheckBox::stateChanged, this, [this](){ logToFile = ui->logToFile->isChecked(); });
    connect(ui->logFile, &QLineEdit::textChanged, this, [this](){ logFile = ui->logFile->text(); });
    connect(ui->logLevel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){ logLevel = index; });

    connect(ui->indexPaths, &QListWidget::itemChanged, this, [this](QListWidgetItem * item){
        int row = ui->indexPaths->row(item);
        indexFilePaths.removeAt(row);
        indexFilePaths.insert(row,item->text());
    });
    connect(ui->addIndexPath, &QPushButton::clicked, this, [this](){
        QListWidgetItem *item = new QListWidgetItem("type path here");
        item->setFlags(item->flags () | Qt::ItemIsEditable);
        ui->indexPaths->addItem(item);
        indexFilePaths.append("");
    });

    resetOptionsToDefaults();

    setupResultsTable();

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;

    setWindowTitle("SexySolver Internal Sextractor and Astrometry.net Based Solver");

}

void MainWindow::resetOptionsToDefaults()
{
    ExternalSextractorSolver extTemp(stats, m_ImageBuffer, this);

    sextractorBinaryPath = extTemp.sextractorBinaryPath;
    confPath = extTemp.confPath;
    solverPath = extTemp.solverPath;
    wcsPath = extTemp.wcsPath;
    cleanupTemporaryFiles = extTemp.cleanupTemporaryFiles;

        ui->sextractorPath->setText(sextractorBinaryPath);
        ui->configFilePath->setText(confPath);
        ui->solverPath->setText(solverPath);
        ui->wcsPath->setText(wcsPath);
        ui->cleanupTemp->setChecked(cleanupTemporaryFiles);

    SexySolver temp(stats, m_ImageBuffer, this);

    //Basic Settings
    tempPath = QDir::tempPath();

        ui->showStars->setChecked(true);
        ui->tempPath->setText(tempPath);

    //Sextractor Settings
    calculateHFR = temp.calculateHFR;
    apertureShape = temp.apertureShape;
    kron_fact = temp.kron_fact;
    subpix = temp.subpix;
    r_min = temp.r_min;

        ui->calculateHFR->setChecked(calculateHFR);
        ui->apertureShape->setCurrentIndex(apertureShape);
        ui->kron_fact->setText(QString::number(kron_fact));
        ui->subpix->setText(QString::number(subpix));
        ui->r_min->setText(QString::number(r_min));

    magzero = temp.magzero;
    minarea = temp.minarea;
    deblend_thresh = temp.deblend_thresh;
    deblend_contrast = temp.deblend_contrast;
    clean = temp.clean;
    clean_param = temp.clean_param;
    fwhm = temp.fwhm;

        ui->magzero->setText(QString::number(magzero));
        ui->minarea->setText(QString::number(minarea));
        ui->deblend_thresh->setText(QString::number(deblend_thresh));
        ui->deblend_contrast->setText(QString::number(deblend_contrast));
        ui->cleanCheckBox->setChecked(clean == 1);
        ui->clean_param->setText(QString::number(clean_param));
        ui->fwhm->setText(QString::number(fwhm));

    //Star Filter Settings
    maxEllipse = temp.maxEllipse;
    removeBrightest = temp.removeBrightest;
    removeDimmest = temp.removeDimmest;
    saturationLimit = temp.saturationLimit;

        ui->maxEllipse->setText(QString::number(maxEllipse));
        ui->brightestPercent->setText(QString::number(removeBrightest));
        ui->dimmestPercent->setText(QString::number(removeDimmest));
        ui->saturationLimit->setText(QString::number(saturationLimit));

    //Astrometry Settings
    inParallel = temp.inParallel;
    solverTimeLimit = temp.solverTimeLimit;
    minwidth = temp.minwidth;
    maxwidth = temp.maxwidth;
    radius = temp.search_radius;

        ui->inParallel->setChecked(inParallel);
        ui->solverTimeLimit->setText(QString::number(solverTimeLimit));
        ui->minWidth->setText(QString::number(minwidth));
        ui->maxWidth->setText(QString::number(maxwidth));
        ui->radius->setText(QString::number(radius));

    clearAstrometrySettings(); //Resets the Position and Scale settings

    //Astrometry Log Ratio Settings
    logratio_tokeep = temp.logratio_tokeep;
    logratio_tosolve = temp.logratio_tosolve;
    logratio_totune = temp.logratio_totune;

        ui->oddsToKeep->setText(QString::number(logratio_tokeep));
        ui->oddsToSolve->setText(QString::number(logratio_tosolve));
        ui->oddsToTune->setText(QString::number(logratio_totune));

    //Astrometry Logging Settings
    logFile = QDir::tempPath() + "/AstrometryLog.txt";
    logToFile = temp.logToFile;
    logLevel = temp.logLevel;

        ui->logFile->setText(logFile);
        ui->logToFile->setChecked(logToFile);
        ui->logLevel->setCurrentIndex(logLevel);

    //Astrometry Index File Paths to Search
    indexFilePaths.clear();
    ui->indexPaths->clear();


#if defined(Q_OS_OSX)
    //Mac Default location
    indexFilePaths.append(QDir::homePath() + "/Library/Application Support/Astrometry");
#elif defined(Q_OS_LINUX)
    //Linux Default Location
    indexFilePaths.append("/usr/share/astrometry/");
    //Linux Local KStars Location
    QString localAstroPath = QDir::homePath() + "/.local/share/kstars/astrometry/";
    if(QFileInfo(localAstroPath).exists())
        indexFilePaths.append(localAstroPath);
#elif defined(_WIN32)
    //A Windows Location
    QString localAstroPath = "C:/Astrometry";
    if(QFileInfo(localAstroPath).exists())
        indexFilePaths.append(localAstroPath);
#endif

    foreach(QString pathName, indexFilePaths)
    {
        QListWidgetItem *item = new QListWidgetItem(pathName);
        item->setFlags(item->flags () | Qt::ItemIsEditable);
        ui->indexPaths->addItem(item);
    }

}

MainWindow::~MainWindow()
{
    delete ui;
}

//This method clears the tables and displays when the user requests it.
void MainWindow::clearAll()
{
    ui->logDisplay->clear();
    ui->starList->clearContents();
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





//The following methods are meant for starting the sextractor and image solving.
//The methods run when the buttons are clicked.  They call the methods inside SexySolver and ExternalSextractorSovler

//This method responds when the user clicks the Sextract Button
bool MainWindow::sextractImage()
{
    if(!prepareForProcesses())
        return false;

    //If the Use SexySolver CheckBox is checked, use the internal method instead.
    if(ui->useSexySolver->isChecked())
        return sextractInternally();

    #ifdef _WIN32
    logOutput("An External Sextractor is not easily installed on Windows, try the Internal Sextractor please.");
    return false;
    #endif

    sextractExternally();
    return true;
}

//This method runs when the user clicks the Sextract and Solve buttton
bool MainWindow::solveImage()
{
    if(!prepareForProcesses())
        return false;

    //If the Use SexySolver CheckBox is checked, use the internal method instead.
    if(ui->useSexySolver->isChecked())
        return sextractAndSolveInternally();

    sextractAndSolveExternally();
    return true;
}

//This method runs when the user clicks the Classic Solve button
//It is meant to run the traditional solve KStars used to do using python and astrometry.net
bool MainWindow::classicSolve()
{
    if(!prepareForProcesses())
        return false;

    setupExternalSextractorSolver();
    setSolverSettings();

    connect(sexySolver, &SexySolver::finished, this, &MainWindow::classicSolverComplete);

    solverTimer.start();
    extSolver->classicSolve();

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

    solverTimer.start();
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

    solverTimer.start();
    extSolver->sextractAndSolve();
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

    solverTimer.start();
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

      solverTimer.start();
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
    extSolver->fileToSolve = fileToSolve;
    extSolver->dirPath = dirPath;
    extSolver->basePath = tempPath;
    extSolver->sextractorBinaryPath = sextractorBinaryPath;
    extSolver->confPath = confPath;
    extSolver->solverPath = solverPath;
    extSolver->wcsPath = wcsPath;
    extSolver->cleanupTemporaryFiles = cleanupTemporaryFiles;
}

//This sets up the Internal SexySolver and sets settings specific to it
void MainWindow::setupInternalSexySolver()
{
    //Creates the SexySolver
    sexySolver = new SexySolver(stats ,m_ImageBuffer, this);

    //Connects the SexySolver for logging
    connect(sexySolver, &SexySolver::logNeedsUpdating, this, &MainWindow::logOutput, Qt::QueuedConnection);

}

//This sets all the settings for either the internal or external sextractor
//based on the requested settings in the mainwindow interface.
//If you are implementing the SexySolver Library in your progra, you may choose to change some or all of these settings or use the defaults.
void MainWindow::setSextractorSettings()
{ 
    //These are to pass the parameters to the internal sextractor
    sexySolver->calculateHFR = calculateHFR;
    sexySolver->apertureShape = apertureShape;
    sexySolver->kron_fact = kron_fact;
    sexySolver->subpix = subpix ;
    sexySolver->r_min = r_min;
    sexySolver->inflags = inflags;
    sexySolver->magzero = magzero;
    sexySolver->minarea = minarea;
    sexySolver->deblend_thresh = deblend_thresh;
    sexySolver->deblend_contrast = deblend_contrast;
    sexySolver->clean = clean;
    sexySolver->clean_param = clean_param;
    sexySolver->createConvFilterFromFWHM(fwhm);

    //Star Filter Settings
    sexySolver->removeBrightest = removeBrightest;
    sexySolver->removeDimmest = removeDimmest;
    sexySolver->maxEllipse = maxEllipse;
    sexySolver->saturationLimit = saturationLimit;
}

//This sets all the settings for either the internal or external astrometry.net solver
//based on the requested settings in the mainwindow interface.
//If you are implementing the SexySolver Library in your progra, you may choose to change some or all of these settings or use the defaults.
void MainWindow::setSolverSettings()
{
    //Settings that usually get set by the config file
    sexySolver->setIndexFolderPaths(indexFilePaths);
    sexySolver->maxwidth = maxwidth;
    sexySolver->minwidth = minwidth;
    sexySolver->inParallel = inParallel;
    sexySolver->solverTimeLimit = solverTimeLimit;

    //Setting the scale settings
    if(use_scale)
        sexySolver->setSearchScale(fov_low, fov_high, units);

    //Setting the initial search location settings
    if(use_position)
        sexySolver->setSearchPosition(ra, dec, radius);

    //Setting the settings to know when to stop or keep searching for solutions
    sexySolver->logratio_tokeep = logratio_tokeep;
    sexySolver->logratio_totune = logratio_totune;
    sexySolver->logratio_tosolve = logratio_tosolve;

    //Setting the logging settings
    sexySolver->logToFile = logToFile;
    sexySolver->logFile = logFile;
    sexySolver->logLevel = logLevel;
}

//This runs when the sextractor is complete.
//It reports the time taken, prints a message, loads the sextraction stars to the startable, and adds the sextraction stats to the results table.
bool MainWindow::sextractorComplete(int error)
{
    if(error == 0)
    {
        elapsed = solverTimer.elapsed() / 1000.0;
        stars = sexySolver->getStarList();
        displayTable();
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString("Successfully sextracted %1 stars.").arg(stars.size()));
        logOutput(QString("Sextraction took a total of: %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

        addSextractionToTable();
        return true;
    }
    else
    {
        elapsed = solverTimer.elapsed() / 1000.0;
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString("Sextractor failed after %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        return false;
    }
}

//This runs when the solver is complete.  It reports the time taken, prints a message, and adds the solution to the results table.
bool MainWindow::solverComplete(int error)
{
    if(error == 0)
    {
        elapsed = solverTimer.elapsed() / 1000.0;
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString("Sextraction and Solving took a total of: %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        addSolutionToTable(sexySolver->solution);
        return true;
    }
    else
    {
        elapsed = solverTimer.elapsed() / 1000.0;
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString("Solver failed after %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        return false;
    }
}

//This runs when the solver is complete.  It reports the time taken, prints a message, and adds the solution to the results table.
bool MainWindow::classicSolverComplete(int error)
{
    if(error == 0)
    {
        elapsed = solverTimer.elapsed() / 1000.0;
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString("Classic Solving took a total of: %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        addClassicSolutionToTable(sexySolver->solution);
        return true;
    }
    else
    {
        elapsed = solverTimer.elapsed() / 1000.0;
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString("Solver failed after %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        return false;
    }
}

//This method will attempt to abort the sextractor, sovler, and any other processes currently being run, no matter which type
void MainWindow::abort()
{
    logOutput("Aborting. . .");
    if(!sexySolver.isNull())
        sexySolver->abort();
    if(!extSolver.isNull())
        extSolver->abort();
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
    QString newFileURL=tempPath + QDir::separator() + fileInfo.fileName().remove(" ");
    QFile::copy(fileURL, newFileURL);
    QFileInfo newFileInfo(newFileURL);
    dirPath = fileInfo.absolutePath();
    fileToSolve = newFileURL;

    clearAstrometrySettings();

    bool loadSuccess;
    if(newFileInfo.suffix()=="fits"||newFileInfo.suffix()=="fit")
        loadSuccess = loadFits();
    else
        loadSuccess = loadOtherFormat();

    if(loadSuccess)
    {
        imageLoaded = true;
        ui->starList->clear();
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
    if (fits_open_diskfile(&fptr, fileToSolve.toLatin1(), READONLY, &status))
    {
        logOutput(QString("Error opening fits file %1").arg(fileToSolve));
        return false;
    }
    else
        stats.size = QFile(fileToSolve).size();

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
    QImageReader fileReader(fileToSolve.toLatin1());

    if (QImageReader::supportedImageFormats().contains(fileReader.format()) == false)
    {
        logOutput("Failed to convert" + fileToSolve + "to FITS since format, " + fileReader.format() + ", is not supported in Qt");
        return false;
    }

    QString errMessage;
    QImage imageFromFile;
    if(!imageFromFile.load(fileToSolve.toLatin1()))
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
                QString text = QString("Star: %1, x: %2, y: %3, mag: %4, flux: %5, HFR: %6").arg(i + 1).arg(star.x).arg(star.y).arg(star.mag).arg(star.flux).arg(star.HFR);
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
            ui->starList->selectRow(i);
    }
}


//THis method responds to row selections in the table and higlights the star you select in the image
void MainWindow::starClickedInTable()
{
    if(ui->starList->selectedItems().count() > 0)
    {
        QTableWidgetItem *currentItem = ui->starList->selectedItems().first();
        selectedStar = ui->starList->row(currentItem);
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
    QTableWidget *table = ui->starList;
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
    QTableWidget *table = ui->starList;

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

    int status = 0, fits_ccd_width, fits_ccd_height, fits_binx = 1, fits_biny = 1;
    char comment[128], error_status[512];
    fitsfile *fptr = nullptr;

    double fits_fov_x, fits_fov_y, fov_lower, fov_upper, fits_ccd_hor_pixel = -1,
           fits_ccd_ver_pixel = -1, fits_focal_length = -1;

    status = 0;

    // Use open diskfile as it does not use extended file names which has problems opening
    // files with [ ] or ( ) in their names.
    if (fits_open_diskfile(&fptr, fileToSolve.toLatin1(), READONLY, &status))
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
        use_position = true;
        ui->ra->setText(QString::number(ra));
        ui->dec->setText(QString::number(dec));

    }

    status = 0;
    double pixelScale = 0;
    // If we have pixel scale in arcsecs per pixel then lets use that directly
    // instead of calculating it from FOCAL length and other information
    if (fits_read_key(fptr, TDOUBLE, "SCALE", &pixelScale, comment, &status) == 0)
    {
        fov_low  = 0.9 * pixelScale;
        fov_high = 1.1 * pixelScale;
        units = "app";
        use_scale = true;

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
    fov_low  = fov_lower;
    fov_high = fov_upper;
    units = "aw";
    use_scale = true;

    ui->scale_low->setText(QString::number(fov_low));
    ui->scale_high->setText(QString::number(fov_high));
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
    //These are in the order that they will appear in the table.

    addColumnToTable(ui->resultsTable,"Time");
    addColumnToTable(ui->resultsTable,"Int?");
    addColumnToTable(ui->resultsTable,"Command");
    addColumnToTable(ui->resultsTable,"Stars");
    //Sextractor Parameters
    addColumnToTable(ui->resultsTable,"doHFR");
    addColumnToTable(ui->resultsTable,"Shape");
    addColumnToTable(ui->resultsTable,"Kron");
    addColumnToTable(ui->resultsTable,"Subpix");
    addColumnToTable(ui->resultsTable,"r_min");
    addColumnToTable(ui->resultsTable,"minarea");
    addColumnToTable(ui->resultsTable,"d_thresh");
    addColumnToTable(ui->resultsTable,"d_cont");
    addColumnToTable(ui->resultsTable,"clean");
    addColumnToTable(ui->resultsTable,"clean param");
    addColumnToTable(ui->resultsTable,"fwhm");
    //Star Filtering Parameters
    addColumnToTable(ui->resultsTable,"Max Ell");
    addColumnToTable(ui->resultsTable,"Cut Bri");
    addColumnToTable(ui->resultsTable,"Cut Dim");
    addColumnToTable(ui->resultsTable,"Sat Lim");
    //Astrometry Parameters
    addColumnToTable(ui->resultsTable,"Pos?");
    addColumnToTable(ui->resultsTable,"Scale?");
    addColumnToTable(ui->resultsTable,"Resort?");
    //Results
    addColumnToTable(ui->resultsTable,"RA");
    addColumnToTable(ui->resultsTable,"DEC");
    addColumnToTable(ui->resultsTable,"Orientation");
    addColumnToTable(ui->resultsTable,"Field Width");
    addColumnToTable(ui->resultsTable,"Field Height");
    addColumnToTable(ui->resultsTable,"Field");

    ui->resultsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    updateHiddenResultsTableColumns();
}

//This adds a Sextraction to the Results Table
//To add, remove, or change the way certain columns are filled when a sextraction is finished, edit them here.
void MainWindow::addSextractionToTable()
{
    QTableWidget *table = ui->resultsTable;
    table->insertRow(table->rowCount());

    setItemInColumn(table, "Time", QString::number(elapsed));
    if(extSolver.isNull())
        setItemInColumn(ui->resultsTable, "Int?", "Internal");
    else
        setItemInColumn(ui->resultsTable, "Int?", "External");
    setItemInColumn(ui->resultsTable, "Command", "Sextract");
    setItemInColumn(ui->resultsTable, "Stars", QString::number(sexySolver->getNumStarsFound()));
    //Sextractor Parameters
    setItemInColumn(ui->resultsTable,"doHFR", QVariant(sexySolver->calculateHFR).toString());
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

    setItemInColumn(ui->resultsTable,"Shape", shapeName);
    setItemInColumn(ui->resultsTable,"Kron", QString::number(sexySolver->kron_fact));
    setItemInColumn(ui->resultsTable,"Subpix", QString::number(sexySolver->subpix));
    setItemInColumn(ui->resultsTable,"r_min", QString::number(sexySolver->r_min));
    setItemInColumn(ui->resultsTable,"minarea", QString::number(sexySolver->minarea));
    setItemInColumn(ui->resultsTable,"d_thresh", QString::number(sexySolver->deblend_thresh));
    setItemInColumn(ui->resultsTable,"d_cont", QString::number(sexySolver->deblend_contrast));
    setItemInColumn(ui->resultsTable,"clean", QString::number(sexySolver->clean));
    setItemInColumn(ui->resultsTable,"clean param", QString::number(sexySolver->clean_param));
    setItemInColumn(ui->resultsTable,"fwhm", QString::number(sexySolver->fwhm));
    setItemInColumn(ui->resultsTable, "Field", ui->fileNameDisplay->text());

    //StarFilter Parameters
    setItemInColumn(ui->resultsTable,"Max Ell", QString::number(sexySolver->maxEllipse));
    setItemInColumn(ui->resultsTable,"Cut Bri", QString::number(sexySolver->removeBrightest));
    setItemInColumn(ui->resultsTable,"Cut Dim", QString::number(sexySolver->removeDimmest));
    setItemInColumn(ui->resultsTable,"Sat Lim", QString::number(sexySolver->saturationLimit));

}

//This adds a solution to the Results Table
//To add, remove, or change the way certain columns are filled when a solve is finished, edit them here.
void MainWindow::addSolutionToTable(Solution solution)
{
    addSextractionToTable();

    QTableWidget *table = ui->resultsTable;

    setItemInColumn(ui->resultsTable, "Command", "Sextract and Solve");

    //Astrometry Parameters
    setItemInColumn(ui->resultsTable, "Pos?", QVariant(sexySolver->use_position).toString());
    setItemInColumn(ui->resultsTable, "Scale?", QVariant(sexySolver->use_scale).toString());
    setItemInColumn(ui->resultsTable, "Resort?", QVariant(sexySolver->resort).toString());

    //Results
    setItemInColumn(table, "RA", solution.rastr);
    setItemInColumn(table, "DEC", solution.decstr);
    setItemInColumn(table, "Orientation", QString::number(solution.orientation));
    setItemInColumn(table, "Field Width", QString::number(solution.fieldWidth));
    setItemInColumn(table, "Field Height", QString::number(solution.fieldHeight));
    setItemInColumn(table, "Field", ui->fileNameDisplay->text());
}

//This adds a solution to the Results Table
//To add, remove, or change the way certain columns are filled when a solve is finished, edit them here.
void MainWindow::addClassicSolutionToTable(Solution solution)
{
    QTableWidget *table = ui->resultsTable;
    table->insertRow(table->rowCount());

    setItemInColumn(table, "Time", QString::number(elapsed));
    setItemInColumn(ui->resultsTable, "Int?", "External");
    setItemInColumn(ui->resultsTable, "Command", "Classic Solve");

    //Astrometry Parameters
    setItemInColumn(ui->resultsTable, "Pos?", QVariant(sexySolver->use_position).toString());
    setItemInColumn(ui->resultsTable, "Scale?", QVariant(sexySolver->use_scale).toString());
    setItemInColumn(ui->resultsTable, "Resort?", QVariant(sexySolver->resort).toString());

    //Results
    setItemInColumn(table, "RA", solution.rastr);
    setItemInColumn(table, "DEC", solution.decstr);
    setItemInColumn(table, "Orientation", QString::number(solution.orientation));
    setItemInColumn(table, "Field Width", QString::number(solution.fieldWidth));
    setItemInColumn(table, "Field Height", QString::number(solution.fieldHeight));
    setItemInColumn(table, "Field", ui->fileNameDisplay->text());
}

//I wrote this method to hide certain columns in the Results Table if the user wants to reduce clutter in the table.
void MainWindow::updateHiddenResultsTableColumns()
{
    //Sextractor Params
    setColumnHidden(ui->resultsTable,"doHFR", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"Shape", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"Kron", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"Subpix", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"r_min", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"minarea", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"d_thresh", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"d_cont", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"clean", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"clean param", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"fwhm", !showSextractorParams);
    //Star Filtering Parameters
    setColumnHidden(ui->resultsTable,"Max Ell", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"Cut Bri", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"Cut Dim", !showSextractorParams);
    setColumnHidden(ui->resultsTable,"Sat Lim", !showSextractorParams);
    //Astrometry Parameters
    setColumnHidden(ui->resultsTable,"Pos?", !showAstrometryParams);
    setColumnHidden(ui->resultsTable,"Scale?", !showAstrometryParams);
    setColumnHidden(ui->resultsTable,"Resort?", !showAstrometryParams);
    //Results
    setColumnHidden(ui->resultsTable,"RA", !showSolutionDetails);
    setColumnHidden(ui->resultsTable,"DEC", !showSolutionDetails);
    setColumnHidden(ui->resultsTable,"Orientation", !showSolutionDetails);
    setColumnHidden(ui->resultsTable,"Field Width", !showSolutionDetails);
    setColumnHidden(ui->resultsTable,"Field Height", !showSolutionDetails);
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











