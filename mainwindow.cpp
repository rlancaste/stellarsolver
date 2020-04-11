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

void addColumnToTable(QTableWidget *table, QString heading)
{
    int colNum = table->columnCount();
    table->insertColumn(colNum);
    table->setHorizontalHeaderItem(colNum,new QTableWidgetItem(heading));
}

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

    connect(ui->Abort,&QAbstractButton::clicked, this, &MainWindow::abort );
    connect(ui->Clear,&QAbstractButton::clicked, this, &MainWindow::clearAll );
    connect(ui->exportTable,&QAbstractButton::clicked, this, &MainWindow::saveSolutionTable);

    //Behaviors for the StarList
    ui->starList->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(ui->starList,&QTableWidget::itemSelectionChanged, this, &MainWindow::starClickedInTable);

    //Behaviors for the Image to interact with the StartList
    connect(ui->Image,&ImageLabel::mouseHovered,this, &MainWindow::mouseOverStar);
    connect(ui->Image,&ImageLabel::mouseClicked,this, &MainWindow::mouseClickedOnStar);

    //Hides the panels into the sides and bottom
    ui->splitter->setSizes(QList<int>() << ui->splitter->height() << 0 );
    ui->splitter_2->setSizes(QList<int>() << 0 << ui->splitter_2->width() << 0 );

    //Basic Settings
    connect(ui->configFilePath, &QLineEdit::textChanged, this, [this](){ confPath = ui->configFilePath->text(); });
    connect(ui->sextractorPath, &QLineEdit::textChanged, this, [this](){ sextractorBinaryPath = ui->sextractorPath->text(); });
    connect(ui->solverPath, &QLineEdit::textChanged, this, [this](){ solverPath = ui->solverPath->text(); });
    connect(ui->tempPath, &QLineEdit::textChanged, this, [this](){ tempPath = ui->tempPath->text(); });
    connect(ui->wcsPath, &QLineEdit::textChanged, this, [this](){ wcsPath = ui->wcsPath->text(); });

    connect(ui->reset, &QPushButton::clicked, this, &MainWindow::resetOptionsToDefaults);

    //Sextractor Settings
    connect(ui->showStars,&QAbstractButton::clicked, this, &MainWindow::updateImage );
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

    setupSolutionTable();

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;


    //Delete the temp files for Sextractor in case the parameters get changed during testing and coding
    QDir temp(tempPath);
    temp.remove("default.param");
    temp.remove("default.conv");

    setWindowTitle("SexySolver Internal Sextractor and Astrometry.net Based Solver");

}

void MainWindow::resetOptionsToDefaults()
{

    ExternalSextractorSolver extTemp(stats, m_ImageBuffer, this);
    ui->sextractorPath->setText(extTemp.sextractorBinaryPath);
    ui->configFilePath->setText(extTemp.confPath);
    ui->solverPath->setText(extTemp.solverPath);
    ui->wcsPath->setText(extTemp.wcsPath);

    SexySolver temp(stats, m_ImageBuffer, this);

    //Basic Settings
    tempPath = QDir::tempPath();

        ui->showStars->setChecked(true);
        ui->tempPath->setText(tempPath);

    //Sextractor Settings
    apertureShape = temp.apertureShape;
    kron_fact = temp.kron_fact;
    subpix = temp.subpix;
    r_min = temp.r_min;

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

        ui->maxEllipse->setText(QString::number(maxEllipse));
        ui->brightestPercent->setText(QString::number(removeBrightest));
        ui->dimmestPercent->setText(QString::number(removeDimmest));

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


//These methods are for the logging of information to the textfield at the bottom of the window.
//They are used by everything

void MainWindow::clearAll()
{
    ui->logDisplay->clear();
    ui->starList->clearContents();
    ui->solutionTable->clearContents();
    stars.clear();
    updateImage();
}

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
    if(!sexySolver.isNull())
    {
        sexySolver->abort();
        sexySolver.clear();
        extSolver.clear();
    }
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
//The methods run when the buttons are clicked.  The actual methods for doing sextraction and solving are further down.

//I wrote this method to call the sextract method below in basically the same way we do in KStars right now.
//It uses the external sextractor (or sex) program
//It then will load the results into a table to the right of the image
bool MainWindow::sextractImage()
{
    if(!prepareForProcesses())
        return false;

    //If the Use SexySolver CheckBox is checked, use the internal method instead.
    if(ui->useSexySolver->isChecked())
        return sextractInternally();

    #ifdef _WIN32
    logOutput("Sextractor is not easily installed on Windows, try the Internal Sextractor please.");
    return false;
    #endif

    solverTimer.start();

    sextract(true);
    return true;
}

//I wrote this method to call the sextract and solve methods below in basically the same way we do in KStars right now.
//It uses the external programs solve-field and sextractor (or sex)
//It times the entire process and prints out how long it took
bool MainWindow::solveImage()
{
    if(!prepareForProcesses())
        return false;

    //If the Use SexySolver CheckBox is checked, use the internal method instead.
    if(ui->useSexySolver->isChecked())
        return solveInternally();

    solverTimer.start();

#ifndef _WIN32 //This is because the sextractor program is VERY difficult to install,
    if(sextract(false))
    {
#endif
        if(solveField())
            return true;
        return false;
#ifndef _WIN32
    }
#endif
    return false;
}

//I wrote this method to call the internal sextract method below.
//It then will load the results into a table to the right of the image
bool MainWindow::sextractInternally()
{
    solverTimer.start();
    runInnerSextractor();
    return true;
}

//I wrote this method to start the internal solver method below
//It runs in a separate thread so that it is nonblocking
//It times the entire process and prints out how long it took
bool MainWindow::solveInternally()
{
      solverTimer.start();
      runInnerSolver();
      return true;
}

//This method will abort the sextractor, sovler, and any other processes currently being run, no matter which type
//It will NOT abort the solver if the internal solver is run with QT Concurrent
void MainWindow::abort()
{
    logOutput("Aborting. . .");
    if(!sexySolver.isNull())
        sexySolver->abort();
}

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

    saveAsFITS();

    return true;
}

//This is very necessary for solving non-fits images with external Sextractor
//This was copied and pasted and modified from ImageToFITS in fitsdata in KStars
bool MainWindow::saveAsFITS()
{
    QFileInfo fileInfo(fileToSolve.toLatin1());
    QString newFilename = tempPath + QDir::separator() + fileInfo.baseName() + "_solve.fits";

    int status = 0;
    fitsfile * new_fptr;
    long naxis = rawImage.allGray() ? 2 : 3, nelements, exposure;
    long naxes[3] = { stats.width, stats.height, naxis == 3 ? 3 : 1 };
    char error_status[512] = {0};

    QFileInfo newFileInfo(newFilename);
    if(newFileInfo.exists())
        QFile(newFilename).remove();

    nelements = stats.samples_per_channel * m_Channels;

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, newFilename.toLatin1(), &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    fptr = new_fptr;

    if (fits_create_img(fptr, BYTE_IMG, naxis, naxes, &status))
    {
        logOutput(QString("fits_create_img failed: %1").arg(error_status));
        status = 0;
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        return false;
    }

    /* Write Data */
    if (fits_write_img(fptr, stats.dataType, 1, nelements, m_ImageBuffer, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    /* Write keywords */

    exposure = 1;
    fits_update_key(fptr, TLONG, "EXPOSURE", &exposure, "Total Exposure Time", &status);

    // NAXIS1
    if (fits_update_key(fptr, TUSHORT, "NAXIS1", &(stats.width), "length of data axis 1", &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    // NAXIS2
    if (fits_update_key(fptr, TUSHORT, "NAXIS2", &(stats.height), "length of data axis 2", &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    // ISO Date
    if (fits_write_date(fptr, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    fileToSolve = newFilename;

    fits_flush_file(fptr, &status);

    logOutput("Saved FITS file:" + fileToSolve);

    return status;
}

//This method writes the table to the file
//I had to create it from the examples on NASA's website
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/quick/node10.html
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/cookbook/node16.html
bool MainWindow::writeSextractorTable()
{

    if(sextractorFilePath == "")
    {
        srand(time(NULL));
        sextractorFilePath = basePath + QDir::separator() + "sexySolver_" + QString::number(rand()) + ".xyls";
    }

    QFile sextractorFile(sextractorFilePath);
    if(sextractorFile.exists())
        sextractorFile.remove();

    int status = 0;
    fitsfile * new_fptr;


    if (fits_create_file(&new_fptr, sextractorFilePath.toLatin1(), &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    int tfields=2;
    int nrows=stars.size();
    QString extname="Sextractor_File";

    //Columns: X_IMAGE, double, pixels, Y_IMAGE, double, pixels
    char* ttype[] = { xcol, ycol };
    char* tform[] = { colFormat, colFormat };
    char* tunit[] = { colUnits, colUnits };
    const char* extfile = "Sextractor_File";

    float *xArray = new float[stars.size()];
    float *yArray = new float[stars.size()];

    for (int i = 0; i < stars.size(); i++)
    {
        xArray[i] = stars.at(i).x;
        yArray[i] = stars.at(i).y;
    }

    if(fits_create_tbl(new_fptr, BINARY_TBL, nrows, tfields,
        ttype, tform, tunit, extfile, &status))
    {
        logOutput(QString("Could not create binary table."));
        return false;
    }

    int firstrow  = 1;  /* first row in table to write   */
    int firstelem = 1;
    int column = 1;

    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, xArray, &status))
    {
        logOutput(QString("Could not write x pixels in binary table."));
        return false;
    }

    column = 2;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, yArray, &status))
    {
        emit logOutput(QString("Could not write y pixels in binary table."));
        return false;
    }

    if(fits_close_file(new_fptr, &status))
    {
        emit logOutput(QString("Error closing file."));
        return false;
    }

    free(xArray);
    free(yArray);

    return true;
}

//This method was copied and pasted from Fitsdata in KStars
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

void MainWindow::zoomIn()
{
    if(!imageLoaded)
        return;

    currentZoom *= 1.5;
    updateImage();
}

void MainWindow::zoomOut()
{
    if(!imageLoaded)
        return;

    currentZoom /= 1.5;
    updateImage();
}

//This code was copied and pasted and modified from rescale in Fitsview in KStars
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

QRect MainWindow::getStarInImage(Star star)
{
    double starx = star.x * currentWidth / stats.width ;
    double stary = star.y * currentHeight / stats.height;
    if(apertureShape == SHAPE_ELLIPSE || apertureShape == SHAPE_AUTO)
    {
        double starw = 2 * star.a * currentHeight / stats.height;
        double starh = 2 * star.b * currentHeight / stats.height;
        //int r = 10 * currentWidth / stats.width ;
        return QRect(starx - starw, stary - starh , starw*2, starh*2);
    }
    else
    {
        double r = r_min * currentHeight / stats.height;
        return QRect(starx - r, stary - r , r*2, r*2);
    }
}

//This method is very loosely based on updateFrame in Fitsview in Kstars
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
            QRect starInImage = getStarInImage(star);
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
void MainWindow::clearImageBuffers()
{
    delete[] m_ImageBuffer;
    m_ImageBuffer = nullptr;
    //m_BayerBuffer = nullptr;
}


void MainWindow::mouseOverStar(QPoint location)
{
    bool starFound = false;
    for(int i = 0 ; i < stars.size() ; i ++)
    {
        Star star = stars.at(i);
        QRect starInImage = getStarInImage(star);
        if(starInImage.contains(location))
        {
            QString text = QString("Star: %1, x: %2, y: %3, mag: %4, flux: %5").arg(i + 1).arg(star.x).arg(star.y).arg(star.mag).arg(star.flux);
            QToolTip::showText(QCursor::pos(), text, ui->Image);
            selectedStar = i;
            starFound = true;
            updateImage();
        }
    }
    if(!starFound)
        QToolTip::hideText();
}

void MainWindow::mouseClickedOnStar(QPoint location)
{
    //logOutput(QString("Mouseclicked: x:") + location.x() + QString(", y:") + location.y());
    for(int i = 0 ; i < stars.size() ; i ++)
    {
        QRect starInImage = getStarInImage(stars.at(i));
        if(starInImage.contains(location))
            ui->starList->selectRow(i);
    }
}

//I wrote these methods to load the sextracted stars into a table to the right of the image


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
    addColumnToTable(table,"a");
    addColumnToTable(table,"b");
    addColumnToTable(table,"theta");

    for(int i = 0; i < stars.size(); i ++)
    {
        table->insertRow(table->rowCount());
        Star star = stars.at(i);

        setItemInColumn(table, "MAG_AUTO", QString::number(star.mag));
        setItemInColumn(table, "FLUX_AUTO", QString::number(star.flux));
        setItemInColumn(table, "X_IMAGE", QString::number(star.x));
        setItemInColumn(table, "Y_IMAGE", QString::number(star.y));
        setItemInColumn(table, "a", QString::number(star.a));
        setItemInColumn(table, "b", QString::number(star.b));
        setItemInColumn(table, "theta", QString::number(star.theta));
    }
}




//These methods will get the solver options from a fits file and prepare them for image solving.



//This method is copied and pasted and modified from getSolverOptionsFromFITS in Align in KStars
//Then it was split in two parts
//Tnis part extracts the options from the FITS file and prepares them.
//It is used by both the internal and external solver
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



//This method is copied and pasted and modified from the code I wrote to use sextractor in OfflineAstrometryParser in KStars
bool MainWindow::sextract(bool justSextract)
{
    extSolver = new ExternalSextractorSolver(stats, m_ImageBuffer, this);
    sexySolver = extSolver;

    extSolver->fileToSolve = fileToSolve;
    connect(extSolver, &ExternalSextractorSolver::logNeedsUpdating, this, &MainWindow::logOutput);
    connect(extSolver, &ExternalSextractorSolver::starsFound, this, &MainWindow::sextractorComplete);
    setSextractorSettings();

    extSolver->sextract(justSextract);

    return true;

}



//The code for this method is copied and pasted and modified from OfflineAstrometryParser in KStars
bool MainWindow::solveField()
{
    extSolver = new ExternalSextractorSolver(stats, m_ImageBuffer, this);
    sexySolver = extSolver;
    setSolverSettings();
    extSolver->fileToSolve = fileToSolve;

    connect(extSolver, &ExternalSextractorSolver::logNeedsUpdating, this, &MainWindow::logOutput);
    connect(extSolver, &ExternalSextractorSolver::finished, this, &MainWindow::solverComplete);
    extSolver->runExternalSextractorAndSolver();

    return true;
}

bool MainWindow::sextractorComplete()
{
    double elapsed = solverTimer.elapsed() / 1000.0;
    stars = sexySolver->getStarList();
    displayTable();
    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    logOutput(QString("Successfully sextracted %1 stars.").arg(stars.size()));
    logOutput(QString("Sextraction took a total of: %1 second(s).").arg( elapsed));
    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

    addSextractionToTable();
    writeSextractorTable();  //Just in case they then want to solve it.
    return true;
}


//This was adapted from KStars' OfflineAstrometryParser
bool MainWindow::solverComplete(int x)
{
    double elapsed = solverTimer.elapsed() / 1000.0;
    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    logOutput(QString("Sextraction and Solving took a total of: %1 second(s).").arg( elapsed));
    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    addSolutionToTable(sexySolver->solution);
    return true;
}

bool MainWindow::runInnerSextractor()
{
    sexySolver = new SexySolver(stats, m_ImageBuffer, this);

    connect(sexySolver, &SexySolver::logNeedsUpdating, this, &MainWindow::logOutput, Qt::QueuedConnection);
    connect(sexySolver, &SexySolver::starsFound, this, &MainWindow::sextractorComplete);
    setSextractorSettings();
    sexySolver->start();
    return true;
}

bool MainWindow::runInnerSolver()
{
    sexySolver = new SexySolver(stats ,m_ImageBuffer, this);
    connect(sexySolver, &SexySolver::logNeedsUpdating, this, &MainWindow::logOutput, Qt::QueuedConnection);
    connect(sexySolver, &SexySolver::finished, this, &MainWindow::solverComplete);

    setSolverSettings();

    sexySolver->start();
    return true;
}

void MainWindow::setSextractorSettings()
{
    sexySolver->justSextract = true;

    //These are to pass the parameters to the internal sextractor
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
}

void MainWindow::setSolverSettings()
{
    sexySolver->justSextract = false;

    //Sextractor Settings
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

    //Astrometry Settings

    sexySolver->setIndexFolderPaths(indexFilePaths);
    sexySolver->maxwidth = maxwidth;
    sexySolver->minwidth = minwidth;
    sexySolver->inParallel = inParallel;
    sexySolver->solverTimeLimit = solverTimeLimit;

    if(use_scale)
        sexySolver->setSearchScale(fov_low, fov_high, units);

    if(use_position)
        sexySolver->setSearchPosition(ra, dec, radius);

    sexySolver->logratio_tokeep = logratio_tokeep;
    sexySolver->logratio_totune = logratio_totune;
    sexySolver->logratio_tosolve = logratio_tosolve;

    sexySolver->logToFile = logToFile;
    sexySolver->logFile = logFile;
    sexySolver->logLevel = logLevel;
}

//To add new columns to this table, just add it to both functions
//So that the column gets setup and then gets filled in.

void MainWindow::setupSolutionTable()
{
    //These are in the order that they will appear in the table.

    addColumnToTable(ui->solutionTable,"Time");
    addColumnToTable(ui->solutionTable,"Int?");
    addColumnToTable(ui->solutionTable,"Stars");
    //Sextractor Parameters
    addColumnToTable(ui->solutionTable,"Shape");
    addColumnToTable(ui->solutionTable,"Kron");
    addColumnToTable(ui->solutionTable,"Subpix");
    addColumnToTable(ui->solutionTable,"r_min");
    addColumnToTable(ui->solutionTable,"minarea");
    addColumnToTable(ui->solutionTable,"d_thresh");
    addColumnToTable(ui->solutionTable,"d_cont");
    addColumnToTable(ui->solutionTable,"clean");
    addColumnToTable(ui->solutionTable,"clean param");
    addColumnToTable(ui->solutionTable,"fwhm");
    //Astrometry Parameters
    addColumnToTable(ui->solutionTable,"Pos?");
    addColumnToTable(ui->solutionTable,"Scale?");
    addColumnToTable(ui->solutionTable,"Resort?");
    //Results
    addColumnToTable(ui->solutionTable,"RA");
    addColumnToTable(ui->solutionTable,"DEC");
    addColumnToTable(ui->solutionTable,"Orientation");
    addColumnToTable(ui->solutionTable,"Field Width");
    addColumnToTable(ui->solutionTable,"Field Height");
    addColumnToTable(ui->solutionTable,"Field");

    ui->solutionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

void MainWindow::addSextractionToTable()
{
    QTableWidget *table = ui->solutionTable;
    table->insertRow(table->rowCount());

    double elapsed = solverTimer.elapsed() / 1000.0;

    setItemInColumn(table, "Time", QString::number(elapsed));
    if(extSolver.isNull())
        setItemInColumn(ui->solutionTable, "Int?", "true");
    else
        setItemInColumn(ui->solutionTable, "Int?", "false");
    setItemInColumn(ui->solutionTable, "Stars", QString::number(sexySolver->getNumStarsFound()));
    //Sextractor Parameters
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

    setItemInColumn(ui->solutionTable,"Shape", shapeName);
    setItemInColumn(ui->solutionTable,"Kron", QString::number(sexySolver->kron_fact));
    setItemInColumn(ui->solutionTable,"Subpix", QString::number(sexySolver->subpix));
    setItemInColumn(ui->solutionTable,"r_min", QString::number(sexySolver->r_min));
    setItemInColumn(ui->solutionTable,"minarea", QString::number(sexySolver->minarea));
    setItemInColumn(ui->solutionTable,"d_thresh", QString::number(sexySolver->deblend_thresh));
    setItemInColumn(ui->solutionTable,"d_cont", QString::number(sexySolver->deblend_contrast));
    setItemInColumn(ui->solutionTable,"clean", QString::number(sexySolver->clean));
    setItemInColumn(ui->solutionTable,"clean param", QString::number(sexySolver->clean_param));
    setItemInColumn(ui->solutionTable,"fwhm", QString::number(sexySolver->fwhm));
    setItemInColumn(ui->solutionTable, "Field", ui->fileNameDisplay->text());

}

void MainWindow::addSolutionToTable(Solution solution)
{
    QTableWidget *table = ui->solutionTable;
    table->insertRow(table->rowCount());

    double elapsed = solverTimer.elapsed() / 1000.0;
    setItemInColumn(table, "Time", QString::number(elapsed));

    if(extSolver.isNull())
        setItemInColumn(ui->solutionTable, "Int?", "true");
    else
        setItemInColumn(ui->solutionTable, "Int?", "false");

    setItemInColumn(ui->solutionTable, "Stars", QString::number(sexySolver->getNumStarsFound()));

    //Sextractor Parameters
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
    //Sextractor Parameters
    setItemInColumn(ui->solutionTable,"Shape", shapeName);
    setItemInColumn(ui->solutionTable,"Kron", QString::number(sexySolver->kron_fact));
    setItemInColumn(ui->solutionTable,"Subpix", QString::number(sexySolver->subpix));
    setItemInColumn(ui->solutionTable,"r_min", QString::number(sexySolver->r_min));
    setItemInColumn(ui->solutionTable,"minarea", QString::number(sexySolver->minarea));
    setItemInColumn(ui->solutionTable,"d_thresh", QString::number(sexySolver->deblend_thresh));
    setItemInColumn(ui->solutionTable,"d_cont", QString::number(sexySolver->deblend_contrast));
    setItemInColumn(ui->solutionTable,"clean", QString::number(sexySolver->clean));
    setItemInColumn(ui->solutionTable,"clean param", QString::number(sexySolver->clean_param));
    setItemInColumn(ui->solutionTable,"fwhm", QString::number(sexySolver->fwhm));

    //Astrometry Parameters
    setItemInColumn(ui->solutionTable, "Pos?", QVariant(sexySolver->use_position).toString());
    setItemInColumn(ui->solutionTable, "Scale?", QVariant(sexySolver->use_scale).toString());
    setItemInColumn(ui->solutionTable, "Resort?", QVariant(sexySolver->resort).toString());

    //Results
    setItemInColumn(table, "RA", solution.rastr);
    setItemInColumn(table, "DEC", solution.decstr);
    setItemInColumn(table, "Orientation", QString::number(solution.orientation));
    setItemInColumn(table, "Field Width", QString::number(solution.fieldWidth));
    setItemInColumn(table, "Field Height", QString::number(solution.fieldHeight));
    setItemInColumn(table, "Field", ui->fileNameDisplay->text());
}

void MainWindow::saveSolutionTable()
{
    if (ui->solutionTable->rowCount() == 0)
        return;

    QUrl exportFile = QFileDialog::getSaveFileUrl(this, "Export Solution Table", dirPath,
                      "CSV File (*.csv)");
    if (exportFile.isEmpty()) // if user presses cancel
        return;
    if (exportFile.toLocalFile().endsWith(QLatin1String(".csv")) == false)
        exportFile.setPath(exportFile.toLocalFile() + ".csv");

    QString path = exportFile.toLocalFile();

    if (QFile::exists(path))
    {
        int r = QMessageBox::question(this,
                QString("A file named \"%1\" already exists. ").arg("Overwrite File?"),
                     "Overwrite it?",
                     exportFile.fileName());
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

    for (int c = 0; c < ui->solutionTable->columnCount(); c++)
    {
        outstream << ui->solutionTable->horizontalHeaderItem(c)->text() << ',';
    }
    outstream << endl;

    for (int r = 0; r < ui->solutionTable->rowCount(); r++)
    {
        for (int c = 0; c < ui->solutionTable->columnCount(); c++)
        {
            QTableWidgetItem *cell = ui->solutionTable->item(r, c);

            if (cell)
                outstream << cell->text() << ',';
        }
        outstream << endl;
    }
    QMessageBox::information(this, "Message", QString("Solution Points Saved as: %1").arg(path));
    file.close();
}











