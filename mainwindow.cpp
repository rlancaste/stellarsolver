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

#include <sys/time.h>
#include <time.h>
#include <libgen.h>
#include <getopt.h>
#include <dirent.h>
#include <assert.h>

#include <QThread>
#include <QtConcurrent>


MainWindow::MainWindow() :
    QMainWindow(),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);


   this->show();

    ui->starList->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(ui->LoadImage,&QAbstractButton::clicked, this, &MainWindow::loadImage );

    connect(ui->SextractStars,&QAbstractButton::clicked, this, &MainWindow::sextractImage );
    connect(ui->SolveImage,&QAbstractButton::clicked, this, &MainWindow::solveImage );
    connect(ui->InnerSextract,&QAbstractButton::clicked, this, &MainWindow::sextractInternally);
    connect(ui->InnerSolve,&QAbstractButton::clicked, this, &MainWindow::solveInternally );

    connect(ui->Abort,&QAbstractButton::clicked, this, &MainWindow::abort );
    connect(ui->ClearLog,&QAbstractButton::clicked, this, &MainWindow::clearLog );
    connect(ui->zoomIn,&QAbstractButton::clicked, this, &MainWindow::zoomIn );
    connect(ui->zoomOut,&QAbstractButton::clicked, this, &MainWindow::zoomOut );
    connect(ui->AutoScale,&QAbstractButton::clicked, this, &MainWindow::autoScale );
    connect(ui->showStars,&QAbstractButton::clicked, this, &MainWindow::updateImage );

    connect(ui->starList,&QTableWidget::itemSelectionChanged, this, &MainWindow::starClickedInTable);
    connect(this, SIGNAL(logNeedsUpdating(QString)), this, SLOT(logOutput(QString)), Qt::QueuedConnection);

    ui->splitter->setSizes(QList<int>() << ui->splitter->height() << 0 );
    ui->splitter_2->setSizes(QList<int>() << ui->splitter_2->width() << 0 );

    dirPath=QDir::homePath();

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;
}


MainWindow::~MainWindow()
{
    delete ui;
}


//These methods are for the logging of information to the textfield at the bottom of the window.
//They are used by everything

void MainWindow::logSextractor()
{
    QString rawText(sextractorProcess->readAll().trimmed());
    logOutput(rawText.remove("[1M>").remove("[1A"));
}

void MainWindow::logSolver()
{
     logOutput(solver->readAll().trimmed());
}

void MainWindow::clearLog()
{
    ui->logDisplay->clear();
}

void MainWindow::logOutput(QString text)
{
     ui->logDisplay->append(text);
     ui->logDisplay->show();
}


//These are some utility functions that can be used in all the code below

//I had to create this method because i was having some difficulty turning a QString into a char* that would persist long enough to be used in the program.
char* MainWindow::charQStr(QString in) {
    std::string fname = QString(in).toStdString();
    char* cstr;
    cstr = new char [fname.size()+1];
    strcpy( cstr, fname.c_str() );
    return cstr;
}

//I created this method because I wanted to remove some temp files and display the output and the code was getting repetitive.
void MainWindow::removeTempFile(char * fileName)
{

    if(QFile(fileName).exists())
    {
        QFile(fileName).remove();
        if(QFile(fileName).exists())
            emit logNeedsUpdating(QString("Error: %1 was NOT removed").arg(fileName));
        else
            emit logNeedsUpdating(QString("%1 was removed").arg(fileName));
    }
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
    return true;
}

//I wrote this method to display the table after sextraction has occured.
void MainWindow::displayTable()
{
    sortStars();
    updateStarTableFromList();

    if(ui->splitter_2->sizes().last() < 10)
        ui->splitter_2->setSizes(QList<int>() << ui->splitter_2->width() / 2 << 200 );
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

    if(sextract())
    {
        getSextractorTable();
        displayTable();

        return true;
    }
    return false;
}

//I wrote this method to call the sextract and solve methods below in basically the same way we do in KStars right now.
//It uses the external programs solve-field and sextractor (or sex)
//It times the entire process and prints out how long it took
bool MainWindow::solveImage()
{
    if(!prepareForProcesses())
        return false;

    solverTimer.start();


    if(sextract())
    {
        if(solveField())
            return true;
        return false;
    }
    return false;
}

//I wrote this method to call the internal sextract method below.
//It then will load the results into a table to the right of the image
bool MainWindow::sextractInternally()
{
    if(!prepareForProcesses())
        return false;

    if(runInnerSextractor())
    {
        displayTable();
        return true;
    }
    return false;
}

//I wrote this method to start the internal solver method below
//It runs in a separate thread so that it is nonblocking
//It times the entire process and prints out how long it took
bool MainWindow::solveInternally()
{
    if(!prepareForProcesses())
        return false;

    solverTimer.start();

    if(runInnerSextractor())
    {
        internalSolver.clear();

        //This method can be aborted, but doesn't seem to work on Linux?
      //  internalSolver = QThread::create([this]{ runEngine(); });
      //  internalSolver->start();

        //This method cannot be aborted

        QtConcurrent::run(this, &MainWindow::runEngine);

        return true;
    }
    return false;
}

//This method will abort the sextractor, sovler, and any other processes currently being run, no matter which type
//It will NOT abort the solver if the internal solver is run with QT Concurrent
void MainWindow::abort()
{
    if(!solver.isNull())
        solver->kill();
    if(!sextractorProcess.isNull())
        sextractorProcess->kill();
    if(!internalSolver.isNull())
        internalSolver->terminate();
    logOutput("Solve Aborted");
}





//The following methods deal with the loading and displaying of the image


//I wrote this method to select the file name for the image and call the load methods below to load it
bool MainWindow::loadImage()
{
    QString fileURL = QFileDialog::getOpenFileName(nullptr, "Load Image", dirPath,
                                               "Images (*.fits *.fit *.bmp *.gif *.jpg *.jpeg *.tif *.tiff)");
    if (fileURL.isEmpty())
        return false;
    QFileInfo fileInfo(fileURL);
    if(!fileInfo.exists())
        return false;
    QString newFileURL=QDir::tempPath() + "/" + fileInfo.fileName().remove(" ");
    QFile::copy(fileURL, newFileURL);
    QFileInfo newFileInfo(newFileURL);
    dirPath = fileInfo.absolutePath();
    fileToSolve = newFileURL;

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
        ui->splitter_2->setSizes(QList<int>() << ui->splitter_2->width() << 0 );
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

    if (fits_get_img_param(fptr, 3, &(stats.bitpix), &(stats.ndim), naxes, &status))
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

    switch (stats.bitpix)
    {
        case BYTE_IMG:
            m_DataType           = TBYTE;
            stats.bytesPerPixel = sizeof(uint8_t);
            break;
        case SHORT_IMG:
            // Read SHORT image as USHORT
            m_DataType           = TUSHORT;
            stats.bytesPerPixel = sizeof(int16_t);
            break;
        case USHORT_IMG:
            m_DataType           = TUSHORT;
            stats.bytesPerPixel = sizeof(uint16_t);
            break;
        case LONG_IMG:
            // Read LONG image as ULONG
            m_DataType           = TULONG;
            stats.bytesPerPixel = sizeof(int32_t);
            break;
        case ULONG_IMG:
            m_DataType           = TULONG;
            stats.bytesPerPixel = sizeof(uint32_t);
            break;
        case FLOAT_IMG:
            m_DataType           = TFLOAT;
            stats.bytesPerPixel = sizeof(float);
            break;
        case LONGLONG_IMG:
            m_DataType           = TLONGLONG;
            stats.bytesPerPixel = sizeof(int64_t);
            break;
        case DOUBLE_IMG:
            m_DataType           = TDOUBLE;
            stats.bytesPerPixel = sizeof(double);
            break;
        default:
            errMessage = QString("Bit depth %1 is not supported.").arg(stats.bitpix);
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

    if (fits_read_img(fptr, static_cast<uint16_t>(m_DataType), 1, nelements, nullptr, m_ImageBuffer, &anynullptr, &status))
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
    use_position = false;
    ra = 0;
    dec = 0;
    use_scale = false;
    fov_low = "";
    fov_high = "";

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

    stats.bitpix = 8; //Note: This will need to be changed.  I think QT only loads 8 bpp images.  Also the depth method gives the total bits per pixel in the image not just the bits per pixel in each channel.
    switch (stats.bitpix)
        {
            case BYTE_IMG:
                m_DataType           = TBYTE;
                stats.bytesPerPixel = sizeof(uint8_t);
                break;
            case SHORT_IMG:
                // Read SHORT image as USHORT
                m_DataType           = TUSHORT;
                stats.bytesPerPixel = sizeof(int16_t);
                break;
            case USHORT_IMG:
                m_DataType           = TUSHORT;
                stats.bytesPerPixel = sizeof(uint16_t);
                break;
            case LONG_IMG:
                // Read LONG image as ULONG
                m_DataType           = TULONG;
                stats.bytesPerPixel = sizeof(int32_t);
                break;
            case ULONG_IMG:
                m_DataType           = TULONG;
                stats.bytesPerPixel = sizeof(uint32_t);
                break;
            case FLOAT_IMG:
                m_DataType           = TFLOAT;
                stats.bytesPerPixel = sizeof(float);
                break;
            case LONGLONG_IMG:
                m_DataType           = TLONGLONG;
                stats.bytesPerPixel = sizeof(int64_t);
                break;
            case DOUBLE_IMG:
                m_DataType           = TDOUBLE;
                stats.bytesPerPixel = sizeof(double);
                break;
            default:
                errMessage = QString("Bit depth %1 is not supported.").arg(stats.bitpix);
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
    QString newFilename = QDir::tempPath() + "/" + fileInfo.baseName() + "_solve.fits";

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
    if (fits_write_img(fptr, m_DataType, 1, nelements, m_ImageBuffer, &status))
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

//This method was copied and pasted from Fitsdata in KStars
bool MainWindow::checkDebayer()
{
    int status = 0;
    char bayerPattern[64];

    // Let's search for BAYERPAT keyword, if it's not found we return as there is no bayer pattern in this image
    if (fits_read_keyword(fptr, "BAYERPAT", bayerPattern, nullptr, &status))
        return false;

    if (stats.bitpix != 16 && stats.bitpix != 8)
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
    switch (m_DataType)
    {
        case TBYTE:
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

    double width = ui->scrollArea->rect().width();
    double height = ui->scrollArea->rect().height() - 30; //The minus 30 is due to the image filepath label

    // Find the zoom level which will enclose the current FITS in the current window size
    double zoomX                  = ( width / w);
    double zoomY                  = ( height / h);
    (zoomX < zoomY) ? currentZoom = zoomX : currentZoom = zoomY;

    updateImage();
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
        for(int star = 0 ; star < stars.size() ; star++)
        {
            int starx = static_cast<int>(round(stars.at(star).x * currentWidth / stats.width)) ;
            int stary = static_cast<int>(round(stars.at(star).y * currentHeight / stats.height)) ;
            int r = 10 * currentWidth / stats.width ;

            if(star == selectedStar - 1)
            {
                QPen highlighter(QColor("yellow"));
                highlighter.setWidth(4);
                p.setPen(highlighter);
                p.setOpacity(1);
                p.drawEllipse(starx - r, stary - r , r*2, r*2);
            }
            else
            {
                p.setPen(QColor("red"));
                p.setOpacity(0.6);
                p.drawEllipse(starx - r, stary - r , r*2, r*2);
            }
        }
        p.end();
    }

    ui->Image->setPixmap(renderedImage);
}

//This code is copied and pasted from FITSView in KStars
void MainWindow::doStretch(QImage *outputImage)
{
    if (outputImage->isNull())
        return;
    Stretch stretch(static_cast<int>(stats.width),
                    static_cast<int>(stats.height),
                    m_Channels, static_cast<uint16_t>(m_DataType));

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




//I wrote these methods to load the sextracted stars into a table to the right of the image


//THis method responds to row selections in the table and higlights the star you select in the image
void MainWindow::starClickedInTable()
{
    if(ui->starList->selectedItems().count() > 0)
    {
        QTableWidgetItem *currentItem = ui->starList->selectedItems().first();
        selectedStar = ui->starList->row(currentItem);
        updateImage();
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
    selectedStar = 0;
    ui->starList->clear();
    ui->starList->setColumnCount(4);
    ui->starList->setRowCount(stars.count()+1);
    ui->starList->setItem(0,1,new QTableWidgetItem(QString("MAG_AUTO")));
    ui->starList->setItem(0,2,new QTableWidgetItem(QString("X_IMAGE")));
    ui->starList->setItem(0,3,new QTableWidgetItem(QString("Y_IMAGE")));

    for(int i = 0; i < stars.size(); i ++)
    {
        Star star = stars.at(i);
        ui->starList->setItem(i+1,0,new QTableWidgetItem(QString::number(i+1)));
        ui->starList->setItem(i+1,1,new QTableWidgetItem(QString::number(star.mag)));
        ui->starList->setItem(i+1,2,new QTableWidgetItem(QString::number(star.x)));
        ui->starList->setItem(i+1,3,new QTableWidgetItem(QString::number(star.y)));
    }
}

//This method is copied and pasted and modified from tablist.c in astrometry.net
//This is needed to load in the stars sextracted by an extrnal sextractor to get them into the table
bool MainWindow::getSextractorTable()
{
     QFile sextractorFile(sextractorFilePath);
     if(!sextractorFile.exists())
     {
         logOutput("Can't display sextractor file since it doesn't exist.");
         return false;
     }

    fitsfile * new_fptr;
    char error_status[512];

    /* FITS file pointer, defined in fitsio.h */
    char *val, value[1000], nullptrstr[]="*";
    char keyword[FLEN_KEYWORD], colname[FLEN_VALUE];
    int status = 0;   /*  CFITSIO status value MUST be initialized to zero!  */
    int hdunum, hdutype = ANY_HDU, ncols, ii, anynul, dispwidth[1000];
    long nelements[1000];
    int firstcol, lastcol = 1, linewidth;
    int max_linewidth = 80;
    int elem, firstelem, lastelem = 0, nelems;
    long jj, nrows, kk;

    if (fits_open_diskfile(&new_fptr, sextractorFilePath.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logOutput(QString::fromUtf8(error_status));
        return false;
    }

    if ( fits_get_hdu_num(new_fptr, &hdunum) == 1 )
        /* This is the primary array;  try to move to the */
        /* first extension and see if it is a table */
        fits_movabs_hdu(new_fptr, 2, &hdutype, &status);
    else
        fits_get_hdu_type(new_fptr, &hdutype, &status); /* Get the HDU type */

    if (!(hdutype == ASCII_TBL || hdutype == BINARY_TBL)) {
        return false;
    }

    fits_get_num_rows(new_fptr, &nrows, &status);
    fits_get_num_cols(new_fptr, &ncols, &status);

    for (jj=1; jj<=ncols; jj++)
        fits_get_coltype(new_fptr, jj, nullptr, &nelements[jj], nullptr, &status);


    /* find the number of columns that will fit within max_linewidth
     characters */
    for (;;) {
        int breakout = 0;
        linewidth = 0;
        /* go on to the next element in the current column. */
        /* (if such an element does not exist, the inner 'for' loop
         does not get run and we skip to the next column.) */
        firstcol = lastcol;
        firstelem = lastelem + 1;
        elem = firstelem;

        for (lastcol = firstcol; lastcol <= ncols; lastcol++) {
            int typecode;
            fits_get_col_display_width(new_fptr, lastcol, &dispwidth[lastcol], &status);
            fits_get_coltype(new_fptr, lastcol, &typecode, nullptr, nullptr, &status);
            typecode = abs(typecode);
            if (typecode == TBIT)
                nelements[lastcol] = (nelements[lastcol] + 7)/8;
            else if (typecode == TSTRING)
                nelements[lastcol] = 1;
            nelems = nelements[lastcol];
            for (lastelem = elem; lastelem <= nelems; lastelem++) {
                int nextwidth = linewidth + dispwidth[lastcol] + 1;
                if (nextwidth > max_linewidth) {
                    breakout = 1;
                    break;
                }
                linewidth = nextwidth;
            }
            if (breakout)
                break;
            /* start at the first element of the next column. */
            elem = 1;
        }

        /* if we exited the loop naturally, (not via break) then include all columns. */
        if (!breakout) {
            lastcol = ncols;
            lastelem = nelements[lastcol];
        }

        if (linewidth == 0)
            break;

        stars.clear();


        /* print each column, row by row (there are faster ways to do this) */
        val = value;
        for (jj = 1; jj <= nrows && !status; jj++) {
                ui->starList->setItem(jj,0,new QTableWidgetItem(QString::number(jj)));
            float starx = 0;
            float stary = 0;
            float mag = 0;
            for (ii = firstcol; ii <= lastcol; ii++)
                {
                    kk = ((ii == firstcol) ? firstelem : 1);
                    nelems = ((ii == lastcol) ? lastelem : nelements[ii]);
                    for (; kk <= nelems; kk++)
                        {
                            /* read value as a string, regardless of intrinsic datatype */
                            if (fits_read_col_str (new_fptr,ii,jj,kk, 1, nullptrstr,
                                                   &val, &anynul, &status) )
                                break;  /* jump out of loop on error */
                            if(ii == 1)
                                mag = QString(value).trimmed().toFloat();
                            if(ii == 2)
                                starx = QString(value).trimmed().toFloat();
                            if(ii == 3)
                                stary = QString(value).trimmed().toFloat();
                        }
                }

            Star star = {starx, stary, mag};

            stars.append(star);
        }

        if (!breakout)
            break;
    }
    fits_close_file(new_fptr, &status);

    if (status) fits_report_error(stderr, status); /* print any error message */
    return(status);
}










//These methods will get the solver options from a fits file and prepare them for image solving.



//This method is copied and pasted and modified from getSolverOptionsFromFITS in Align in KStars
//Then it was split in two parts
//Tnis part extracts the options from the FITS file and prepares them.
//It is used by both the internal and external solver
bool MainWindow::getSolverOptionsFromFITS()
{
    ra = 0;
    dec = 0;
    fov_low = "";
    fov_high= "";
    use_scale = false;
    use_position = false;

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
        use_position = true;

    status = 0;
    double pixelScale = 0;
    // If we have pixel scale in arcsecs per pixel then lets use that directly
    // instead of calculating it from FOCAL length and other information
    if (fits_read_key(fptr, TDOUBLE, "SCALE", &pixelScale, comment, &status) == 0)
    {
        fov_low  = QString::number(0.9 * pixelScale);
        fov_high = QString::number(1.1 * pixelScale);
        units = "app";

        use_scale = true;

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
    fov_low  = QString::number(fov_lower);
    fov_high = QString::number(fov_upper);

    units = "aw";

    use_scale = true;

    return true;
}



//This method is copied and pasted and modified from getSolverOptionsFromFITS in Align in KStars
//Then it was split in two parts
//Tnis part generates the argument list from the options for the external solver only
QStringList MainWindow::getSolverArgsList()
{
    QStringList solverArgs;

    // Start with always-used arguments
    solverArgs << "-O"
                << "--no-plots";

    // Now go over boolean options
    solverArgs << "--no-verify";
    solverArgs << "--resort";

    // downsample
    solverArgs << "--downsample" << QString::number(2);


    solverArgs << "--width" << QString::number(stats.width);
    solverArgs << "--height" << QString::number(stats.height);
    solverArgs << "--x-column" << "X_IMAGE";
    solverArgs << "--y-column" << "Y_IMAGE";
    solverArgs << "--sort-column" << "MAG_AUTO";
    solverArgs << "--sort-ascending";

                //Note This set of items is NOT NEEDED for Sextractor, it is needed to avoid python usage
                //This may need to be changed later, but since the goal for using sextractor is to avoid python, this is placed here.
    solverArgs << "--no-remove-lines";
    solverArgs << "--uniformize" << "0";

    if (use_scale)
        solverArgs << "-L" << fov_low << "-H" << fov_high << "-u" << units;

    if (use_position)
        solverArgs << "-3" << QString::number(ra * 15.0) << "-4" << QString::number(dec) << "-5" << "15";

    return solverArgs;
}






//This method is copied and pasted and modified from the code I wrote to use sextractor in OfflineAstrometryParser in KStars
bool MainWindow::sextract()
{
    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

    sextractorFilePath = QDir::tempPath() + "/SextractorList.xyls";
    QFile sextractorFile(sextractorFilePath);
    if(sextractorFile.exists())
        sextractorFile.remove();

    //Configuration arguments for sextractor
    QStringList sextractorArgs;
    //This one is not really that necessary, it will use the defaults if it can't find it
    //We will set all of the things we need in the parameters below
    //sextractorArgs << "-c" << "/usr/local/share/sextractor/default.sex";

    sextractorArgs << "-CATALOG_NAME" << sextractorFilePath;
    sextractorArgs << "-CATALOG_TYPE" << "FITS_1.0";

    //sextractor needs a default.param file in the working directory
    //This creates that file with the options we need for astrometry.net

    QString paramPath =  QDir::tempPath() + "/default.param";
    QFile paramFile(paramPath);
    if(!paramFile.exists())
    {
        if (paramFile.open(QIODevice::WriteOnly) == false)
            QMessageBox::critical(nullptr,"Message","Sextractor file write error.");
        else
        {
            QTextStream out(&paramFile);
            out << "MAG_AUTO                 Kron-like elliptical aperture magnitude                   [mag]\n";
            out << "X_IMAGE                  Object position along x                                   [pixel]\n";
            out << "Y_IMAGE                  Object position along y                                   [pixel]\n";
            paramFile.close();
        }
    }
    sextractorArgs << "-PARAMETERS_NAME" << paramPath;


    //sextractor needs a default.conv file in the working directory
    //This creates the default one

    QString convPath =  QDir::tempPath() + "/default.conv";
    QFile convFile(convPath);
    if(!convFile.exists())
    {
        if (convFile.open(QIODevice::WriteOnly) == false)
            QMessageBox::critical(nullptr,"Message","Sextractor file write error.");
        else
        {
            QTextStream out(&convFile);
            out << "CONV NORM\n";
            out << "1 2 1\n";
            out << "2 4 2\n";
            out << "1 2 1\n";
            convFile.close();
        }
    }
    sextractorArgs << "-FILTER" << "Y";
    sextractorArgs << "-FILTER_NAME" << convPath;
    sextractorArgs << "-MAG_ZEROPOINT" << "20";


    sextractorArgs <<  fileToSolve;

    sextractorProcess.clear();
    sextractorProcess = new QProcess(this);

    QString sextractorBinaryPath;
#if defined(Q_OS_OSX)
        sextractorBinaryPath = "/usr/local/bin/sex";
#elif defined(Q_OS_LINUX)
        sextractorBinaryPath = "/usr/bin/sextractor";
#endif

    sextractorProcess->setWorkingDirectory(QDir::tempPath());

    sextractorProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(sextractorProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(logSextractor()));


    sextractorProcess->start(sextractorBinaryPath, sextractorArgs);
    logOutput("Starting sextractor...");
    logOutput(sextractorBinaryPath + " " + sextractorArgs.join(' '));
    sextractorProcess->waitForFinished();
    logOutput(sextractorProcess->readAllStandardError().trimmed());

    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

    return true;

}



//The code for this method is copied and pasted and modified from OfflineAstroetryParser in KStars
bool MainWindow::solveField()
{
    logOutput("++++++++++++++++++++++++++++++++++++++++++++++");
    sextractorFilePath = QDir::tempPath() + "/SextractorList.xyls";
    QFile sextractorFile(sextractorFilePath);
    if(!sextractorFile.exists())
    {
        logOutput("Please Sextract the image first");
    }

    QStringList solverArgs=getSolverArgsList();

    QString confPath = "/Applications/KStars.app/Contents/MacOS/astrometry/bin/astrometry.cfg";
#if defined(Q_OS_OSX)
        confPath = "/Applications/KStars.app/Contents/MacOS/astrometry/bin/astrometry.cfg";
#elif defined(Q_OS_LINUX)
        confPath = "$HOME/.local/share/kstars/astrometry/astrometry.cfg";
#endif
    solverArgs << "--config" << confPath;

    QString solutionFile = QDir::tempPath() + "/SextractorList.wcs";
    solverArgs << "-W" << solutionFile;

    solverArgs << sextractorFilePath;

    solver.clear();
    solver = new QProcess(this);

    QString solverPath;

#if defined(Q_OS_OSX)
        solverPath = "/usr/local/bin/solve-field";
#elif defined(Q_OS_LINUX)
        solverPath = "/usr/bin/solve-field";
#endif

    connect(solver, SIGNAL(finished(int)), this, SLOT(solverComplete(int)));
    solver->setProcessChannelMode(QProcess::MergedChannels);
    connect(solver, SIGNAL(readyReadStandardOutput()), this, SLOT(logSolver()));

    solver->start(solverPath, solverArgs);

    logOutput("Starting solver...");


    QString command = solverPath + ' ' + solverArgs.join(' ');

    logOutput(command);
    return true;
}

//This was adapted from KStars' OfflineAstrometryParser
bool MainWindow::solverComplete(int x)
{
    double elapsed = solverTimer.elapsed() / 1000.0;
    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    logOutput(QString("External Sextraction and Solving took a total of: %1 second(s).").arg( elapsed));
    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
}







//The code in this section is my attempt at running an internal sextractor program based on SEP
//I used KStars and the SEP website as a guide for creating these functions
//It saves the output to SextractorList.xyls in the temp directory.



bool MainWindow::runInnerSextractor()
{
    logOutput("++++++++++++++++++++++++++++++++++++++++++++++");

    int x = 0, y = 0, w = stats.width, h = stats.height, maxRadius = 50;

    auto * data = new float[w * h];

    switch (stats.bitpix)
    {
        case BYTE_IMG:
            getFloatBuffer<uint8_t>(data, x, y, w, h);
            break;
        case SHORT_IMG:
            getFloatBuffer<int16_t>(data, x, y, w, h);
            break;
        case USHORT_IMG:
            getFloatBuffer<uint16_t>(data, x, y, w, h);
            break;
        case LONG_IMG:
            getFloatBuffer<int32_t>(data, x, y, w, h);
            break;
        case ULONG_IMG:
            getFloatBuffer<uint32_t>(data, x, y, w, h);
            break;
        case FLOAT_IMG:
            delete [] data;
            data = reinterpret_cast<float *>(m_ImageBuffer);
            break;
        case LONGLONG_IMG:
            getFloatBuffer<int64_t>(data, x, y, w, h);
            break;
        case DOUBLE_IMG:
            getFloatBuffer<double>(data, x, y, w, h);
            break;
        default:
            delete [] data;
            return false;
    }

    float * imback = nullptr;
    double * flux = nullptr, *fluxerr = nullptr, *area = nullptr;
    short * flag = nullptr;
    int status = 0;
    sep_bkg * bkg = nullptr;
    sep_catalog * catalog = nullptr;
    float conv[] = {1, 2, 1, 2, 4, 2, 1, 2, 1};

    // #0 Create SEP Image structure
    sep_image im = {data, nullptr, nullptr, SEP_TFLOAT, 0, 0, w, h, 0.0, SEP_NOISE_NONE, 1.0, 0.0};

    // #1 Background estimate
    status = sep_background(&im, 64, 64, 3, 3, 0.0, &bkg);
    if (status != 0) goto exit;

    // #2 Background evaluation
    imback = (float *)malloc((w * h) * sizeof(float));
    status = sep_bkg_array(bkg, imback, SEP_TFLOAT);
    if (status != 0) goto exit;

    // #3 Background subtraction
    status = sep_bkg_subarray(bkg, im.data, im.dtype);
    if (status != 0) goto exit;

    // #4 Source Extraction
    // Note that we set deblend_cont = 1.0 to turn off deblending.
    status = sep_extract(&im, 2 * bkg->globalrms, SEP_THRESH_ABS, 5, conv, 3, 3, SEP_FILTER_CONV, 32, 0.005, 1, 1.0, &catalog);
    if (status != 0) goto exit;

    stars.clear();

    for (int i = 0; i < catalog->nobj; i++)
    {
        double kronrad;
        double kron_fact = 2.5;
        double r = 3;
        short flag;
        int subpix = 5;
        float r_min = 3.5;


        float xPos = catalog->x[i];
        float yPos = catalog->y[i];
        //float a = catalog->cxx[i];
        //float b = catalog->cxy[i];
        //float theta = catalog->cyy[i];

        short inflags;
        double sum;
        double sumerr;
        double area;

       // sep_kron_radius(&im, xPos, yPos, a, b, theta, r, &kronrad, &flag);

        //bool use_circle = kronrad * sqrt(a * b) < r_min;

        //if(use_circle)
        //{
            sep_sum_circle(&im, xPos, yPos, r_min, subpix, inflags, &sum, &sumerr, &area, &flag);
        //}
        //else
        //{
            //sep_sum_ellipse(&im, xPos, yPos, a, b, theta, kron_fact*kronrad, subpix, inflags, &sum, &sumerr, &area, &flag);
        //}

        float magzero = 20;
        float mag = magzero - 2.5 * log10(sum);

        Star star = {xPos, yPos, mag};

        stars.append(star);

    }

    writeSextractorTable();
    logOutput(QString("Successfully sextracted %1 stars.").arg(stars.size()));

    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

    return true;

exit:
    if (stats.bitpix != FLOAT_IMG)
        delete [] data;
    sep_bkg_free(bkg);
    sep_catalog_free(catalog);
    free(imback);
    free(flux);
    free(fluxerr);
    free(area);
    free(flag);

    if (status != 0)
    {
        char errorMessage[512];
        sep_get_errmsg(status, errorMessage);
        logOutput(errorMessage);
        return false;
    }

    return false;
}

template <typename T>
void MainWindow::getFloatBuffer(float * buffer, int x, int y, int w, int h)
{
    auto * rawBuffer = reinterpret_cast<T *>(m_ImageBuffer);

    float * floatPtr = buffer;

    int x2 = x + w;
    int y2 = y + h;

    for (int y1 = y; y1 < y2; y1++)
    {
        int offset = y1 * stats.width;
        for (int x1 = x; x1 < x2; x1++)
        {
            *floatPtr++ = rawBuffer[offset + x1];
        }
    }
}

//This method writes the table to the file
//I had to create it from the examples on NASA's website
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/quick/node10.html
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/cookbook/node16.html
bool MainWindow::writeSextractorTable()
{
    sextractorFilePath = QDir::tempPath() + "/SextractorList.xyls";
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

    int tfields=3;
    int nrows=stars.size();
    QString extname="Sextractor_File";
    //Columns: MAG_AUTO, mag, X_IMAGE, pixels, Y_IMAGE, pixels
    char* ttype[] = { "MAG_AUTO", "X_IMAGE", "Y_IMAGE" };
    char* tform[] = { "1E", "1E", "1E" };
    char* tunit[] = { "mag", "pixels", "pixels" };
    char* extfile = charQStr("Sextractor_File");

    float magArray[stars.size()];
    float xArray[stars.size()];
    float yArray[stars.size()];

    for (int i = 0; i < stars.size(); i++)
    {
        magArray[i] = stars.at(i).mag; //THIS NEEDS TO BE MAGs
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

    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, magArray, &status))
    {
        logOutput(QString("Could not write mag in binary table."));
        return false;
    }
    column = 2;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, xArray, &status))
    {
        logOutput(QString("Could not write x pixels in binary table."));
        return false;
    }

    column = 3;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, yArray, &status))
    {
        logOutput(QString("Could not write y pixels in binary table."));
        return false;
    }

    if(fits_close_file(new_fptr, &status))
    {
        logOutput(QString("Error closing file."));
        return false;
    }

    return true;
}







//All the code below is my attempt to implement astrometry.net code in such a way that it could run internally in a program instead of requiring
//external method calls.  This would be critical for running astrometry.net on windows and it might make the code more efficient on Linux and mac since it
//would not have to prepare command line options and parse them all the time.


//This method was copied and pasted from augment-xylist.c in astrometry.net
void augment_xylist_free_contents(augment_xylist_t* axy) {
    sl_free2(axy->verifywcs);
    il_free(axy->verifywcs_ext);
    sl_free2(axy->tagalong);
    il_free(axy->depths);
    il_free(axy->fields);
    if (axy->predistort)
        sip_free(axy->predistort);
}

//This method was copied and pasted from augment-xylist.c in astrometry.net
void augment_xylist_init(augment_xylist_t* axy) {
    memset(axy, 0, sizeof(augment_xylist_t));
    axy->tempdir = "/tmp";
    axy->tweak = TRUE;
    axy->tweakorder = 2;
    axy->depths = il_new(4);
    axy->fields = il_new(16);
    axy->verifywcs = sl_new(4);
    axy->verifywcs_ext = il_new(4);
    axy->tagalong = sl_new(4);
    axy->try_verify = TRUE;
    axy->resort = TRUE;
    axy->ra_center = HUGE_VAL;
    axy->dec_center = HUGE_VAL;
    axy->parity = PARITY_BOTH;
    axy->uniformize = 10;
    axy->verify_uniformize = TRUE;
}

//This method was copied and pasted from augment-xylist.c in astrometry.net
static void add_sip_coeffs(qfits_header* hdr, const char* prefix, const sip_t* sip) {
    char key[64];
    int m, n, order;

    if (sip->a_order) {
        sprintf(key, "%sSAO", prefix);
        order = sip->a_order;
        fits_header_add_int(hdr, key, order, "SIP forward polynomial order");
        for (m=0; m<=order; m++) {
            for (n=0; (m+n)<=order; n++) {
                if (m+n < 1)
                    continue;
                sprintf(key, "%sA%i%i", prefix, m, n);
                fits_header_add_double(hdr, key, sip->a[m][n], "");
                sprintf(key, "%sB%i%i", prefix, m, n);
                fits_header_add_double(hdr, key, sip->b[m][n], "");
            }
        }
    }
    if (sip->ap_order) {
        order = sip->ap_order;
        sprintf(key, "%sSAPO", prefix);
        fits_header_add_int(hdr, key, order, "SIP reverse polynomial order");
        for (m=0; m<=order; m++) {
            for (n=0; (m+n)<=order; n++) {
                if (m+n < 1)
                    continue;
                sprintf(key, "%sAP%i%i", prefix, m, n);
                fits_header_add_double(hdr, key, sip->ap[m][n], "");
                sprintf(key, "%sBP%i%i", prefix, m, n);
                fits_header_add_double(hdr, key, sip->bp[m][n], "");
            }
        }
    }
}


//This method was adapted from a combination of the main method in augment-xylist-main.c and the method augment_xylist in augment-xylist.c in astrometry.net
bool MainWindow::augmentXYList()
{

    emit logNeedsUpdating("Attempting to make an AXY file from the xyls file");
    char* newfits;
    int i, I;

    qfits_header* hdr = nullptr;

    augment_xylist_init(allaxy);

    // default output filename patterns.

    QString basedir = QDir::tempPath() + "/SextractorList";

    allaxy->xylsfn = charQStr(QString("%1.xyls").arg(basedir));

    allaxy->axyfn    = charQStr(QString("%1.axy").arg(basedir));
    allaxy->matchfn  = charQStr(QString("%1.match").arg(basedir));
    allaxy->rdlsfn   = charQStr(QString("%1.rdls").arg(basedir));
    allaxy->solvedfn = charQStr(QString("%1.solved").arg(basedir));
    allaxy->wcsfn    = charQStr(QString("%1.wcs").arg(basedir));
    allaxy->corrfn   = charQStr(QString("%1.corr").arg(basedir));
    newfits          = charQStr(QString("%1_withWCS.fits").arg(basedir));
        //index_xyls = "%s-indx.xyls";

    emit logNeedsUpdating(("Deleting Temp files"));
    removeTempFile(allaxy->axyfn);
    removeTempFile(allaxy->matchfn);
    removeTempFile(allaxy->rdlsfn);
    removeTempFile(allaxy->solvedfn);
    removeTempFile(allaxy->wcsfn);
    removeTempFile(allaxy->corrfn);



    allaxy->resort=TRUE;
    allaxy->sort_ascending = TRUE;
    allaxy->xcol=strdup("X_IMAGE");
    allaxy->ycol=strdup("Y_IMAGE");
    allaxy->sortcol=strdup("MAG_AUTO");
    allaxy->W=stats.width;
    allaxy->H=stats.height;

    if(use_scale)
    {
        //L
        allaxy->scalelo = fov_low.toDouble();
        //H
        allaxy->scalehi = fov_high.toDouble();
        //u
        if(units == "app")
            allaxy->scaleunit = SCALE_UNITS_ARCSEC_PER_PIX;
        if(units =="aw")
             allaxy->scaleunit = SCALE_UNITS_ARCMIN_WIDTH;
    }

    if(use_position)
    {
        //3
        allaxy->ra_center = ra * 15.0;
        //4
        allaxy->dec_center = dec;
        //5
        allaxy->search_radius = 15;
    }

    // tempfiles to delete when we finish
        sl* tempfiles;

        dl* scales;

        // as we process the image (uncompress it, eg), keep track of extension
        // (along with axy->fitsimgfn if keep_fitsimg is set)
        allaxy->fitsimgext = allaxy->extension;

        tempfiles = sl_new(4);
        scales = dl_new(4);

        char* reason;
        uint8_t isxyls = xylist_is_file_xylist(allaxy->xylsfn, allaxy->extension,
                                       allaxy->xcol, allaxy->ycol, &reason);
        if (!isxyls)
        {
            emit logNeedsUpdating(QString("not xyls because: %1").arg(reason));
            return false;
        }


          //  if (allaxy->extension && (allaxy->extension != 1)) {
                // Copy just this extension to a temp file.
                FILE* fout;
                FILE* fin;
                off_t offset;
                off_t nbytes;
                off_t nprimary;
                anqfits_t* anq;
                char* extfn;

                anq = anqfits_open_hdu(allaxy->xylsfn, allaxy->extension);
                if (!anq) {
                    emit logNeedsUpdating(QString("Failed to open xyls file %1 up to extension %2").arg(
                          allaxy->xylsfn).arg(allaxy->extension));
                    return false;
                }

                fin = fopen(allaxy->xylsfn, "rb");
                if (!fin) {
                    emit logNeedsUpdating(QString("Failed to open xyls file \"%1\"").arg(allaxy->xylsfn));
                    return false;
                }

                nprimary = anqfits_header_size(anq, 0) + anqfits_data_size(anq, 0);
                offset = anqfits_header_start(anq, allaxy->extension);
                nbytes = anqfits_header_size(anq, allaxy->extension) +
                    anqfits_data_size(anq, allaxy->extension);
                anqfits_close(anq);
                extfn = create_temp_file("ext", allaxy->tempdir);
                sl_append_nocopy(tempfiles, extfn);
                fout = fopen(extfn, "wb");
                if (!fout) {
                    emit logNeedsUpdating(QString("Failed to open temp file \"%1\" to write extension").arg(
                             extfn));
                    return false;
                }
                emit logNeedsUpdating(QString("Copying ext %1 of %2 to temp %3\n").arg( allaxy->extension).arg(
                        allaxy->xylsfn).arg( extfn));
                if (pipe_file_offset(fin, 0, nprimary, fout)) {
                    emit logNeedsUpdating(QString("Failed to copy the primary HDU of xylist file %1 to %2").arg(
                          allaxy->xylsfn).arg( extfn));
                    return false;
                }
                if (pipe_file_offset(fin, offset, nbytes, fout)) {
                    emit logNeedsUpdating(QString("Failed to copy HDU %1 of xylist file %2 to %3").arg(
                          allaxy->extension).arg( allaxy->xylsfn).arg(extfn));
                    return false;
                }
                fclose(fin);
                if (fclose(fout)) {
                    emit logNeedsUpdating(QString("Failed to close %1").arg( extfn));
                    return false;
                }
           // }



                char* sortedxylsfn = NULL;



                    if (allaxy->resort) {
                        uint8_t do_tabsort = FALSE;

                        if (!allaxy->sortcol)
                            allaxy->sortcol = "FLUX";
                        if (!allaxy->bgcol)
                            allaxy->bgcol = "BACKGROUND";

                        if (!sortedxylsfn) {
                            sortedxylsfn = create_temp_file("sorted", allaxy->tempdir);
                            sl_append_nocopy(tempfiles, sortedxylsfn);
                        }

                        if (allaxy->resort) {
                            //char* err;
                            int rtn;
                            emit logNeedsUpdating(QString("Sorting file \"%1\" to \"%2\" using columns flux (%3) and background (%4), %5scending\n")
                                    .arg(allaxy->xylsfn).arg( sortedxylsfn).arg( allaxy->sortcol).arg( allaxy->bgcol).arg( allaxy->sort_ascending?"a":"de"));
                            //errors_start_logging_to_string();
                            rtn = resort_xylist(allaxy->xylsfn, sortedxylsfn, allaxy->sortcol, allaxy->bgcol, allaxy->sort_ascending);
                            //err = errors_stop_logging_to_string(": ");
                            if (rtn) {
                                emit logNeedsUpdating(QString("Sorting brightness using %1 and BACKGROUND columns failed; falling back to %2.\n").arg
                                       (allaxy->sortcol).arg (allaxy->sortcol));
                                //emit logNeedsUpdating(QString("Reason: %1\n").arg( err));
                                do_tabsort = TRUE;
                            }
                           // free(err);

                        } else
                            do_tabsort = TRUE;

                        if (do_tabsort) {

                            emit logNeedsUpdating(QString("Sorting by brightness: input=%1, output=%2, column=%3.\n").arg(
                                    allaxy->xylsfn).arg( sortedxylsfn).arg( allaxy->sortcol));
                            tabsort(allaxy->xylsfn, sortedxylsfn,
                                    allaxy->sortcol, !allaxy->sort_ascending);
                        }


                        allaxy->xylsfn = sortedxylsfn;
                    }


                    // start piling FITS headers in there.
                    hdr = anqfits_get_header2(allaxy->xylsfn, 0);
                    if (!hdr) {
                        emit logNeedsUpdating(QString("Failed to read FITS header from file %1").arg (allaxy->xylsfn));
                        exit(-1);
                    }

                    // delete any existing processing directives
                    //delete_existing_an_headers(hdr);


                    // we may write long filenames.
                    fits_header_add_longstring_boilerplate(hdr);

                    //if (addwh) {
                        fits_header_add_int(hdr, "IMAGEW", allaxy->W, "image width");
                        fits_header_add_int(hdr, "IMAGEH", allaxy->H, "image height");
                    //}
                    qfits_header_add(hdr, "ANRUN", "T", "Solve this field!", nullptr);

                    if (allaxy->cpulimit > 0)
                        fits_header_add_double(hdr, "ANCLIM", allaxy->cpulimit, "CPU time limit (seconds)");

                    if (allaxy->xcol)
                        qfits_header_add(hdr, "ANXCOL", allaxy->xcol, "Name of column containing X coords", nullptr);
                    if (allaxy->ycol)
                        qfits_header_add(hdr, "ANYCOL", allaxy->ycol, "Name of column containing Y coords", nullptr);

                    if (allaxy->tagalong_all)
                        qfits_header_add(hdr, "ANTAGALL", "T", "Tag-along all columns from index to RDLS", nullptr);
                    else
                        for (i=0; i<sl_size(allaxy->tagalong); i++) {
                            char key[64];
                            sprintf(key, "ANTAG%i", i+1);
                            qfits_header_add(hdr, key, sl_get(allaxy->tagalong, i), "Tag-along column from index to RDLS", nullptr);
                        }

                    if (allaxy->sort_rdls)
                        qfits_header_add(hdr, "ANRDSORT", allaxy->sort_rdls, "Sort RDLS file by this column", nullptr);

                    qfits_header_add(hdr, "ANVERUNI", allaxy->verify_uniformize ? "T":"F", "Uniformize field during verification", nullptr);
                    qfits_header_add(hdr, "ANVERDUP", allaxy->verify_dedup ? "T":"F", "Deduplicate field during verification", nullptr);

                    if (allaxy->odds_to_tune_up)
                        fits_header_add_double(hdr, "ANODDSTU", allaxy->odds_to_tune_up, "Odds ratio to tune up a match");
                    if (allaxy->odds_to_solve)
                        fits_header_add_double(hdr, "ANODDSSL", allaxy->odds_to_solve, "Odds ratio to consider a field solved");
                    if (allaxy->odds_to_bail)
                        fits_header_add_double(hdr, "ANODDSBL", allaxy->odds_to_bail, "Odds ratio to consider a hypothesis rejected");
                    if (allaxy->odds_to_stoplooking)
                        fits_header_add_double(hdr, "ANODDSST", allaxy->odds_to_stoplooking, "Odds ratio to stop trying to improve the odds ratio");

                    if ((allaxy->scalelo > 0.0) || (allaxy->scalehi > 0.0)) {
                        double appu, appl;
                        switch (allaxy->scaleunit) {
                        case SCALE_UNITS_DEG_WIDTH:
                            emit logNeedsUpdating(QString("Scale range: %1 to %1 degrees wide\n").arg( allaxy->scalelo).arg( allaxy->scalehi));
                            appl = deg2arcsec(allaxy->scalelo) / (double)allaxy->W;
                            appu = deg2arcsec(allaxy->scalehi) / (double)allaxy->W;
                            emit logNeedsUpdating(QString("Image width %i pixels; arcsec per pixel range %1 %2\n").arg( allaxy->W).arg (appl).arg( appu));
                            break;
                        case SCALE_UNITS_ARCMIN_WIDTH:
                            emit logNeedsUpdating(QString("Scale range: %1 to %2 arcmin wide\n").arg (allaxy->scalelo).arg( allaxy->scalehi));
                            appl = arcmin2arcsec(allaxy->scalelo) / (double)allaxy->W;
                            appu = arcmin2arcsec(allaxy->scalehi) / (double)allaxy->W;
                            emit logNeedsUpdating(QString("Image width %i pixels; arcsec per pixel range %1 %2\n").arg (allaxy->W).arg( appl).arg (appu));
                            break;
                        case SCALE_UNITS_ARCSEC_PER_PIX:
                            emit logNeedsUpdating(QString("Scale range: %1 to %2 arcsec/pixel\n").arg (allaxy->scalelo).arg (allaxy->scalehi));
                            appl = allaxy->scalelo;
                            appu = allaxy->scalehi;
                            break;
                        case SCALE_UNITS_FOCAL_MM:
                            emit logNeedsUpdating(QString("Scale range: %1 to %2 mm focal length\n").arg (allaxy->scalelo).arg (allaxy->scalehi));
                            // "35 mm" film is 36 mm wide.
                            appu = rad2arcsec(atan(36. / (2. * allaxy->scalelo))) / (double)allaxy->W;
                            appl = rad2arcsec(atan(36. / (2. * allaxy->scalehi))) / (double)allaxy->W;
                            emit logNeedsUpdating(QString("Image width %i pixels; arcsec per pixel range %1 %2\n").arg (allaxy->W).arg (appl).arg (appu));
                            break;
                        default:
                            emit logNeedsUpdating(QString("Unknown scale unit code %1\n").arg (allaxy->scaleunit));
                            return -1;
                        }
                        dl_append(scales, appl);
                        dl_append(scales, appu);
                    }

                    for (i=0; i<dl_size(scales)/2; i++) {
                        char key[64];
                        double lo = dl_get(scales, 2*i);
                        double hi = dl_get(scales, 2*i + 1);
                        if (lo > 0.0) {
                            sprintf(key, "ANAPPL%i", i+1);
                            fits_header_add_double(hdr, key, lo, "scale: arcsec/pixel min");
                        }
                        if (hi > 0.0) {
                            sprintf(key, "ANAPPU%i", i+1);
                            fits_header_add_double(hdr, key, hi, "scale: arcsec/pixel max");
                        }
                    }

                    if (allaxy->quadsize_min > 0.0)
                        fits_header_add_double(hdr, "ANQSFMIN", allaxy->quadsize_min, "minimum quad size: fraction");
                    if (allaxy->quadsize_max > 0.0)
                        fits_header_add_double(hdr, "ANQSFMAX", allaxy->quadsize_max, "maximum quad size: fraction");

                    if (allaxy->set_crpix) {
                        if (allaxy->set_crpix_center) {
                            qfits_header_add(hdr, "ANCRPIXC", "T", "Set CRPIX to the image center.", nullptr);
                        } else {
                            fits_header_add_double(hdr, "ANCRPIX1", allaxy->crpix[0], "Set CRPIX1 to this val.");
                            fits_header_add_double(hdr, "ANCRPIX2", allaxy->crpix[1], "Set CRPIX2 to this val.");
                        }
                    }

                    qfits_header_add(hdr, "ANTWEAK", (allaxy->tweak ? "T" : "F"), (allaxy->tweak ? "Tweak: yes please!" : "Tweak: no, thanks."), nullptr);
                    if (allaxy->tweak && allaxy->tweakorder)
                        fits_header_add_int(hdr, "ANTWEAKO", allaxy->tweakorder, "Tweak order");

                    if (allaxy->solvedfn)
                        fits_header_addf_longstring(hdr, "ANSOLVED", "solved output file", "%s", allaxy->solvedfn);
                    if (allaxy->solvedinfn)
                        fits_header_addf_longstring(hdr, "ANSOLVIN", "solved input file", "%s", allaxy->solvedinfn);
                    if (allaxy->cancelfn)
                        fits_header_addf_longstring(hdr, "ANCANCEL", "cancel output file", "%s", allaxy->cancelfn);
                    if (allaxy->matchfn)
                        fits_header_addf_longstring(hdr, "ANMATCH", "match output file", "%s", allaxy->matchfn);
                    if (allaxy->rdlsfn)
                        fits_header_addf_longstring(hdr, "ANRDLS", "ra-dec output file", "%s", allaxy->rdlsfn);
                    if (allaxy->scampfn)
                        fits_header_addf_longstring(hdr, "ANSCAMP", "SCAMP reference catalog output file", "%s", allaxy->scampfn);
                    if (allaxy->wcsfn)
                        fits_header_addf_longstring(hdr, "ANWCS", "WCS header output filename", "%s", allaxy->wcsfn);
                    if (allaxy->corrfn)
                        fits_header_addf_longstring(hdr, "ANCORR", "Correspondences output filename", "%s", allaxy->corrfn);
                    if (allaxy->codetol > 0.0)
                        fits_header_add_double(hdr, "ANCTOL", allaxy->codetol, "code tolerance");
                    if (allaxy->pixelerr > 0.0)
                        fits_header_add_double(hdr, "ANPOSERR", allaxy->pixelerr, "star pos'n error (pixels)");

                    if (allaxy->parity != PARITY_BOTH) {
                        if (allaxy->parity == PARITY_NORMAL)
                            qfits_header_add(hdr, "ANPARITY", "POS", "det(CD) > 0", nullptr);
                        else if (allaxy->parity == PARITY_FLIP)
                            qfits_header_add(hdr, "ANPARITY", "NEG", "det(CD) < 0", nullptr);
                    }

                    if ((allaxy->ra_center != HUGE_VAL) &&
                        (allaxy->dec_center != HUGE_VAL) &&
                        (allaxy->search_radius >= 0.0)) {
                        fits_header_add_double(hdr, "ANERA", allaxy->ra_center, "RA center estimate (deg)");
                        fits_header_add_double(hdr, "ANEDEC", allaxy->dec_center, "Dec center estimate (deg)");
                        fits_header_add_double(hdr, "ANERAD", allaxy->search_radius, "Search radius from estimated posn (deg)");
                    }

                    for (i=0; i<il_size(allaxy->depths)/2; i++) {
                        int depthlo, depthhi;
                        char key[64];
                        depthlo = il_get(allaxy->depths, 2*i);
                        depthhi = il_get(allaxy->depths, 2*i + 1);
                        sprintf(key, "ANDPL%i", (i+1));
                        fits_header_addf(hdr, key, "", "%i", depthlo);
                        sprintf(key, "ANDPU%i", (i+1));
                        fits_header_addf(hdr, key, "", "%i", depthhi);
                    }

                    for (i=0; i<il_size(allaxy->fields)/2; i++) {
                        int lo = il_get(allaxy->fields, 2*i);
                        int hi = il_get(allaxy->fields, 2*i + 1);
                        char key[64];
                        if (lo == hi) {
                            sprintf(key, "ANFD%i", (i+1));
                            fits_header_add_int(hdr, key, lo, "field to solve");
                        } else {
                            sprintf(key, "ANFDL%i", (i+1));
                            fits_header_add_int(hdr, key, lo, "field range: low");
                            sprintf(key, "ANFDU%i", (i+1));
                            fits_header_add_int(hdr, key, hi, "field range: high");
                        }
                    }

                    I = 0;
                    for (i=0; i<sl_size(allaxy->verifywcs); i++) {
                        sip_t sip;
                        const char* fn;
                        int ext;

                        fn = sl_get(allaxy->verifywcs, i);
                        ext = il_get(allaxy->verifywcs_ext, i);
                        if (!sip_read_header_file_ext(fn, ext, &sip)) {
                            emit logNeedsUpdating(QString("Failed to parse WCS header from file \"%1\" ext %2").arg( fn).arg(ext));
                            continue;
                        }

                        I++;
                        {
                            tan_t* wcs = &(sip.wcstan);
                            // note, this initialization has to happen *after* you read the WCS header :)
                            double vals[] = { wcs->crval[0], wcs->crval[1],
                                              wcs->crpix[0], wcs->crpix[1],
                                              wcs->cd[0][0], wcs->cd[0][1],
                                              wcs->cd[1][0], wcs->cd[1][1] };
                            char key[64];
                            char* keys[] = { "ANW%iPIX1", "ANW%iPIX2", "ANW%iVAL1", "ANW%iVAL2",
                                             "ANW%iCD11", "ANW%iCD12", "ANW%iCD21", "ANW%iCD22" };
                            int j;
                            for (j = 0; j < 8; j++) {
                                sprintf(key, keys[j], I);
                                fits_header_add_double(hdr, key, vals[j], "");
                            }

                            sprintf(key, "ANW%i", I);
                            add_sip_coeffs(hdr, key, &sip);
                        }
                    }

                    if (allaxy->predistort) {
                        fits_header_add_double(hdr, "ANDPIX0", allaxy->predistort->wcstan.crpix[0], "Pre-distortion ref pix x");
                        fits_header_add_double(hdr, "ANDPIX1", allaxy->predistort->wcstan.crpix[1], "Pre-distortion ref pix y");
                        add_sip_coeffs(hdr, "AND", allaxy->predistort);
                    }

                    fout = fopen(allaxy->axyfn, "wb");
                    if (!fout) {
                        emit logNeedsUpdating(QString("Failed to open output file %1").arg(allaxy->axyfn));
                        exit(-1);
                    }

                    emit logNeedsUpdating(QString("Writing headers to file %1\n").arg( allaxy->axyfn));

                    if (qfits_header_dump(hdr, fout)) {
                        emit logNeedsUpdating(QString("Failed to write FITS header"));
                        exit(-1);
                    }
                    qfits_header_destroy(hdr);

                    // copy blocks from xyls to output.
                    {
                        FILE* fin;
                        off_t offset;
                        off_t nbytes;
                        anqfits_t* anq;
                        int ext = 1;

                        anq = anqfits_open_hdu(allaxy->xylsfn, ext);
                        if (!anq) {
                            emit logNeedsUpdating(QString("Failed to open xyls file %1 up to extension %i").arg (allaxy->xylsfn).arg (ext));
                            exit(-1);
                        }
                        offset = anqfits_header_start(anq, ext);
                        nbytes = anqfits_header_size(anq, ext) + anqfits_data_size(anq, ext);

                        emit logNeedsUpdating(QString("Copying data block of file %1 to output %2.\n").arg(
                                allaxy->xylsfn).arg( allaxy->axyfn));
                        anqfits_close(anq);

                        fin = fopen(allaxy->xylsfn, "rb");
                        if (!fin) {
                            emit logNeedsUpdating(QString("Failed to open xyls file \"%1\"").arg( allaxy->xylsfn));
                            exit(-1);
                        }

                        if (pipe_file_offset(fin, offset, nbytes, fout)) {
                            emit logNeedsUpdating(QString("Failed to copy the data segment of xylist file %1 to %2").arg(
                                  allaxy->xylsfn).arg (allaxy->axyfn));
                            exit(-1);
                        }
                        fclose(fin);
                    }
                    fclose(fout);

                 cleanup:
                    if (!allaxy->no_delete_temp) {
                        for (i=0; i<sl_size(tempfiles); i++) {
                            char* fn = sl_get(tempfiles, i);
                            emit logNeedsUpdating(QString("Deleting temp file %1\n").arg(fn));
                            if (unlink(fn)) {
                                emit logNeedsUpdating(QString("Failed to delete temp file \"%1\"").arg( fn));
                            }
                        }
                    }

                    dl_free(scales);
                    sl_free2(tempfiles);


             //augment_xylist_free_contents(allaxy);

             emit logNeedsUpdating("The AXY file has been created");
             return 0;

}

//This method was adapted from the main method in engine-main.c in astrometry.net
int MainWindow::runEngine()
{
    emit logNeedsUpdating("++++++++++++++++++++++++++++++++++++++++++++++");
    sextractorFilePath = QDir::tempPath() + "/SextractorList.xyls";
    QFile sextractorFile(sextractorFilePath);
    if(!sextractorFile.exists())
    {
        emit logNeedsUpdating("Please Sextract the image first");
    }

    augmentXYList();

    char* theAXYFile =  charQStr(QDir::tempPath() + "/SextractorList.axy");


    int c;
    char* configfn = nullptr;
    int i;
    engine_t* engine;
    char* basedir = nullptr;
    sl* strings = sl_new(4);
    char* cancelfn = nullptr;
    char* solvedfn = nullptr;
    FILE* fin = nullptr;

    sl* inds = sl_new(4);

    engine = engine_new();

    //log_to(stderr);
    //log_init(LOG_MSG);

#if defined(Q_OS_OSX)
        configfn = charQStr("/Applications/KStars.app/Contents/MacOS/astrometry/bin/astrometry.cfg");
#elif defined(Q_OS_LINUX)
        configfn = charQStr(QString("%1/.local/share/kstars/astrometry/astrometry.cfg").arg(QDir::homePath()));
#endif

    basedir = charQStr(QDir::tempPath());

    gslutils_use_error_system();


    if (!streq(configfn, "none")) {
        if (engine_parse_config_file(engine, configfn)) {
            emit logNeedsUpdating(QString("Failed to parse (or encountered an error while interpreting) config file \"%1\"\n").arg( configfn));
            return -1;
        }
    }

    //Removed because of a windows issue with glob, not sure if this is needed or not.
/**
    if (sl_size(inds)) {
        // Expand globs.
        for (i=0; i<sl_size(inds); i++) {
            char* s = sl_get(inds, i);
            glob_t myglob;
            int flags = GLOB_TILDE | GLOB_BRACE;
            if (glob(s, flags, nullptr, &myglob)) {
               emit logNeedsUpdating(QString("Failed to expand wildcards in index-file path \"%1").arg(s));
                return -1;
            }
            for (c=0; c<myglob.gl_pathc; c++) {
                if (engine_add_index(engine, myglob.gl_pathv[c])) {
                    emit logNeedsUpdating(QString("Failed to add index \"%1\"").arg( myglob.gl_pathv[c]));
                    return -1;
                }
            }
            globfree(&myglob);
        }
    }
**/
    if (!pl_size(engine->indexes)) {
        emit logNeedsUpdating(QString("\n\n"
               "---------------------------------------------------------------------\n"
               "You must list at least one index in the config file (%1)\n\n"
               "See http://astrometry.net/use.html about how to get some index files.\n"
               "---------------------------------------------------------------------\n"
               "\n").arg( configfn));
        return -1;
    }



    if (engine->minwidth <= 0.0 || engine->maxwidth <= 0.0) {
        emit logNeedsUpdating(QString("\"minwidth\" and \"maxwidth\" in the config file %1 must be positive!\n").arg( configfn));
        return -1;
    }

    if (!il_size(engine->default_depths)) {
        parse_depth_string(engine->default_depths,
                           "10 20 30 40 50 60 70 80 90 100 "
                           "110 120 130 140 150 160 170 180 190 200");
    }

    engine->cancelfn = cancelfn;
    engine->solvedfn = solvedfn;

    i = optind;
    char* jobfn;
    job_t* job;
    struct timeval tv1, tv2;

    jobfn = theAXYFile;

    gettimeofday(&tv1, nullptr);
    emit logNeedsUpdating(QString("Reading file \"%1\"...\n").arg( jobfn));

    job = engine_read_job_file(engine, jobfn);
    if (!job) {
        emit logNeedsUpdating(QString("Failed to read job file \"%1\"").arg( jobfn));
        return -1;
    }

    if (basedir) {
            emit logNeedsUpdating(QString("Setting job's output base directory to %1\n").arg( basedir));
            job_set_output_base_dir(job, basedir);
    }

    emit logNeedsUpdating("Starting Solver Engine!");

    if (engine_run_job(engine, job))
        emit logNeedsUpdating("Failed to run_job()\n");

    sip_t wcs;
        double ra, dec, fieldw, fieldh;
        char rastr[32], decstr[32];
        char* fieldunits;

    // print info about the field.
        emit logNeedsUpdating(QString("Solved Field: %1").arg(fileToSolve));
        if (file_exists (allaxy->wcsfn)) {
            double orient;
            if  (allaxy->wcs_last_mod) {
                time_t t = file_get_last_modified_time (allaxy->wcsfn);
                if (t == allaxy->wcs_last_mod) {
                    emit logNeedsUpdating("Warning: there was already a WCS file, and its timestamp has not changed.\n");
                }
            }
            if (!sip_read_header_file (allaxy->wcsfn, &wcs)) {
                emit logNeedsUpdating(QString("Failed to read WCS header from file %1").arg( allaxy->wcsfn));
                exit(-1);
            }
            sip_get_radec_center(&wcs, &ra, &dec);
            sip_get_radec_center_hms_string(&wcs, rastr, decstr);
            sip_get_field_size(&wcs, &fieldw, &fieldh, &fieldunits);
            orient = sip_get_orientation(&wcs);
            emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
            emit logNeedsUpdating(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
            emit logNeedsUpdating(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr).arg( decstr));
            emit logNeedsUpdating(QString("Field size: %1 x %2 %3").arg( fieldw).arg( fieldh).arg( fieldunits));
            emit logNeedsUpdating(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
            // Note, negative determinant = positive parity.
            double det = sip_det_cd(&wcs);
            emit logNeedsUpdating(QString("Field parity: %1\n").arg( (det < 0 ? "pos" : "neg")));

        } else {
            emit logNeedsUpdating("Did not solve (or no WCS file was written).\n");
        }

    job_free(job);
    gettimeofday(&tv2, nullptr);
    emit logNeedsUpdating(QString("Spent %1 seconds on this field.\n").arg(millis_between(&tv1, &tv2)/1000.0));
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

    engine_free(engine);
    sl_free2(strings);
    sl_free2(inds);

    if (fin)
        fclose(fin);

    double elapsed = solverTimer.elapsed() / 1000.0;
    emit logNeedsUpdating(QString("Internal Sextraction and Solving took a total of: %1 second(s).").arg( elapsed));
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

    return 0;
}





