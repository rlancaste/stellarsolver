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
#include <QToolTip>


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

    connect(ui->Image,&ImageLabel::mouseHovered,this, &MainWindow::mouseOverStar);
    connect(ui->Image,&ImageLabel::mouseClicked,this, &MainWindow::mouseClickedOnStar);

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
            logOutput(QString("Error: %1 was NOT removed").arg(fileName));
        else
            logOutput(QString("%1 was removed").arg(fileName));
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

      runInnerSolver();
      return true;
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

QRect MainWindow::getStarInImage(Star star)
{
    int starx = static_cast<int>(round(star.x * currentWidth / stats.width)) ;
    int stary = static_cast<int>(round(star.y * currentHeight / stats.height)) ;
    int r = 10 * currentWidth / stats.width ;
    return QRect(starx - r, stary - r , r*2, r*2);
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
            QRect starInImage = getStarInImage(stars.at(starnum));

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


void MainWindow::mouseOverStar(QPoint location)
{
    bool starFound = false;
    for(int i = 0 ; i < stars.size() ; i ++)
    {
        Star star = stars.at(i);
        QRect starInImage = getStarInImage(star);
        if(starInImage.contains(location))
        {
            QString text = QString("Star: %1, x: %2, y: %3, mag: %4").arg(i + 1).arg(star.x).arg(star.y).arg(star.mag);
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
    ui->starList->setColumnCount(3);
    ui->starList->setRowCount(stars.count());
    ui->starList->setHorizontalHeaderItem(0,new QTableWidgetItem(QString("MAG_AUTO")));
    ui->starList->setHorizontalHeaderItem(1,new QTableWidgetItem(QString("X_IMAGE")));
    ui->starList->setHorizontalHeaderItem(2,new QTableWidgetItem(QString("Y_IMAGE")));

    for(int i = 0; i < stars.size(); i ++)
    {
        Star star = stars.at(i);
        ui->starList->setItem(i,0,new QTableWidgetItem(QString::number(star.mag)));
        ui->starList->setItem(i,1,new QTableWidgetItem(QString::number(star.x)));
        ui->starList->setItem(i,2,new QTableWidgetItem(QString::number(star.y)));
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
    connect(solver, &QProcess::readyReadStandardOutput, this, &MainWindow::logSolver);

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
    logOutput(QString("Sextraction and Solving took a total of: %1 second(s).").arg( elapsed));
    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
}

bool MainWindow::sextractorComplete()
{
    double elapsed = solverTimer.elapsed() / 1000.0;
    stars = internalSolver->getStarList();
    displayTable();
    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    logOutput(QString("Successfully sextracted %1 stars.").arg(stars.size()));
    logOutput(QString("Sextraction took a total of: %1 second(s).").arg( elapsed));
    logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
}

bool MainWindow::runInnerSextractor()
{
    internalSolver.clear();
    internalSolver = new InternalSolver(fileToSolve, sextractorFilePath, stats, m_ImageBuffer, true, this);
    connect(internalSolver, &InternalSolver::logNeedsUpdating, this, &MainWindow::logOutput, Qt::QueuedConnection);
    connect(internalSolver, &InternalSolver::starsFound, this, &MainWindow::sextractorComplete);
    internalSolver->start();
    return true;
}

bool MainWindow::runInnerSolver()
{
    sextractorFilePath = QDir::tempPath() + "/SextractorList.xyls";

    solverTimer.start();
    internalSolver.clear();
    internalSolver = new InternalSolver(fileToSolve, sextractorFilePath, stats ,m_ImageBuffer, false, this);
    connect(internalSolver, &InternalSolver::logNeedsUpdating, this, &MainWindow::logOutput, Qt::QueuedConnection);
    connect(internalSolver, &InternalSolver::finished, this, &MainWindow::solverComplete);


    augment_xylist_t* allaxy =internalSolver->solverParams();

    // default output filename patterns.

    QString basedir = QDir::tempPath() + "/SextractorList";

    allaxy->axyfn    = charQStr(QString("%1.axy").arg(basedir));
    allaxy->matchfn  = charQStr(QString("%1.match").arg(basedir));
    allaxy->rdlsfn   = charQStr(QString("%1.rdls").arg(basedir));
    allaxy->solvedfn = charQStr(QString("%1.solved").arg(basedir));
    allaxy->wcsfn    = charQStr(QString("%1.wcs").arg(basedir));
    allaxy->corrfn   = charQStr(QString("%1.corr").arg(basedir));

    logOutput("Deleting Temp files");
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


    internalSolver->start();
    return true;
}












