/*  MainWindow for StellarSolver Tester Application, developed by Robert Lancaster, 2020

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
#include <QDesktopServices>

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

#include <QShortcut>
#include <QThread>
#include <QInputDialog>
#include <QtConcurrent>
#include <QToolTip>
#include <QtGlobal>
#include "version.h"
#include "testerutils/fileio.h"

MainWindow::MainWindow() :
    QMainWindow(),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);


    this->show();

    //The Options at the top of the Window
    connect(ui->ImageLoad, &QAbstractButton::clicked, this, &MainWindow::imageLoad );
    ui->ImageLoad->setToolTip("Loads an Image into the Viewer");
    connect(ui->zoomIn, &QAbstractButton::clicked, this, &MainWindow::zoomIn );
    ui->zoomIn->setToolTip("Zooms In on the Image");
    connect(ui->zoomOut, &QAbstractButton::clicked, this, &MainWindow::zoomOut );
    ui->zoomOut->setToolTip("Zooms Out on the Image");
    connect(ui->AutoScale, &QAbstractButton::clicked, this, &MainWindow::autoScale );
    ui->AutoScale->setToolTip("Rescales the image based on the available space");

    // Keyboard shortcuts -- if you add more, remember to add to the helpPopup() method below.
    QShortcut *s = new QShortcut(QKeySequence(QKeySequence::ZoomIn), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::zoomIn);
    s = new QShortcut(QKeySequence(QKeySequence::ZoomOut), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::zoomOut);
    s = new QShortcut(QKeySequence(tr("Ctrl+0")), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::autoScale);
    s = new QShortcut(QKeySequence(QKeySequence::Open), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::imageLoad );
    s = new QShortcut(QKeySequence(QKeySequence::MoveToNextChar), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::panRight);
    s = new QShortcut(QKeySequence(QKeySequence::MoveToPreviousChar), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::panLeft);
    s = new QShortcut(QKeySequence(QKeySequence::MoveToNextLine), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::panDown);
    s = new QShortcut(QKeySequence(QKeySequence::MoveToPreviousLine), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::panUp);
    s = new QShortcut(QKeySequence(QKeySequence::Quit), ui->Image);
    connect(s, &QShortcut::activated, this, &QCoreApplication::quit);
    s = new QShortcut(QKeySequence(tr("Ctrl+l")), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::toggleLogDisplay);
    s = new QShortcut(QKeySequence(QKeySequence::FullScreen), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::toggleFullScreen);
    s = new QShortcut(QKeySequence(QKeySequence::HelpContents), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::helpPopup);
    s = new QShortcut(QKeySequence("?"), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::helpPopup);
    s = new QShortcut(QKeySequence("h"), ui->Image);
    connect(s, &QShortcut::activated, this, &MainWindow::helpPopup);

    //The Options at the bottom of the Window
    ui->trials->setToolTip("The number of times to Sextract or Solve to get an average time that it takes.");
    connect(ui->startExtraction, &QAbstractButton::clicked, this, &MainWindow::extractButtonClicked );
    connect(ui->startSolving, &QAbstractButton::clicked, this, &MainWindow::solveButtonClicked );
    connect(ui->editExtractorProfile, &QAbstractButton::clicked, this, [this]()
    {
        ui->optionsProfile->setCurrentIndex(ui->extractionProfile->currentIndex());
        ui->optionsTab->setCurrentIndex(1);
        ui->extractionProfile->setCurrentIndex(0);
    });
    connect(ui->editSolverProfile, &QAbstractButton::clicked, this, [this]()
    {
        ui->optionsProfile->setCurrentIndex(ui->solverProfile->currentIndex());
        ui->optionsTab->setCurrentIndex(1);
        ui->solverProfile->setCurrentIndex(0);
    });
    connect(ui->Abort, &QAbstractButton::clicked, this, &MainWindow::abort );
    ui->Abort->setToolTip("Aborts the current process if one is running.");
    connect(ui->ClearStars, &QAbstractButton::clicked, this, &MainWindow::clearStars );
    ui->ClearStars->setToolTip("Clears the star table and the stars from the image");

    connect(ui->ClearResults, &QAbstractButton::clicked, this, &MainWindow::clearResults );
    ui->ClearResults->setToolTip("Clears the Results Table");

    //The Options for the StellarSolver Options
    ui->optionsProfile->setToolTip("The Options Profile currently in the options box. Selecting a profile will reset all the Star Extractor and Solver settings to that profile.");
    connect(ui->optionsProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::loadOptionsProfile);
    ui->addOptionProfile->setToolTip("Adds the current options in the left option pane to a new profile");
    connect(ui->addOptionProfile, &QAbstractButton::clicked, this, [this]()
    {
        bool ok;
        QString name = QInputDialog::getText(this, tr("New Options Profile"),
                                             tr("What would you like your profile to be called?"), QLineEdit::Normal,
                                             "", &ok);
        if (ok && !name.isEmpty())
        {
            optionsAreSaved = true;
            SSolver::Parameters params = getSettingsFromUI();
            params.listName = name;
            ui->optionsProfile->setCurrentIndex(0); //So we don't trigger any loading of any other profiles
            optionsList.append(params);
            ui->optionsProfile->addItem(name);
            ui->extractionProfile->addItem(name);
            ui->solverProfile->addItem(name);
            ui->optionsProfile->setCurrentText(name);
        }
    });
    ui->removeOptionProfile->setToolTip("Removes the selected profile from the list of profiles");
    connect(ui->removeOptionProfile, &QAbstractButton::clicked, this, [this]()
    {
        int item = ui->optionsProfile->currentIndex();
        if(item < 1)
        {
            QMessageBox::critical(nullptr, "Message", "You can't delete this profile");
            return;
        }
        ui->optionsProfile->setCurrentIndex(0); //So we don't trigger any loading of any other profiles
        ui->optionsProfile->removeItem(item);
        ui->extractionProfile->removeItem(item);
        ui->solverProfile->removeItem(item);
        optionsList.removeAt(item - 1);

    });
    ui->saveSettings->setToolTip("Saves a file with Options Profiles to a desired location");
    connect(ui->saveSettings, &QPushButton::clicked, this, &MainWindow::saveOptionsProfiles);
    ui->loadSettings->setToolTip("Loads a file with Options Profiles from a saved location");
    connect(ui->loadSettings, &QPushButton::clicked, this, &MainWindow::loadOptionsProfiles);

    QWidget *extractorOptions = ui->optionsBox->widget(0);
    QWidget *starFilterOptions = ui->optionsBox->widget(1);
    QWidget *astrometryOptions = ui->optionsBox->widget(2);

    QList<QLineEdit *> lines;
    lines = extractorOptions->findChildren<QLineEdit *>();
    lines.append(starFilterOptions->findChildren<QLineEdit *>());
    lines.append(astrometryOptions->findChildren<QLineEdit *>());
    foreach(QLineEdit *line, lines)
        connect(line, &QLineEdit::textEdited, this, &MainWindow::settingJustChanged);

    QList<QCheckBox *> checks;
    checks = extractorOptions->findChildren<QCheckBox *>();
    checks.append(starFilterOptions->findChildren<QCheckBox *>());
    checks.append(astrometryOptions->findChildren<QCheckBox *>());
    foreach(QCheckBox *check, checks)
        connect(check, &QCheckBox::stateChanged, this, &MainWindow::settingJustChanged);

    QList<QComboBox *> combos;
    combos = extractorOptions->findChildren<QComboBox *>();
    combos.append(starFilterOptions->findChildren<QComboBox *>());
    combos.append(astrometryOptions->findChildren<QComboBox *>());
    foreach(QComboBox *combo, combos)
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::settingJustChanged);

    QList<QSpinBox *> spins;
    spins = extractorOptions->findChildren<QSpinBox *>();
    spins.append(starFilterOptions->findChildren<QSpinBox *>());
    spins.append(astrometryOptions->findChildren<QSpinBox *>());
    foreach(QSpinBox *spin, spins)
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::settingJustChanged);


    //Hides the panels into the sides and bottom
    ui->vertSplitter->setSizes(QList<int>() << ui->vertSplitter->height() << 0 );
    ui->horSplitter->setSizes(QList<int>() << 100 << ui->horSplitter->width() / 2  << 0 );

    //Settings for the External SExtractor and Solver
    ui->configFilePath->setToolTip("The path to the Astrometry.cfg file used by astrometry.net for configuration.");
    ui->sextractorPath->setToolTip("The path to the external SExtractor executable");
    ui->solverPath->setToolTip("The path to the external Astrometry.net solve-field executable");
    ui->astapPath->setToolTip("The path to the external ASTAP executable");
    ui->watneyPath->setToolTip("The path to the external Watney Astrometry Solver executable");
    ui->basePath->setToolTip("The base path where SExtractor and astrometry.net temporary files are saved on your computer");
    ui->openTemp->setToolTip("Opens the directory (above) to where the external solvers save their files");
    ui->wcsPath->setToolTip("The path to wcsinfo for the external Astrometry.net");
    ui->cleanupTemp->setToolTip("This option allows the program to clean up temporary files created when running various processes");
    ui->generateAstrometryConfig->setToolTip("Determines whether to generate an astrometry.cfg file based on the options in the options panel or to use the external config file above.");
    ui->onlySendFITSFiles->setToolTip("This option only applies to the local external solvers using their own builtin star extractor. If it is off, the file will be sent in its native form.  If it is on, it will be converted to FITS.  This is good for avoiding Python usage.");
    ui->onlineServer->setToolTip("This is the server that StellarSolver will use for the Online solves.  This will typically be nova.astrometry.net, but it could also be an ANSVR server or a custom one.");
    ui->apiKey->setToolTip("This is the api key used for astrometry.net online.  You can enter your own and then have access to your solves later.");
    connect(ui->openTemp, &QAbstractButton::clicked, this, [this]()
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(ui->basePath->text()));
    });
    //StellarSolver Tester Options
    connect(ui->showStars, &QAbstractButton::clicked, this, &MainWindow::updateImage );
    ui->setSubFrame->setToolTip("Sets or clears the Subframe for Sextraction if desired");
    connect(ui->setSubFrame, &QAbstractButton::clicked, this, &MainWindow::setSubframe );

    connect(ui->setPathsAutomatically, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int num)
    {

        ExternalProgramPaths paths;
        switch(num)
        {
            case 0:
                paths = ExternalExtractorSolver::getLinuxDefaultPaths();
                break;
            case 1:
                paths = ExternalExtractorSolver::getLinuxInternalPaths();
                break;
            case 2:
                paths = ExternalExtractorSolver::getMacHomebrewPaths();
                break;
            case 3:
                paths = ExternalExtractorSolver::getWinANSVRPaths();
                break;
            case 4:
                paths = ExternalExtractorSolver::getWinCygwinPaths();
                break;
            default:
                paths = ExternalExtractorSolver::getLinuxDefaultPaths();
                break;
        }

        ui->sextractorPath->setText(paths.sextractorBinaryPath);
        ui->configFilePath->setText(paths.confPath);
        ui->solverPath->setText(paths.solverPath);
        ui->astapPath->setText(paths.astapBinaryPath);
        ui->watneyPath->setText(paths.watneyBinaryPath);
        ui->wcsPath->setText(paths.wcsPath);
    });
    ui->setPathsAutomatically->setToolTip("This allows you to select the default values of typical configurations of paths to external files/programs on different systems from a dropdown");

    //Star Extractor Settings

    ui->apertureShape->setToolTip("This selects whether to instruct the Star Extractor to use Ellipses or Circles for flux calculations");
    ui->kron_fact->setToolTip("This sets the Kron Factor for use with the kron radius for flux calculations.");
    ui->subpix->setToolTip("The subpix setting.  The instructions say to make it 5");
    ui->r_min->setToolTip("The minimum radius for stars for flux calculations.");
    //no inflags???;
    ui->magzero->setToolTip("This is the 'zero' magnitude used for settting the magnitude scale for the stars in the image during sextraction.");
    ui->minarea->setToolTip("This is the minimum area in pixels for a star detection, smaller stars are ignored.");
    ui->thresh_multiple->setToolTip("Add the multiple times the rms background level to the detection threshold.");
    ui->thresh_offset->setToolTip("Add this offset to the detection threshold");
    ui->deblend_thresh->setToolTip("The number of thresholds the intensity range is divided up into");
    ui->deblend_contrast->setToolTip("The percentage of flux a separate peak must # have to be considered a separate object");

    ui->cleanCheckBox->setToolTip("Attempts to 'clean' the image to remove artifacts caused by bright objects");
    ui->clean_param->setToolTip("The cleaning parameter, not sure what it does.");

    //The Convolution Filter Parameters
    ui->convFilterType->setToolTip("This allows you to choose the type of Convolution Filter Generated");
    ui->fwhm->setToolTip("This allows you to set the FWHM in Pixels for Convolution Filter Generation");

    ui->showConv->setToolTip("Loads the convolution filter into a window for viewing");

    ui->partition->setToolTip("Whether or not to partition the image during SEP operations for Internal SEP.  This can greatly speed up star extraction, but at the cost of possibly missing some objects.  For solving, Focusing, and guiding operations, this doesn't matter, but for doing science, you might want to turn it off.");

    connect(ui->showConv,&QPushButton::clicked,this,[this](){
        if(!convInspector)
        {
            convInspector = new QDialog(this);
            convInspector->setWindowTitle("Convolution Filter Inspector");
            convInspector->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
            convTable = new QTableWidget(this);
            QGridLayout *layout = new QGridLayout(this);
            convTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            convTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            convTable->horizontalHeader()->hide();
            convTable->verticalHeader()->hide();
            convInspector->setLayout(layout);
            layout->addWidget(convTable);
        }
        convInspector->show();
        reloadConvTable();
    });

    connect(ui->convFilterType,QOverload<int>::of(&QComboBox::currentIndexChanged),this, &MainWindow::reloadConvTable);
    connect(ui->fwhm,QOverload<int>::of(&QSpinBox::valueChanged),this, &MainWindow::reloadConvTable);

    //Star Filter Settings
    connect(ui->resortQT, &QCheckBox::stateChanged, this, [this]()
    {
        ui->resort->setChecked(ui->resortQT->isChecked());
    });
    ui->resortQT->setToolTip("This resorts the stars based on magnitude.  It MUST be checked for the next couple of filters to be enabled.");
    ui->maxSize->setToolTip("This is the maximum diameter of stars to include in pixels");
    ui->minSize->setToolTip("This is the minimum diameter of stars to include in pixels");
    ui->maxEllipse->setToolTip("Stars are typically round, this filter divides stars' semi major and minor axes and rejects stars with distorted shapes greater than this number (1 is perfectly round)");
    ui->initialKeep->setToolTip("Keep just this number of stars in the list based upon star size.  They will be the biggest in the list.  If there are less than this number, they will all be kept.  This filter is primarily for HFR operations, so they take less time.");
    ui->keepNum->setToolTip("Keep just this number of star in the list based on magnitude.  They will be the brightest in the list.  If there are less than this number, they will all be kept.  This filter is mainly for the solver, so that it takes less time.");
    ui->brightestPercent->setToolTip("Removes the brightest % of stars from the image");
    ui->dimmestPercent->setToolTip("Removes the dimmest % of stars from the image");
    ui->saturationLimit->setToolTip("Removes stars above a certain % of the saturation limit of an image of this data type");

    //Astrometry Settings
    ui->inParallel->setToolTip("Loads the Astrometry index files in parallel.  This can speed it up, but uses more resources");
    ui->multiAlgo->setToolTip("Allows solving in multiple threads or multiple cores with several algorithms");
    ui->solverTimeLimit->setToolTip("This is the maximum time the Astrometry.net solver should spend on the image before giving up");
    ui->minWidth->setToolTip("Sets a the minimum degree limit in the scales for Astrometry to search if the scale parameter isn't set");
    ui->maxWidth->setToolTip("Sets a the maximum degree limit in the scales for Astrometry to search if the scale parameter isn't set");

    connect(ui->resort, &QCheckBox::stateChanged, this, [this]()
    {
        ui->resortQT->setChecked(ui->resort->isChecked());
    });
    ui->autoDown->setToolTip("This determines whether to automatically downsample or use the parameter below.");
    ui->downsample->setToolTip("This downsamples or bins the image to hopefully make it solve faster.");
    ui->resort->setToolTip("This resorts the stars based on magnitude. It usually makes it solve faster.");
    ui->use_scale->setToolTip("Whether or not to use the estimated image scale below to try to speed up the solve");
    ui->scale_low->setToolTip("The minimum size for the estimated image scale");
    ui->scale_high->setToolTip("The maximum size for the estimated image scale");
    ui->units->setToolTip("The units for the estimated image scale");

    ui->use_position->setToolTip("Whether or not to use the estimated position below to try to speed up the solve");
    ui->ra->setToolTip("The estimated RA of the object in decimal form in hours not degrees");
    ui->raString->setToolTip("The display of how the RA looks in H:M:S form");
    connect(ui->ra, &QLineEdit::textChanged, this, [this]()
    {
        char rastr[32];
        double ra = ui->ra->text().toDouble() * 15;
        ra2hmsstring(ra, rastr);
        ui->raString->setText(rastr);
    });
    ui->dec->setToolTip("The estimated DEC of the object in decimal form in degrees");
    ui->deString->setToolTip("The display of how the DEC looks in D:M:S form");
    connect(ui->dec, &QLineEdit::textChanged, this, [this]()
    {
        char decstr[32];
        double de = ui->dec->text().toDouble();
        dec2dmsstring(de, decstr);
        ui->deString->setText(decstr);
    });
    ui->radius->setToolTip("The search radius (degrees) of a circle centered on this position for astrometry.net to search for solutions");

    ui->oddsToKeep->setToolTip("The Astrometry oddsToKeep Parameter.  This may need to be changed or removed");
    ui->oddsToSolve->setToolTip("The Astrometry oddsToSolve Parameter.  This may need to be changed or removed");
    ui->oddsToTune->setToolTip("The Astrometry oddsToTune Parameter.  This may need to be changed or removed");

    ui->logToFile->setToolTip("Whether the stellarsolver should just output to the log window or whether it should log to a file.");
    ui->logFileName->setToolTip("The name and path of the file to log to, if this is blank, it will automatically log to a file in the temp Directory with an automatically generated name.");
    ui->logLevel->setToolTip("The verbosity level of the log to be displayed in the log window or saved to a file.");

    connect(ui->indexFolderPaths, &QComboBox::currentTextChanged, this, [this]()
    {
        loadIndexFilesListInFolder();
    });
    ui->indexFolderPaths->setToolTip("The paths on your compute to search for index files.  To add another, just start typing in the box.  To select one to look at, use the drop down.");
    connect(ui->removeIndexPath, &QPushButton::clicked, this, [this]()
    {
        ui->indexFolderPaths->removeItem( ui->indexFolderPaths->currentIndex());
    });
    ui->removeIndexPath->setToolTip("Removes the selected path in the index folder paths dropdown so that it won't get passed to the solver");
    connect(ui->addIndexPath, &QPushButton::clicked, this, [this]()
    {
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
    ui->singleIndexNum->setToolTip("The number of the index series to use in solving, if selected");
    ui->singleHealpix->setToolTip("The healpix (sky position) of the index series to use in solving, if selected");
    ui->useAllIndexes->setToolTip("Whether to use all the index files, or just the one index series selected");
    connect(ui->useAllIndexes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int ind)
    {
        ui->singleIndexNum->setReadOnly(ind == 0 || ind == 2);
        ui->singleHealpix->setReadOnly(ind == 0 || ind == 2);
        if(ind == 0)
        {
            ui->singleIndexNum->clear();
            ui->singleHealpix->clear();
        }
        else if(ind == 0) // ?? this condition can't succeed ??
        {            
            if(ui->singleIndexNum->text() == "")
            {
                ui->singleIndexNum->setText("4");
                ui->singleHealpix->clear();
            }
        }
        else
        {
            ui->singleIndexNum->setText(lastIndexNumber);
            ui->singleHealpix->setText(lastHealpix);
        }
    });
    connect(ui->singleIndexNum, &QLineEdit::textChanged, this, &MainWindow::loadIndexFilesToUse);
    connect(ui->singleHealpix, &QLineEdit::textChanged, this, &MainWindow::loadIndexFilesToUse);
    connect(ui->useAllIndexes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::loadIndexFilesToUse);

    //Behaviors and Settings for the StarTable
    connect(this, &MainWindow::readyForStarTable, this, &MainWindow::displayTable);
    ui->starTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(ui->starTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::starClickedInTable);
    ui->starTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(ui->exportStarTable, &QAbstractButton::clicked, this, &MainWindow::saveStarTable);
    ui->showStars->setToolTip("This toggles the stars circles on and off in the image");
    connect(ui->starOptions, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateImage);
    ui->starOptions->setToolTip("This allows you to select different types of star circles to put on the stars.  Warning, some require HFR to have been calculated first.");
    connect(ui->showFluxInfo, &QCheckBox::stateChanged, this, [this]()
    {
        showFluxInfo = ui->showFluxInfo->isChecked();
        updateHiddenStarTableColumns();
    });
    ui->showFluxInfo->setToolTip("This toggles whether to show or hide the HFR, peak, Flux columns in the star table after Sextraction.");
    connect(ui->showStarShapeInfo, &QCheckBox::stateChanged, this, [this]()
    {
        showStarShapeInfo = ui->showStarShapeInfo->isChecked();
        updateHiddenStarTableColumns();
    });
    ui->showStarShapeInfo->setToolTip("This toggles whether to show or hide the information about each star's semi-major axis, semi-minor axis, and orientation in the star table after sextraction.");

    //Behaviors for the Mouse over the Image to interact with the StartList and the UI
    connect(ui->Image, &ImageLabel::mouseMoved, this, &MainWindow::mouseMovedOverImage);
    connect(ui->Image, &ImageLabel::mouseClicked, this, &MainWindow::mouseClickedInImage);
    connect(ui->Image, &ImageLabel::mouseDown, this, &MainWindow::mousePressedInImage);

    //Behavior and settings for the Results Table
    setupResultsTable();
    ui->resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->resultsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(ui->exportResultsTable, &QAbstractButton::clicked, this, &MainWindow::saveResultsTable);
    ui->exportResultsTable->setToolTip("Exports the log of processes executed during this session to a CSV file for further analysis");
    connect(ui->showExtractorParams, &QCheckBox::stateChanged, this, [this]()
    {
        showExtractorParams = ui->showExtractorParams->isChecked();
        updateHiddenResultsTableColumns();
    });
    ui->showExtractorParams->setToolTip("This toggles whether to show or hide the Star Extractor Settings in the Results table at the bottom");
    connect(ui->showAstrometryParams, &QCheckBox::stateChanged, this, [this]()
    {
        showAstrometryParams = ui->showAstrometryParams->isChecked();
        updateHiddenResultsTableColumns();
    });
    ui->showAstrometryParams->setToolTip("This toggles whether to show or hide the Astrometry Settings in the Results table at the bottom");
    connect(ui->showSolutionDetails, &QCheckBox::stateChanged, this, [this]()
    {
        showSolutionDetails = ui->showSolutionDetails->isChecked();
        updateHiddenResultsTableColumns();
    });
    ui->showSolutionDetails->setToolTip("This toggles whether to show or hide the Solution Details in the Results table at the bottom");

    setWindowTitle(StellarSolver::getVersion());

    ui->progressBar->setTextVisible(false);
    timerMonitor.setInterval(1000); //1 sec intervals
    connect(&timerMonitor, &QTimer::timeout, this, [this]()
    {
        ui->status->setText(QString("Processing Trial %1: %2 s").arg(currentTrial).arg((int)processTimer.elapsed() / 1000) + 1);
    });

    setWindowIcon(QIcon(":/StellarSolverIcon.png"));

    //This Load the saved settings for the StellarSolver
    QSettings programSettings("Astrometry Freeware", "StellarSolver");

    //These will set the index
    int index = 0;
#if defined(Q_OS_OSX)
    if(QFile("/usr/local/bin/solve-field").exists())
        index = 2;
    else
        index = 3;
#elif defined(Q_OS_LINUX)
    index = 0;
#else //Windows
    index = 4;
#endif

    index = programSettings.value("setPathsIndex", index).toInt();
    ui->setPathsAutomatically->setCurrentIndex(index);

    //This gets a temporary ExternalExtractorSolver to get the defaults
    //It tries to load from the saved settings if possible as well.
    ExternalExtractorSolver extTemp(processType, m_ExtractorType, solverType, stats, m_ImageBuffer, this);
    ui->sextractorPath->setText(programSettings.value("sextractorBinaryPath", extTemp.sextractorBinaryPath).toString());
    ui->configFilePath->setText(programSettings.value("confPath", extTemp.confPath).toString());
    ui->solverPath->setText(programSettings.value("solverPath", extTemp.solverPath).toString());
    ui->astapPath->setText(programSettings.value("astapBinaryPath", extTemp.astapBinaryPath).toString());
    ui->watneyPath->setText(programSettings.value("watneyBinaryPath", extTemp.watneyBinaryPath).toString());
    ui->wcsPath->setText(programSettings.value("wcsPath", extTemp.wcsPath).toString());
    ui->cleanupTemp->setChecked(programSettings.value("cleanupTemporaryFiles", extTemp.cleanupTemporaryFiles).toBool());
    ui->generateAstrometryConfig->setChecked(programSettings.value("autoGenerateAstroConfig",
            extTemp.autoGenerateAstroConfig).toBool());
    ui->onlySendFITSFiles->setChecked(programSettings.value("onlySendFITSFiles", false).toBool());
    ui->onlineServer->setText(programSettings.value("onlineServer", "http://nova.astrometry.net").toString());
    ui->apiKey->setText(programSettings.value("apiKey", "iczikaqstszeptgs").toString());

    //These load the default settings from the StellarSolver usting a temporary object
    StellarSolver temp(processType, stats, m_ImageBuffer, this);
    ui->basePath->setText(QDir::tempPath());
    sendSettingsToUI(temp.getCurrentParameters());
    optionsList = temp.getBuiltInProfiles();
    foreach(SSolver::Parameters param, optionsList)
    {
        ui->optionsProfile->addItem(param.listName);
        ui->extractionProfile->addItem(param.listName);
        ui->solverProfile->addItem(param.listName);
    }
    optionsAreSaved = true;  //This way the next command won't trigger the unsaved warning.
    ui->optionsProfile->setCurrentIndex(0);
    ui->extractionProfile->setCurrentIndex(programSettings.value("sextractionProfile", 5).toInt());
    ui->solverProfile->setCurrentIndex(programSettings.value("solverProfile", 4).toInt());

    QString storedPaths = programSettings.value("indexFolderPaths", "").toString();
    QStringList indexFilePaths;
    if(storedPaths == "")
        indexFilePaths = temp.getDefaultIndexFolderPaths();
    else
        indexFilePaths = storedPaths.split(",");
    foreach(QString pathName, indexFilePaths)
        ui->indexFolderPaths->addItem(pathName);
    loadIndexFilesListInFolder();
    loadIndexFilesToUse();

    ui->imageScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->imageScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->imageScrollArea->verticalScrollBar()->setMinimum(0);
    ui->imageScrollArea->verticalScrollBar()->setMaximum(5000);
    ui->imageScrollArea->horizontalScrollBar()->setMinimum(0);
    ui->imageScrollArea->horizontalScrollBar()->setMaximum(5000);
    ui->imageScrollArea->verticalScrollBar()->setValue(2500);
    ui->imageScrollArea->horizontalScrollBar()->setValue(2500);
}

void MainWindow::settingJustChanged()
{
    if(ui->optionsProfile->currentIndex() != 0 )
        ui->optionsProfile->setCurrentIndex(0);
    optionsAreSaved = false;
}

void MainWindow::reloadConvTable()
{
    if(convInspector && convInspector->isVisible())
    {
        convTable->clear();
        QVector<float> convFilter =
        StellarSolver::generateConvFilter((SSolver::ConvFilterType) ui->convFilterType->currentIndex(), ui->fwhm->value());
        int size = sqrt(convFilter.size());
        double max = *std::max_element(convFilter.constBegin(), convFilter.constEnd());
        convTable->setRowCount(size);
        convTable->setColumnCount(size);
        int i = 0;
        for(int r = 0; r < size; r++)
        {
            convTable->setRowHeight(r, 50);
            for(int c = 0; c < size; c++)
            {
                convTable->setColumnWidth(c, 50);
                QTableWidgetItem *newItem = new QTableWidgetItem(QString::number(convFilter.at(i), 'g', 4));
                double col = convFilter.at(i)/max * 255;
                QFont font = newItem->font();
                font.setPixelSize(10);
                newItem->setFont(font);
                newItem->setFlags(newItem->flags() ^ Qt::ItemIsEditable);
                newItem->setBackground(QBrush(QColor(col,col,col)));
                newItem->setForeground(QBrush(Qt::yellow));
                convTable->setItem(r,c,newItem);
                i++;
            }
        }
        //convInspector->resize(size* 50 + 60, size * 50 + 60);
        int dialogWidth = convTable->horizontalHeader()->length() + 40;
        int dialogHeight= convTable->verticalHeader()->length()   + 40;
        convInspector->resize(dialogWidth, dialogHeight);

    }
}

void MainWindow::loadOptionsProfile()
{
    if(ui->optionsProfile->currentIndex() == 0)
        return;

    SSolver::Parameters oldOptions = getSettingsFromUI();

    if( !optionsAreSaved )
    {
        if(QMessageBox::question(this, "Abort?",
                                 "You made unsaved changes in the settings, do you really wish to overwrite them?") == QMessageBox::No)
        {
            ui->optionsProfile->setCurrentIndex(0);
            return;
        }
        optionsAreSaved = true; //They just got overwritten
    }

    SSolver::Parameters newOptions = optionsList.at(ui->optionsProfile->currentIndex() - 1);
    QList<QWidget *> controls = ui->optionsBox->findChildren<QWidget *>();
    foreach(QWidget *control, controls)
        control->blockSignals(true);
    sendSettingsToUI(newOptions);
    foreach(QWidget *control, controls)
        control->blockSignals(false);
    reloadConvTable();
}

MainWindow::~MainWindow()
{
    delete ui;
}

//This method clears the stars and star displays
void MainWindow::clearStars()
{
    ui->starTable->clearContents();
    ui->starTable->setRowCount(0);
    ui->starTable->setColumnCount(0);
    selectedStar = 0;
    stars.clear();
    updateImage();
}

//This method clears the tables and displays when the user requests it.
void MainWindow::clearResults()
{
    ui->logDisplay->clear();
    ui->resultsTable->clearContents();
    ui->resultsTable->setRowCount(0);
}

//These methods are for the logging of information to the textfield at the bottom of the window.
void MainWindow::logOutput(QString text)
{
    ui->logDisplay->appendPlainText(text);
    ui->logDisplay->show();
}

void MainWindow::toggleFullScreen()
{
  if (isFullScreen())
    showNormal();
  else
    showFullScreen();
}

void MainWindow::toggleLogDisplay()
{
  if (ui->tabWidget->isVisible())
    ui->tabWidget->hide();
  else
    ui->tabWidget->show();
}

void MainWindow::helpPopup()
{
      QString helpMessage =
        QString("<table>"
                "<tr><td>Zoom In: </td><td>%1</td></tr>"
                "<tr><td>Zoom Out: </td><td>%2</td></tr>\n"
                "<tr><td>Pan Up: </td><td>%3</td></tr>\n"
                "<tr><td>Pan Down: </td><td>%4</td></tr>\n"
                "<tr><td>Pan Left: </td><td>%5</td></tr>\n"
                "<tr><td>Pan Right: </td><td>%6</td></tr>\n"
                "<tr><td>AutoScale: </td><td>%7</td></tr>\n"
                "<tr><td>LoadImage: </td><td>%8</td></tr>\n"
                "<tr><td>Quit: </td><td>%9</td></tr>\n"
                "<tr><td>Toggle Log Display: </td><td>%10</td></tr>\n"
                "<tr><td>Toggle Full Screen: </td><td>%11</td></tr>\n"
                "<tr><td>Help: </td><td>%12</td></tr>\n"
                "</table>"
                )
        .arg(QKeySequence(QKeySequence::ZoomIn).toString(QKeySequence::NativeText))
        .arg(QKeySequence(QKeySequence::ZoomOut).toString(QKeySequence::NativeText))
        .arg(QKeySequence(QKeySequence::MoveToPreviousLine).toString(QKeySequence::NativeText))
        .arg(QKeySequence(QKeySequence::MoveToNextLine).toString(QKeySequence::NativeText))
        .arg(QKeySequence(QKeySequence::MoveToPreviousChar).toString(QKeySequence::NativeText))
        .arg(QKeySequence(QKeySequence::MoveToNextChar).toString(QKeySequence::NativeText))
        .arg("Ctrl+0")
        .arg(QKeySequence(QKeySequence::Open).toString(QKeySequence::NativeText))
        .arg(QKeySequence(QKeySequence::Quit).toString(QKeySequence::NativeText))
        .arg("Ctrl+l")
        .arg(QKeySequence(QKeySequence::FullScreen).toString(QKeySequence::NativeText))
        .arg(QKeySequence(QKeySequence::HelpContents).toString(QKeySequence::NativeText));

      QMessageBox *msgBox = new QMessageBox(this);
      msgBox->setIcon( QMessageBox::Information );
      msgBox->setText(helpMessage);
      msgBox->setAttribute(Qt::WA_DeleteOnClose);
      msgBox->setModal(false);
      msgBox->show();
}

void MainWindow::setSubframe()
{
    if(useSubframe)
    {
        useSubframe = false;
        ui->setSubFrame->setChecked(false);
    }
    else
    {
        settingSubframe = true;
        ui->mouseInfo->setText("Please select your subframe now.");
    }
    updateImage();
}

void MainWindow::startProcessMonitor()
{
    ui->status->setText(QString("Processing Trial %1").arg(currentTrial));
    ui->progressBar->setRange(0, 0);
    timerMonitor.start();
    processTimer.start();
}

void MainWindow::stopProcessMonitor()
{
    timerMonitor.stop();
    ui->progressBar->setRange(0, 10);
    ui->status->setText("No Process Running");
}

//I wrote this method because we really want to do this before all 4 processes
//It was getting repetitive.
bool MainWindow::prepareForProcesses()
{
    if(ui->vertSplitter->sizes().size() - 1 < 10)
        ui->vertSplitter->setSizes(QList<int>() << ui->vertSplitter->height() / 2 << 100 );
    ui->logDisplay->verticalScrollBar()->setValue(ui->logDisplay->verticalScrollBar()->maximum());

    if(!imageLoaded)
    {
        logOutput("Please Load an Image First");
        return false;
    }
    if(stellarSolver != nullptr)
    {
        if(stellarSolver->isRunning())
        {
            const SSolver::ProcessType type = static_cast<SSolver::ProcessType>(stellarSolver->property("ProcessType").toInt());
            if(type == SOLVE && !stellarSolver->solvingDone())
            {
                if(QMessageBox::question(this, "Abort?", "StellarSolver is solving now. Abort it?") == QMessageBox::No)
                    return false;
            }
            else if((type == EXTRACT || type == EXTRACT_WITH_HFR) && !extractorComplete())
            {
                if(QMessageBox::question(this, "Abort?", "StellarSolver is extracting sources now. Abort it?") == QMessageBox::No)
                    return false;
            }
            stellarSolver->abort();
        }
    }

    numberOfTrials = ui->trials->value();
    totalTime = 0;
    currentTrial = 0;
    lastSolution = FITSImage::Solution();
    hasHFRData = false;

    return true;
}

//I wrote this method to display the table after sextraction has occured.
void MainWindow::displayTable()
{
    sortStars();
    updateStarTableFromList();

    if(ui->horSplitter->sizes().size() - 1 < 10)
        ui->horSplitter->setSizes(QList<int>() << ui->optionsBox->width() << ui->horSplitter->width() / 2 << 200 );
    updateImage();
}

//This method is intended to load a list of the index files to display as feedback to the user.
void MainWindow::loadIndexFilesListInFolder()
{
    QString currentPath = ui->indexFolderPaths->currentText();
    QDir dir(currentPath);
    ui->indexFiles->clear();
    if(dir.exists())
    {
        dir.setNameFilters(QStringList() << "*.fits" << "*.fit");
        if(dir.entryList().count() == 0)
            ui->indexFiles->addItem("No index files in Folder");
        ui->indexFiles->addItems(dir.entryList());
    }
    else
        ui->indexFiles->addItem("Invalid Folder");
}

//This loads and updates the list of files we will use, if not autoindexing.
void MainWindow::loadIndexFilesToUse()
{
    // Convert the UI to a QStringList of directories to search.
    QStringList dirList;
    for(int i = 0; i < ui->indexFolderPaths->count(); i++)
      dirList << ui->indexFolderPaths->itemText(i);
  
    indexFileList.clear();
    if (ui->useAllIndexes->currentIndex() == 0)
        // Get all the fits files.
        indexFileList = StellarSolver::getIndexFiles(dirList, -1, -1);
    else
    {
        // Constraining the index files by index and possibly healpix.
        int healpix = -1;
        if (!ui->singleHealpix->text().isEmpty())
            healpix = ui->singleHealpix->text().toShort();
        int index = ui->singleIndexNum->text().toShort();
        indexFileList = StellarSolver::getIndexFiles(dirList, index, healpix);
    }
    // Strip directory paths for the UI.
    QStringList filenames;
    for (int i = 0; i < indexFileList.count(); ++i)
    {
        QFileInfo f(indexFileList[i]);
        filenames.append(f.fileName());
    }
    // Add the filenames (without the full path) to the UI.
    ui->indexFilesToUse->clear();
    ui->indexFilesToUse->addItems(filenames);
}

//The following methods are meant for starting the star extractor and image solver.
//The methods run when the buttons are clicked.  They call the methods inside StellarSolver and ExternalExtractorSovler

//This method responds when the user clicks the Sextract Button
void MainWindow::extractButtonClicked()
{
    if(!prepareForProcesses())
        return;

    int type = ui->extractorTypeForExtraction->currentIndex();
    switch(type)
    {
        case 0:
            processType = EXTRACT;
            m_ExtractorType = EXTRACTOR_INTERNAL;
            break;
        case 1:
            processType = EXTRACT_WITH_HFR;
            m_ExtractorType = EXTRACTOR_INTERNAL;
            break;
        case 2:
            processType = EXTRACT;
            m_ExtractorType = EXTRACTOR_EXTERNAL;
            break;
        case 3:
            processType = EXTRACT_WITH_HFR;
            m_ExtractorType = EXTRACTOR_EXTERNAL;
            break;

    }
    profileSelection = ui->extractionProfile->currentIndex();
    sextractImage();
}

//This method responds when the user clicks the Sextract Button
void MainWindow::solveButtonClicked()
{
    if(!prepareForProcesses())
        return;

    processType = SOLVE;
    profileSelection = ui->solverProfile->currentIndex();

    solveImage();
}

void MainWindow::resetStellarSolver()
{
    if(stellarSolver != nullptr)
    {
        auto *solver = stellarSolver.release();
        solver->disconnect(this);
        if(solver->isRunning())
        {
            connect(solver, &StellarSolver::finished, solver, &StellarSolver::deleteLater);
            solver->abort();
        }
        else
            solver->deleteLater();
    }

    stellarSolver.reset(new StellarSolver(stats, m_ImageBuffer, this));
    connect(stellarSolver.get(), &StellarSolver::logOutput, this, &MainWindow::logOutput);
}


void MainWindow::sextractImage()
{
    //This makes sure the solver is done before starting another time
    //That way the timer is accurate.
    while(stellarSolver->isRunning())
        qApp->processEvents();

    currentTrial++;
    clearStars();

    //Since this tester uses it many times, it doesn't *need* to replace the stellarsolver every time
    //resetStellarSolver();

    stellarSolver->setProperty("ExtractorType", m_ExtractorType);
    stellarSolver->setProperty("ProcessType", processType);

    //These set the StellarSolver Parameters
    if(profileSelection == 0)
        stellarSolver->setParameters(getSettingsFromUI());
    else
        stellarSolver->setParameters(optionsList.at(profileSelection - 1));


    setupExternalExtractorSolverIfNeeded();
    setupStellarSolverParameters();

    if(useSubframe)
        stellarSolver->setUseSubframe(subframe);
    else
        stellarSolver->clearSubFrame();

    connect(stellarSolver.get(), &StellarSolver::ready, this, &MainWindow::extractorComplete);

    startProcessMonitor();
    stellarSolver->start();
}

//This method runs when the user clicks the Sextract and Solve buttton
void MainWindow::solveImage()
{
    //This makes sure the solver is done before starting another time
    //That way the timer is accurate.
    while(stellarSolver->isRunning())
        qApp->processEvents();

    currentTrial++;

    //Since this tester uses it many times, it doesn't *need* to replace the stellarsolver every time
    //resetStellarSolver();

    m_ExtractorType = (SSolver::ExtractorType) ui->extractorTypeForSolving->currentIndex();
    solverType = (SSolver::SolverType) ui->solverType->currentIndex();

    stellarSolver->setProperty("ProcessType", processType);
    stellarSolver->setProperty("ExtractorType", m_ExtractorType);
    stellarSolver->setProperty("SolverType", solverType);

    //These set the StellarSolver Parameters
    if(profileSelection == 0)
        stellarSolver->setParameters(getSettingsFromUI());
    else
        stellarSolver->setParameters(optionsList.at(profileSelection - 1));

    setupStellarSolverParameters();
    setupExternalExtractorSolverIfNeeded();

    stellarSolver->clearSubFrame();

    //Setting the initial search scale settings
    if(ui->use_scale->isChecked())
        stellarSolver->setSearchScale(ui->scale_low->text().toDouble(), ui->scale_high->text().toDouble(),
                                      (SSolver::ScaleUnits)ui->units->currentIndex());
    else
        stellarSolver->setProperty("UseScale", false);
    //Setting the initial search location settings
    if(ui->use_position->isChecked())
        stellarSolver->setSearchPositionRaDec(ui->ra->text().toDouble(), ui->dec->text().toDouble());
    else
        stellarSolver->setProperty("UsePosition", false);

    connect(stellarSolver.get(), &StellarSolver::ready, this, &MainWindow::solverComplete);

    startProcessMonitor();
    stellarSolver->start();
}

//This sets up the External SExtractor and Solver and sets settings specific to them
void MainWindow::setupExternalExtractorSolverIfNeeded()
{
    //External options
    stellarSolver->setProperty("FileToProcess", fileToProcess);
    stellarSolver->setProperty("BasePath", ui->basePath->text());
    stellarSolver->setProperty("SextractorBinaryPath", ui->sextractorPath->text());
    stellarSolver->setProperty("ConfPath", ui->configFilePath->text());
    stellarSolver->setProperty("SolverPath", ui->solverPath->text());
    stellarSolver->setProperty("ASTAPBinaryPath", ui->astapPath->text());
    stellarSolver->setProperty("WatneyBinaryPath", ui->watneyPath->text());
    stellarSolver->setProperty("WCSPath", ui->wcsPath->text());
    stellarSolver->setProperty("CleanupTemporaryFiles", ui->cleanupTemp->isChecked());
    stellarSolver->setProperty("AutoGenerateAstroConfig", ui->generateAstrometryConfig->isChecked());
    stellarSolver->setProperty("OnlySendFITSFiles", ui->onlySendFITSFiles->isChecked());

    //Online Options
    stellarSolver->setProperty("BasePath",  ui->basePath->text());
    stellarSolver->setProperty("AstrometryAPIKey", ui->apiKey->text());
    stellarSolver->setProperty("AstrometryAPIURL", ui->onlineServer->text());
}

void MainWindow::setupStellarSolverParameters()
{
    stellarSolver->clearIndexFileAndFolderPaths();
    if(ui->useAllIndexes->currentIndex() == 0)
    {
        QStringList indexFolderPaths;
        for(int i = 0; i < ui->indexFolderPaths->count(); i++)
        {
            indexFolderPaths << ui->indexFolderPaths->itemText(i);
        }
        stellarSolver->setIndexFolderPaths(indexFolderPaths);
    }
    else
    {
        stellarSolver->setIndexFilePaths(indexFileList);
    }

    //These setup Logging if desired
    stellarSolver->setProperty("LogToFile", ui->logToFile->isChecked());
    QString filename = ui->logFileName->text();
    if(filename != "" && QFileInfo(filename).dir().exists() && !QFileInfo(filename).isDir())
        stellarSolver->m_LogFileName=filename;
    stellarSolver->setLogLevel((SSolver::logging_level)ui->logLevel->currentIndex());
    stellarSolver->setSSLogLevel((SSolver::SSolverLogLevel)ui->stellarSolverLogLevel->currentIndex());
}

//This sets all the settings for either internal SEP or external SExtractor
//based on the requested settings in the mainwindow interface.
//If you are implementing the StellarSolver Library in your progra, you may choose to change some or all of these settings or use the defaults.
SSolver::Parameters MainWindow::getSettingsFromUI()
{
    SSolver::Parameters params;
    params.listName = "Custom";
    params.description = ui->description->toPlainText();
    //These are to pass the parameters to the internal star extractor
    params.apertureShape = (SSolver::Shape) ui->apertureShape->currentIndex();
    params.kron_fact = ui->kron_fact->text().toDouble();
    params.subpix = ui->subpix->text().toInt() ;
    params.r_min = ui->r_min->text().toFloat();
    //params.inflags
    params.magzero = ui->magzero->text().toFloat();
    params.minarea = ui->minarea->text().toFloat();
    params.threshold_bg_multiple = ui->thresh_multiple->text().toFloat();
    params.threshold_offset = ui->thresh_offset->text().toFloat();
    params.deblend_thresh = ui->deblend_thresh->text().toInt();
    params.deblend_contrast = ui->deblend_contrast->text().toFloat();
    params.clean = (ui->cleanCheckBox->isChecked()) ? 1 : 0;
    params.clean_param = ui->clean_param->text().toDouble();
    params.convFilterType = (SSolver::ConvFilterType)ui->convFilterType->currentIndex();
    params.fwhm = ui->fwhm->text().toInt();
    params.partition = ui->partition->isChecked();

    //Star Filter Settings
    params.resort = ui->resort->isChecked();
    params.maxSize = ui->maxSize->text().toDouble();
    params.minSize = ui->minSize->text().toDouble();
    params.maxEllipse = ui->maxEllipse->text().toDouble();
    params.initialKeep = ui->initialKeep->text().toInt();
    params.keepNum = ui->keepNum->text().toInt();
    params.removeBrightest = ui->brightestPercent->text().toDouble();
    params.removeDimmest = ui->dimmestPercent->text().toDouble();
    params.saturationLimit = ui->saturationLimit->text().toDouble();

    //Settings that usually get set by the config file

    params.maxwidth = ui->maxWidth->text().toDouble();
    params.minwidth = ui->minWidth->text().toDouble();
    params.inParallel = ui->inParallel->isChecked();
    params.multiAlgorithm = (SSolver::MultiAlgo)ui->multiAlgo->currentIndex();
    params.solverTimeLimit = ui->solverTimeLimit->text().toInt();

    params.resort = ui->resort->isChecked();
    params.autoDownsample = ui->autoDown->isChecked();
    params.downsample = ui->downsample->value();
    params.search_radius = ui->radius->text().toDouble();

    //Setting the settings to know when to stop or keep searching for solutions
    params.logratio_tokeep = ui->oddsToKeep->text().toDouble();
    params.logratio_totune = ui->oddsToTune->text().toDouble();
    params.logratio_tosolve = ui->oddsToSolve->text().toDouble();

    return params;
}

void MainWindow::sendSettingsToUI(SSolver::Parameters a)
{
    ui->description->setText(a.description);

    //Star Extractor Settings

    ui->apertureShape->setCurrentIndex(a.apertureShape);
    connect(ui->apertureShape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::settingJustChanged);
    ui->kron_fact->setText(QString::number(a.kron_fact));
    ui->subpix->setText(QString::number(a.subpix));
    ui->r_min->setText(QString::number(a.r_min));

    ui->magzero->setText(QString::number(a.magzero));
    ui->minarea->setText(QString::number(a.minarea));
    ui->thresh_multiple->setText(QString::number(a.threshold_bg_multiple));
    ui->thresh_offset->setText(QString::number(a.threshold_offset));
    ui->deblend_thresh->setText(QString::number(a.deblend_thresh));
    ui->deblend_contrast->setText(QString::number(a.deblend_contrast));
    ui->cleanCheckBox->setChecked(a.clean == 1);
    ui->clean_param->setText(QString::number(a.clean_param));
    ui->convFilterType->setCurrentIndex(a.convFilterType);
    connect(ui->convFilterType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::settingJustChanged);
    ui->fwhm->setValue(a.fwhm);
    ui->partition->setChecked(a.partition);

    //Star Filter Settings

    ui->maxSize->setText(QString::number(a.maxSize));
    ui->minSize->setText(QString::number(a.minSize));
    ui->maxEllipse->setText(QString::number(a.maxEllipse));
    ui->initialKeep->setText(QString::number(a.initialKeep));
    ui->keepNum->setText(QString::number(a.keepNum));
    ui->brightestPercent->setText(QString::number(a.removeBrightest));
    ui->dimmestPercent->setText(QString::number(a.removeDimmest));
    ui->saturationLimit->setText(QString::number(a.saturationLimit));

    //Astrometry Settings

    ui->autoDown->setChecked(a.autoDownsample);
    ui->downsample->setValue(a.downsample);
    ui->inParallel->setChecked(a.inParallel);
    ui->multiAlgo->setCurrentIndex(a.multiAlgorithm);
    ui->solverTimeLimit->setText(QString::number(a.solverTimeLimit));
    ui->minWidth->setText(QString::number(a.minwidth));
    ui->maxWidth->setText(QString::number(a.maxwidth));
    ui->radius->setText(QString::number(a.search_radius));
    ui->resort->setChecked(a.resort);

    //Astrometry Log Ratio Settings

    ui->oddsToKeep->setText(QString::number(a.logratio_tokeep));
    ui->oddsToSolve->setText(QString::number(a.logratio_tosolve));
    ui->oddsToTune->setText(QString::number(a.logratio_totune));
}


//This runs when the star extractor is complete.
//It reports the time taken, prints a message, loads the sextraction stars to the startable, and adds the sextraction stats to the results table.
bool MainWindow::extractorComplete()
{
    elapsed = processTimer.elapsed() / 1000.0;


    disconnect(stellarSolver.get(), &StellarSolver::ready, this, &MainWindow::extractorComplete);

    if(!stellarSolver->failed() && stellarSolver->sextractionDone())
    {
        totalTime += elapsed; //Only add to total time if it was successful
        stars = stellarSolver->getStarList();
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        if(stellarSolver->isCalculatingHFR())
            logOutput(QString(stellarSolver->getCommandString() + " with HFR success! Got %1 stars").arg(stars.size()));
        else
            logOutput(QString(stellarSolver->getCommandString() + " success! Got %1 stars").arg(stars.size()));
        logOutput(QString("Sextraction took a total of: %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        if(currentTrial < numberOfTrials)
        {
            sextractImage();
            return true;
        }
        stopProcessMonitor();
        hasHFRData = stellarSolver->isCalculatingHFR();
    }
    else
    {
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString(stellarSolver->getCommandString() + "failed after %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        stopProcessMonitor();
        if(currentTrial == 1)
            return false;
        currentTrial--; //This solve was NOT successful so it should not be counted in the average.
    }

    emit readyForStarTable();
    ui->resultsTable->insertRow(ui->resultsTable->rowCount());
    addSextractionToTable();
    QTimer::singleShot(100, this, [this]()
    {
        ui->resultsTable->verticalScrollBar()->setValue(ui->resultsTable->verticalScrollBar()->maximum());
    });
    return true;
}

//This runs when the solver is complete.  It reports the time taken, prints a message, and adds the solution to the results table.
bool MainWindow::solverComplete()
{
    elapsed = processTimer.elapsed() / 1000.0;

    disconnect(stellarSolver.get(), &StellarSolver::ready, this, &MainWindow::solverComplete);

    if(!stellarSolver->failed() && stellarSolver->solvingDone())
    {
        totalTime += elapsed; //Only add to the total time if it was successful
        ui->progressBar->setRange(0, 10);
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString(stellarSolver->getCommandString() + " took a total of: %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        lastSolution = stellarSolver->getSolution();
        if(currentTrial < numberOfTrials)
        {
            solveImage();
            return true;
        }
        stopProcessMonitor();
    }
    else
    {
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        logOutput(QString(stellarSolver->getCommandString() + "failed after %1 second(s).").arg( elapsed));
        logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        stopProcessMonitor();
        if(currentTrial == 1)
            return false;
        currentTrial--; //This solve was NOT successful so it should not be counted in the average.
    }

    ui->resultsTable->insertRow(ui->resultsTable->rowCount());
    addSextractionToTable();
    if(stellarSolver->solvingDone())
        addSolutionToTable(stellarSolver->getSolution());
    else
        addSolutionToTable(lastSolution);
    short solutionIndexNumber =stellarSolver->getSolutionIndexNumber();
    lastIndexNumber = QString::number(stellarSolver->getSolutionIndexNumber());
    lastHealpix = QString::number(stellarSolver->getSolutionHealpix());
    if(solutionIndexNumber != -1 && ui->useAllIndexes->currentIndex() == 2)
    {
        ui->singleIndexNum->setText(lastIndexNumber);
        ui->singleHealpix->setText(lastHealpix);
        loadIndexFilesToUse();
    }
    if(ui->autoUpdateScaleAndPosition->isChecked())
    {
        FITSImage::Solution s = stellarSolver->getSolution();
        ui->ra->setText(QString::number(s.ra / 15));
        ui->dec->setText(QString::number(s.dec));
        double scale_low;
        double scale_high;
        switch(ui->units->currentIndex())
        {
        case SSolver::ARCMIN_WIDTH:
            scale_low = s.fieldHeight;
            scale_high = s.fieldWidth;
            break;
        case SSolver::ARCSEC_PER_PIX:
            scale_low = s.pixscale * 0.9;
            scale_high = s.pixscale * 1.1;
            break;
        case SSolver::DEG_WIDTH:
            scale_low = s.fieldHeight / 60;
            scale_high = s.fieldWidth / 60;
            break;
        default:
            scale_low = s.fieldHeight;
            scale_high = s.fieldWidth;
            break;
        }
        ui->scale_low->setText(QString::number(scale_low));
        ui->scale_high->setText(QString::number(scale_high));
    }
    QTimer::singleShot(100, this, [this]()
    {
        ui->resultsTable->verticalScrollBar()->setValue(ui->resultsTable->verticalScrollBar()->maximum());
    });

    if(stellarSolver.get()->hasWCSData())
    {
        hasWCSData = true;
        stars = stellarSolver->getStarList();
        hasHFRData = stellarSolver->isCalculatingHFR();
        if(stars.count() > 0)
            emit readyForStarTable();
    }
    return true;
}

//This method will attempt to abort the star extractor, sovler, and any other processes currently being run, no matter which type
void MainWindow::abort()
{
    numberOfTrials = currentTrial;
    if(stellarSolver != nullptr && stellarSolver->isRunning())
    {
        stellarSolver->abort();
        logOutput("Aborting Process. . .");
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
    if(stellarSolver != nullptr && stellarSolver->isRunning())
    {
        QMessageBox::critical(nullptr, "Message", "A Process is currently running on the image, please wait until it is completed");
        return false;
    }
    QString fileURL = QFileDialog::getOpenFileName(nullptr, "Load Image", dirPath,
                      "Images (*.fits *.fit *.bmp *.gif *.jpg *.jpeg *.tif *.tiff)");
    if (fileURL.isEmpty())
        return false;
    QFileInfo fileInfo(fileURL);
    if(!fileInfo.exists())
        return false;

    dirPath = fileInfo.absolutePath();
    fileToProcess = fileURL;

    clearAstrometrySettings();
    if(stellarSolver != nullptr)
        disconnect(stellarSolver.get(), &StellarSolver::logOutput, this, &MainWindow::logOutput);

    fileio imageLoader;
    imageLoader.logToSignal = true;
    connect(&imageLoader, &fileio::logOutput, this, &MainWindow::logOutput);

    if(imageLoader.loadImage(fileToProcess))
    {
        imageLoaded = true;
        clearImageBuffers();
        lastIndexNumber = "4"; //This will reset the filtering for index files
        lastHealpix = "";
        if(ui->useAllIndexes != 0)
        {
            ui->singleIndexNum->setText(lastIndexNumber);
            ui->singleHealpix->setText(lastHealpix);
        }
        loadIndexFilesToUse();
        m_ImageBuffer = imageLoader.getImageBuffer();
        stats=imageLoader.getStats();
        clearAstrometrySettings();
        if(imageLoader.position_given)
        {
            ui->use_position->setChecked(true);
            ui->ra->setText(QString::number(imageLoader.ra));
            ui->dec->setText(QString::number(imageLoader.dec));

        };
        if(imageLoader.scale_given)
        {
            ui->use_scale->setChecked(true);
            ui->scale_low->setText(QString::number(imageLoader.scale_low));
            ui->scale_high->setText(QString::number(imageLoader.scale_high));
            ui->units->setCurrentIndex(imageLoader.scale_units);
        }
        clearStars();
        ui->horSplitter->setSizes(QList<int>() << ui->optionsBox->width() << ui->horSplitter->width() << 0 );
        ui->fileNameDisplay->setText("Image: " + fileURL);
        initDisplayImage();
        hasWCSData = false;
        resetStellarSolver();
        return true;
    }
    return false;
}

//This method was copied and pasted from Fitsview in KStars
//It sets up the image that will be displayed on the screen
void MainWindow::initDisplayImage()
{
    // Account for leftover when sampling. Thus a 5-wide image sampled by 2
    // would result in a width of 3 (samples 0, 2 and 4).

    int w = (stats.width + sampling - 1) / sampling;
    int h = (stats.height + sampling - 1) / sampling;

    if (stats.channels == 1)
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
    doStretch(&rawImage);
    autoScale();

}

void adjustScrollBar(QScrollBar *scrollBar, double factor)
{
    scrollBar->setValue(int(factor * scrollBar->value()
                            + ((factor - 1) * scrollBar->pageStep()/2)));
}

void slideScrollBar(QScrollBar *scrollBar, double factor)
{
  constexpr double panFactor = 10.0;
  const int newValue = std::min(scrollBar->maximum(),
                                std::max(0, (int) (scrollBar->value() + factor * scrollBar->pageStep()/panFactor)));
  scrollBar->setValue(newValue);
}

void MainWindow::panLeft()
{
  if(!imageLoaded)
    return;
  slideScrollBar(ui->imageScrollArea->horizontalScrollBar(), -1);
}

void MainWindow::panRight()
{
  if(!imageLoaded)
    return;
  slideScrollBar(ui->imageScrollArea->horizontalScrollBar(), 1);
}

void MainWindow::panUp()
{
  if(!imageLoaded)
    return;
  slideScrollBar(ui->imageScrollArea->verticalScrollBar(), 1);
}

void MainWindow::panDown()
{
  if(!imageLoaded)
    return;
  slideScrollBar(ui->imageScrollArea->verticalScrollBar(), -1);
}

//This method reacts when the user clicks the zoom in button
void MainWindow::zoomIn()
{
    if(!imageLoaded)
        return;
    constexpr double zoomFactor = 1.5;
    currentZoom *= zoomFactor;
    adjustScrollBar(ui->imageScrollArea->verticalScrollBar(), zoomFactor);
    adjustScrollBar(ui->imageScrollArea->horizontalScrollBar(), zoomFactor);
    updateImage();
}

//This method reacts when the user clicks the zoom out button
void MainWindow::zoomOut()
{
    if(!imageLoaded)
        return;

    constexpr double zoomFactor = 1.5;
    currentZoom /= zoomFactor;
    adjustScrollBar(ui->imageScrollArea->verticalScrollBar(), 1.0/zoomFactor);
    adjustScrollBar(ui->imageScrollArea->horizontalScrollBar(), 1.0/zoomFactor);
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
QRect MainWindow::getStarSizeInImage(FITSImage::Star star, bool &accurate)
{
    accurate = true;
    double width = 0;
    double height = 0;
    double a = star.a;
    double b = star.b;
    double HFR = star.HFR;
    int starOption = ui->starOptions->currentIndex();

    //This is so that the star will appear on the screen even though it has invalid size info.
    //The External Astrometry.net solver does not report star sizes, nor does the online solver.
    if((a <= 0 || b <= 0) && (starOption == 0 || starOption == 1))
    {
        a = 5;
        b = 5;
        accurate = false;
    }
    if((HFR <= 0) && (starOption == 2 || starOption == 3))
    {
        HFR = 5;
        accurate = false;
    }

    switch(starOption)
    {
        case 0: //Ellipse from Sextraction
            width = 2 * a ;
            height = 2 * b;
            break;

        case 1: //Circle from Sextraction
        {
            double size = 2 * sqrt( pow(a, 2) + pow(b, 2) );
            width = size;
            height = size;
        }
        break;

        case 2: //HFD Size, based on HFR, 2 x radius is the diameter
            width = 2 * HFR;
            height = 2 * HFR;
            break;

        case 3: //2 x HFD size, based on HFR, 4 x radius is 2 x the diameter
            width = 4 * HFR;
            height = 4 * HFR;
            break;
    }

    double starx = star.x * currentWidth / stats.width ;
    double stary = star.y * currentHeight / stats.height;
    double starw = width * currentWidth / stats.width;
    double starh = height * currentHeight / stats.height;
    return QRect(starx - starw, stary - starh, starw * 2, starh * 2);
}

//This method is very loosely based on updateFrame in Fitsview in Kstars
//It will redraw the image when the user loads an image, zooms in, zooms out, or autoscales
//It will also redraw the image when a change needs to be made in how the circles for the stars are displayed such as highlighting one star
void MainWindow::updateImage()
{
    if(!imageLoaded || rawImage.isNull())
        return;

    int w = (stats.width + sampling - 1) / sampling;
    int h = (stats.height + sampling - 1) / sampling;
    currentWidth  = static_cast<int> (w * (currentZoom));
    currentHeight = static_cast<int> (h * (currentZoom));

    scaledImage = rawImage.scaled(currentWidth, currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap renderedImage = QPixmap::fromImage(scaledImage);
    if(ui->showStars->isChecked())
    {
        QPainter p(&renderedImage);
        for(int starnum = 0 ; starnum < stars.size() ; starnum++)
        {
            FITSImage::Star star = stars.at(starnum);
            bool accurate;
            QRect starInImage = getStarSizeInImage(star, accurate);
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
            }
            else
            {
                if(accurate)
                {
                    QPen highlighter(QColor("green"));
                    highlighter.setWidth(2);
                    p.setPen(highlighter);
                    p.setOpacity(1);
                }
                else
                {
                    p.setPen(QColor("red"));
                    p.setOpacity(0.6);
                }
            }
            p.drawEllipse(starInImage);
            p.restore();
        }
        if(useSubframe)
        {
            QPen highlighter(QColor("green"));
            highlighter.setWidth(2);
            p.setPen(highlighter);
            p.setOpacity(1);
            double x = subframe.x() * currentWidth / stats.width ;
            double y = subframe.y() * currentHeight / stats.height;
            double w = subframe.width() * currentWidth / stats.width;
            double h = subframe.height() * currentHeight / stats.height;
            p.drawRect(QRect(x, y, w, h));
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
                    stats.channels, static_cast<uint16_t>(stats.dataType));

    // Compute new auto-stretch params.
    StretchParams stretchParams = stretch.computeParams(m_ImageBuffer);

    stretch.setParams(stretchParams);
    stretch.run(m_ImageBuffer, outputImage, sampling);
}

//This method was copied and pasted from Fitsdata in KStars
//It clears the image buffer out.
void MainWindow::clearImageBuffers()
{
    delete[] m_ImageBuffer;
    m_ImageBuffer = nullptr;
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

        if(x < 0)
            x = 0;
        if(y < 0)
            y = 0;
        if(x > stats.width)
            x = stats.width;
        if(y > stats.height)
            y = stats.height;

        if(settingSubframe)
        {
            int subX = subframe.x();
            int subY = subframe.y();
            int w = x - subX;
            int h = y - subY;
            subframe = QRect(subX, subY, w, h);
            updateImage();
        }

        QString mouseText = "";
        if(hasWCSData)
        {
            QPointF wcsPixelPoint(x, y);
            FITSImage::wcs_point wcsCoord;
            if(stellarSolver.get()->pixelToWCS(wcsPixelPoint, wcsCoord))
            {
                mouseText = QString("RA: %1, DEC: %2").arg(StellarSolver::raString(wcsCoord.ra),
                                StellarSolver::decString(wcsCoord.dec));
            }
            else
            {
                mouseText = QString("WCS Data not fully loaded");
            }
        }
        else
            mouseText = QString("X: %1, Y: %2").arg(x).arg(y);
        if(stats.channels == 1)
            mouseText = mouseText + QString(", Value: %1").arg(getValue(x, y, 0));
        else
            mouseText = mouseText + QString(", Value: R: %1, G: %2, B: %3").arg(getValue(x, y, 0), getValue(x, y, 1), getValue(x, y, 2));
        if(useSubframe)
            mouseText = mouseText + QString(", Subframe X: %1, Y: %2, W: %3, H: %4").arg(subframe.x()).arg(subframe.y()).arg(
                            subframe.width()).arg(subframe.height());
        ui->mouseInfo->setText(mouseText);

        bool starFound = false;
        for(int i = 0 ; i < stars.size() ; i ++)
        {
            FITSImage::Star star = stars.at(i);
            bool accurate;
            QRect starInImage = getStarSizeInImage(star, accurate);
            if(starInImage.contains(location))
            {
                QString text = QString("Star: %1, x: %2, y: %3\nmag: %4, flux: %5, peak:%6").arg(i + 1).arg(star.x).arg(star.y).arg(
                                   star.mag).arg(star.flux).arg(star.peak);
                if(hasHFRData)
                    text += ", " + QString("HFR: %1").arg(star.HFR);
                if(hasWCSData)
                    text += "\n" + QString("RA: %1, DEC: %2").arg(StellarSolver::raString(star.ra), StellarSolver::decString(star.dec));
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
QString MainWindow::getValue(int x, int y, int channel)
{
    if (m_ImageBuffer == nullptr)
        return "";

    int index = y * stats.width + x + channel * stats.width * stats.height;
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
void MainWindow::mouseClickedInImage(QPoint location)
{
    if(settingSubframe)
        settingSubframe = false;

    for(int i = 0 ; i < stars.size() ; i ++)
    {
        bool accurate;
        QRect starInImage = getStarSizeInImage(stars.at(i), accurate);
        if(starInImage.contains(location))
            ui->starTable->selectRow(i);
    }
}

void MainWindow::mousePressedInImage(QPoint location)
{
    if(settingSubframe)
    {
        if(!useSubframe)
        {
            useSubframe = true;
            ui->setSubFrame->setChecked(true);
            double x = location.x() * stats.width / currentWidth;
            double y = location.y() * stats.height / currentHeight;
            subframe = QRect(x, y, 0, 0);
        }
    }
}


//THis method responds to row selections in the table and higlights the star you select in the image
void MainWindow::starClickedInTable()
{
    if(ui->starTable->selectedItems().count() > 0)
    {
        QTableWidgetItem *currentItem = ui->starTable->selectedItems().first();
        selectedStar = ui->starTable->row(currentItem);
        FITSImage::Star star = stars.at(selectedStar);
        double starx = star.x * currentWidth / stats.width ;
        double stary = star.y * currentHeight / stats.height;
        updateImage();
        ui->imageScrollArea->ensureVisible(starx, stary);
    }
}

//This sorts the stars by magnitude for display purposes
void MainWindow::sortStars()
{
    if(stars.size() > 1)
    {
        //Note that a star is dimmer when the mag is greater!
        //We want to sort in decreasing order though!
        std::sort(stars.begin(), stars.end(), [](const FITSImage::Star & s1, const FITSImage::Star & s2)
        {
            return s1.mag < s2.mag;
        });
    }
}

//This is a helper function that I wrote for the methods below
//It add a column with a particular name to the specified table
void addColumnToTable(QTableWidget *table, QString heading)
{
    int colNum = table->columnCount();
    table->insertColumn(colNum);
    table->setHorizontalHeaderItem(colNum, new QTableWidgetItem(heading));
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
            table->setItem(row, c, new QTableWidgetItem(value));
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
    addColumnToTable(table, "MAG_AUTO");
    addColumnToTable(table, "RA (J2000)");
    addColumnToTable(table, "DEC (J2000)");
    addColumnToTable(table, "X_IMAGE");
    addColumnToTable(table, "Y_IMAGE");


    addColumnToTable(table, "FLUX_AUTO");
    addColumnToTable(table, "PEAK");
    if(hasHFRData)
        addColumnToTable(table, "HFR");

    addColumnToTable(table, "a");
    addColumnToTable(table, "b");
    addColumnToTable(table, "theta");

    for(int i = 0; i < stars.size(); i ++)
    {
        table->insertRow(table->rowCount());
        FITSImage::Star star = stars.at(i);

        setItemInColumn(table, "MAG_AUTO", QString::number(star.mag));
        setItemInColumn(table, "X_IMAGE", QString::number(star.x));
        setItemInColumn(table, "Y_IMAGE", QString::number(star.y));
        if(hasWCSData)
        {
            setItemInColumn(table, "RA (J2000)", StellarSolver::raString(star.ra));
            setItemInColumn(table, "DEC (J2000)", StellarSolver::decString(star.dec));
        }

        setItemInColumn(table, "FLUX_AUTO", QString::number(star.flux));
        setItemInColumn(table, "PEAK", QString::number(star.peak));
        if(hasHFRData)
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

    setColumnHidden(table, "FLUX_AUTO", !showFluxInfo);
    setColumnHidden(table, "PEAK", !showFluxInfo);
    setColumnHidden(table, "RA (J2000)", !hasWCSData);
    setColumnHidden(table, "DEC (J2000)", !hasWCSData);
    setColumnHidden(table, "a", !showStarShapeInfo);
    setColumnHidden(table, "b", !showStarShapeInfo);
    setColumnHidden(table, "theta", !showStarShapeInfo);
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

    addColumnToTable(table, "Avg Time");
    addColumnToTable(table, "# Trials");
    addColumnToTable(table, "Command");
    addColumnToTable(table, "Profile");
    addColumnToTable(table, "Loglvl");
    addColumnToTable(table, "Stars");
    //Star Extractor Parameters
    addColumnToTable(table, "Shape");
    addColumnToTable(table, "Kron");
    addColumnToTable(table, "Subpix");
    addColumnToTable(table, "r_min");
    addColumnToTable(table, "minarea");
    addColumnToTable(table, "thresh_mult");
    addColumnToTable(table, "thresh_off");
    addColumnToTable(table, "d_thresh");
    addColumnToTable(table, "d_cont");
    addColumnToTable(table, "clean");
    addColumnToTable(table, "clean param");
    addColumnToTable(table, "conv");
    addColumnToTable(table, "fwhm");
    addColumnToTable(table, "part");
    //Star Filtering Parameters
    addColumnToTable(table, "Max Size");
    addColumnToTable(table, "Min Size");
    addColumnToTable(table, "Max Ell");
    addColumnToTable(table, "Ini Keep");
    addColumnToTable(table, "Keep #");
    addColumnToTable(table, "Cut Bri");
    addColumnToTable(table, "Cut Dim");
    addColumnToTable(table, "Sat Lim");
    //Astrometry Parameters
    addColumnToTable(table, "Pos?");
    addColumnToTable(table, "Scale?");
    addColumnToTable(table, "Resort?");
    addColumnToTable(table, "AutoDown");
    addColumnToTable(table, "Down");
    addColumnToTable(table, "in ||");
    addColumnToTable(table, "Multi");
    addColumnToTable(table, "# Thread");
    //Results
    addColumnToTable(table, "RA (J2000)");
    addColumnToTable(table, "DEC (J2000)");
    addColumnToTable(table, "RA ERR \"");
    addColumnToTable(table, "DEC ERR \"");
    addColumnToTable(table, "Orientation˚");
    addColumnToTable(table, "Field Width \'");
    addColumnToTable(table, "Field Height \'");
    addColumnToTable(table, "PixScale \"");
    addColumnToTable(table, "Parity");
    addColumnToTable(table, "Field");

    updateHiddenResultsTableColumns();
}

//This adds a Sextraction to the Results Table
//To add, remove, or change the way certain columns are filled when a sextraction is finished, edit them here.
void MainWindow::addSextractionToTable()
{
    QTableWidget *table = ui->resultsTable;
    SSolver::Parameters params = stellarSolver->getCurrentParameters();

    setItemInColumn(table, "Avg Time", QString::number(totalTime / currentTrial));
    setItemInColumn(table, "# Trials", QString::number(currentTrial));
    if(stellarSolver->isCalculatingHFR())
        setItemInColumn(table, "Command", stellarSolver->getCommandString() + " w/HFR");
    else
        setItemInColumn(table, "Command", stellarSolver->getCommandString());
    setItemInColumn(table, "Profile", params.listName);
    setItemInColumn(table, "Loglvl", stellarSolver->getLogLevelString());
    setItemInColumn(table, "Stars", QString::number(stellarSolver->getNumStarsFound()));
    //Star Extractor Parameters
    setItemInColumn(table, "Shape", stellarSolver->getShapeString());
    setItemInColumn(table, "Kron", QString::number(params.kron_fact));
    setItemInColumn(table, "Subpix", QString::number(params.subpix));
    setItemInColumn(table, "r_min", QString::number(params.r_min));
    setItemInColumn(table, "minarea", QString::number(params.minarea));
    setItemInColumn(table, "thresh_mult", QString::number(params.threshold_bg_multiple));
    setItemInColumn(table, "thresh_off", QString::number(params.threshold_offset));
    setItemInColumn(table, "d_thresh", QString::number(params.deblend_thresh));
    setItemInColumn(table, "d_cont", QString::number(params.deblend_contrast));
    setItemInColumn(table, "clean", QString::number(params.clean));
    setItemInColumn(table, "clean param", QString::number(params.clean_param));
    setItemInColumn(table, "conv", stellarSolver->getConvFilterString());
    setItemInColumn(table, "fwhm", QString::number(params.fwhm));
    setItemInColumn(table, "part", QString::number(params.partition));
    setItemInColumn(table, "Field", ui->fileNameDisplay->text());

    //StarFilter Parameters
    setItemInColumn(table, "Max Size", QString::number(params.maxSize));
    setItemInColumn(table, "Min Size", QString::number(params.minSize));
    setItemInColumn(table, "Max Ell", QString::number(params.maxEllipse));
    setItemInColumn(table, "Ini Keep", QString::number(params.initialKeep));
    setItemInColumn(table, "Keep #", QString::number(params.keepNum));
    setItemInColumn(table, "Cut Bri", QString::number(params.removeBrightest));
    setItemInColumn(table, "Cut Dim", QString::number(params.removeDimmest));
    setItemInColumn(table, "Sat Lim", QString::number(params.saturationLimit));

}

//This adds a solution to the Results Table
//To add, remove, or change the way certain columns are filled when a solve is finished, edit them here.
void MainWindow::addSolutionToTable(FITSImage::Solution solution)
{
    QTableWidget *table = ui->resultsTable;
    SSolver::Parameters params = stellarSolver->getCurrentParameters();

    setItemInColumn(table, "Avg Time", QString::number(totalTime / currentTrial));
    setItemInColumn(table, "# Trials", QString::number(currentTrial));
    setItemInColumn(table, "Command", stellarSolver->getCommandString());
    setItemInColumn(table, "Profile", params.listName);

    //Astrometry Parameters
    setItemInColumn(table, "Pos?", stellarSolver->property("UsePosition").toString());
    setItemInColumn(table, "Scale?", stellarSolver->property("UseScale").toString());
    setItemInColumn(table, "Resort?", QVariant(params.resort).toString());
    setItemInColumn(table, "AutoDown", QVariant(params.autoDownsample).toString());
    setItemInColumn(table, "Down", QVariant(params.downsample).toString());
    setItemInColumn(table, "in ||", QVariant(params.inParallel).toString());
    setItemInColumn(table, "Multi", stellarSolver->getMultiAlgoString());
    setItemInColumn(table, "# Thread", QVariant(stellarSolver->getNumThreads()).toString());


    //Results
    setItemInColumn(table, "RA (J2000)", StellarSolver::raString(solution.ra));
    setItemInColumn(table, "DEC (J2000)", StellarSolver::decString(solution.dec));
    if(solution.raError == 0)
        setItemInColumn(table, "RA ERR \"", "--");
    else
        setItemInColumn(table, "RA ERR \"", QString::number(solution.raError, 'f', 2));
    if(solution.decError == 0)
        setItemInColumn(table, "DEC ERR \"", "--");
    else
        setItemInColumn(table, "DEC ERR \"", QString::number(solution.decError, 'f', 2));
    setItemInColumn(table, "Orientation˚", QString::number(solution.orientation));
    setItemInColumn(table, "Field Width \'", QString::number(solution.fieldWidth));
    setItemInColumn(table, "Field Height \'", QString::number(solution.fieldHeight));
    setItemInColumn(table, "PixScale \"", QString::number(solution.pixscale));
    setItemInColumn(table, "Parity", solution.parity);
    setItemInColumn(table, "Field", ui->fileNameDisplay->text());
}

//I wrote this method to hide certain columns in the Results Table if the user wants to reduce clutter in the table.
void MainWindow::updateHiddenResultsTableColumns()
{
    QTableWidget *table = ui->resultsTable;
    //Star Extractor Params
    setColumnHidden(table, "Shape", !showExtractorParams);
    setColumnHidden(table, "Kron", !showExtractorParams);
    setColumnHidden(table, "Subpix", !showExtractorParams);
    setColumnHidden(table, "r_min", !showExtractorParams);
    setColumnHidden(table, "minarea", !showExtractorParams);
    setColumnHidden(table, "thresh_mult", !showExtractorParams);
    setColumnHidden(table, "thresh_off", !showExtractorParams);
    setColumnHidden(table, "d_thresh", !showExtractorParams);
    setColumnHidden(table, "d_cont", !showExtractorParams);
    setColumnHidden(table, "clean", !showExtractorParams);
    setColumnHidden(table, "clean param", !showExtractorParams);
    setColumnHidden(table, "conv", !showExtractorParams);
    setColumnHidden(table, "fwhm", !showExtractorParams);
    setColumnHidden(table, "part", !showExtractorParams);
    //Star Filtering Parameters
    setColumnHidden(table, "Max Size", !showExtractorParams);
    setColumnHidden(table, "Min Size", !showExtractorParams);
    setColumnHidden(table, "Max Ell", !showExtractorParams);
    setColumnHidden(table, "Ini Keep", !showExtractorParams);
    setColumnHidden(table, "Keep #", !showExtractorParams);
    setColumnHidden(table, "Cut Bri", !showExtractorParams);
    setColumnHidden(table, "Cut Dim", !showExtractorParams);
    setColumnHidden(table, "Sat Lim", !showExtractorParams);
    //Astrometry Parameters
    setColumnHidden(table, "Pos?", !showAstrometryParams);
    setColumnHidden(table, "Scale?", !showAstrometryParams);
    setColumnHidden(table, "Resort?", !showAstrometryParams);
    setColumnHidden(table, "AutoDown", !showAstrometryParams);
    setColumnHidden(table, "Down", !showAstrometryParams);
    setColumnHidden(table, "in ||", !showAstrometryParams);
    setColumnHidden(table, "Multi", !showAstrometryParams);
    setColumnHidden(table, "# Thread", !showAstrometryParams);
    //Results
    setColumnHidden(table, "RA (J2000)", !showSolutionDetails);
    setColumnHidden(table, "DEC (J2000)", !showSolutionDetails);
    setColumnHidden(table, "RA ERR \"", !showSolutionDetails);
    setColumnHidden(table, "DEC ERR \"", !showSolutionDetails);
    setColumnHidden(table, "Orientation˚", !showSolutionDetails);
    setColumnHidden(table, "Field Width \'", !showSolutionDetails);
    setColumnHidden(table, "Field Height \'", !showSolutionDetails);
    setColumnHidden(table, "PixScale \"", !showSolutionDetails);
    setColumnHidden(table, "Parity", !showSolutionDetails);
}

//This will write the Results table to a csv file if the user desires
//Then the user can analyze the solution information in more detail to try to perfect star extractor and solver parameters
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
        #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            outstream << Qt::endl;
        #else
            outstream << endl;
        #endif

    }
    QMessageBox::information(this, "Message", QString("Results Table Saved as: %1").arg(path));
    file.close();
}

//This will write the Star table to a csv file if the user desires
//Then the user can analyze the solution information in more detail to try to analyze the stars found or try to perfect star extractor parameters
void MainWindow::saveStarTable()
{
    if (ui->starTable->rowCount() == 0)
        return;

    QUrl exportFile = QFileDialog::getSaveFileUrl(this, "Export Star Table", dirPath,
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

    for (int c = 0; c < ui->starTable->columnCount(); c++)
    {
        outstream << ui->starTable->horizontalHeaderItem(c)->text() << ',';
    }
    outstream << "\n";

    for (int r = 0; r < ui->starTable->rowCount(); r++)
    {
        for (int c = 0; c < ui->starTable->columnCount(); c++)
        {
            QTableWidgetItem *cell = ui->starTable->item(r, c);

            if (cell)
                outstream << cell->text() << ',';
            else
                outstream << " " << ',';
        }
        #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            outstream << Qt::endl;
        #else
            outstream << endl;
        #endif
    }
    QMessageBox::information(this, "Message", QString("Star Table Saved as: %1").arg(path));
    file.close();
}

void MainWindow::saveOptionsProfiles()
{
    QString fileURL = QFileDialog::getSaveFileName(nullptr, "Save Options Profiles", dirPath,
                      "INI files(*.ini)");
    if (fileURL.isEmpty())
        return;
    QSettings settings(fileURL, QSettings::IniFormat);
    for(int i = 0 ; i < optionsList.count(); i++)
    {
        SSolver::Parameters params = optionsList.at(i);
        settings.beginGroup(params.listName);
        QMap<QString, QVariant> map = SSolver::Parameters::convertToMap(params);
        QMapIterator<QString, QVariant> it(map);
        while(it.hasNext())
        {
            it.next();
            settings.setValue(it.key(), it.value());
        }
        settings.endGroup();
    }

}

void MainWindow::loadOptionsProfiles()
{
    QString fileURL = QFileDialog::getOpenFileName(nullptr, "Load Options Profiles File", dirPath,
                      "INI files(*.ini)");
    if (fileURL.isEmpty())
        return;
    if(!QFileInfo::exists(fileURL))
    {
        QMessageBox::warning(this, "Message", "The file doesn't exist");
        return;
    }

    ui->optionsProfile->blockSignals(true);
    ui->optionsProfile->clear();
    ui->optionsProfile->addItem("Unsaved Options");
    optionsList = StellarSolver::loadSavedOptionsProfiles(fileURL);
    foreach(SSolver::Parameters params, optionsList)
        ui->optionsProfile->addItem(params.listName);
    ui->optionsProfile->blockSignals(false);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings programSettings("Astrometry Freeware", "StellarSolver");

    programSettings.setValue("setPathsIndex", ui->setPathsAutomatically->currentIndex());

    programSettings.setValue("sextractorBinaryPath", ui->sextractorPath->text());
    programSettings.setValue("sextractionProfile", ui->extractionProfile->currentIndex());
    programSettings.setValue("confPath", ui->configFilePath->text());
    programSettings.setValue("solverPath", ui->solverPath->text());
    programSettings.setValue("solverProfile", ui->solverProfile->currentIndex());
    programSettings.setValue("astapBinaryPath", ui->astapPath->text());
    programSettings.setValue("watneyBinaryPath", ui->watneyPath->text());
    programSettings.setValue("wcsPath", ui->wcsPath->text());
    programSettings.setValue("cleanupTemporaryFiles",  ui->cleanupTemp->isChecked());
    programSettings.setValue("autoGenerateAstroConfig", ui->generateAstrometryConfig->isChecked());
    programSettings.setValue("onlySendFITSFiles", ui->onlySendFITSFiles->isChecked());
    programSettings.setValue("onlineServer", ui->onlineServer->text());
    programSettings.setValue("apiKey", ui->apiKey->text());

    QStringList items;
    for(int i = 0; i < ui->indexFolderPaths->count(); i++)
    {
        items << ui->indexFolderPaths->itemText(i);
    }
    programSettings.setValue("indexFolderPaths", items.join(","));

    programSettings.sync();
    event->accept();
}








