#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QUuid>
#include <QDebug>
#include <QImageReader>
#include <QTableWidgetItem>
#include <QPainter>

MainWindow::MainWindow() :
    QMainWindow(),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);


   this->show();

    connect(ui->LoadImage,&QAbstractButton::clicked, this, &MainWindow::loadImage );
    connect(ui->SextractStars,&QAbstractButton::clicked, this, &MainWindow::sextractAndDisplay );
    connect(ui->SolveImage,&QAbstractButton::clicked, this, &MainWindow::solveImage );
    connect(ui->Abort,&QAbstractButton::clicked, this, &MainWindow::abort );
    connect(ui->ClearLog,&QAbstractButton::clicked, this, &MainWindow::clearLog );
    connect(ui->zoomIn,&QAbstractButton::clicked, this, &MainWindow::zoomIn );
    connect(ui->zoomOut,&QAbstractButton::clicked, this, &MainWindow::zoomOut );
    connect(ui->AutoScale,&QAbstractButton::clicked, this, &MainWindow::autoScale );

    ui->splitter->setSizes(QList<int>() << ui->splitter->height() << 0 );
    ui->splitter_2->setSizes(QList<int>() << ui->splitter_2->width() << 0 );

    dirPath=QDir::homePath();

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;
}

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
        ui->starList->clear();
        stars.clear();
        ui->splitter_2->setSizes(QList<int>() << ui->splitter_2->width() << 0 );
        ui->fileNameDisplay->setText("Image: " + fileURL);
        initDisplayImage();
        return true;
    }
    return false;
}

bool MainWindow::solveImage()
{
    ui->splitter->setSizes(QList<int>() << ui->splitter->height() /2 << ui->splitter->height() /2 );
    ui->logDisplay->verticalScrollBar()->setValue(ui->logDisplay->verticalScrollBar()->maximum());

    if(sextract())
    {
        if(solveField())
            return true;
        return false;
    }
    return false;
}

void MainWindow::abort()
{
    if(!solver.isNull())
        solver->kill();
    if(!sextractorProcess.isNull())
        sextractorProcess->kill();
    log("Solve Aborted");
}

//This method was copied and pasted and modified from fitsdata in KStars
bool MainWindow::loadFits()
{

    int status = 0, anynull = 0;
    long naxes[3];
    QString errMessage;

        // Use open diskfile as it does not use extended file names which has problems opening
        // files with [ ] or ( ) in their names.
    if (fits_open_diskfile(&fptr, fileToSolve.toLatin1(), READONLY, &status))
    {
        log(QString("Error opening fits file %1").arg(fileToSolve));
        return false;
    }
    else
        stats.size = QFile(fileToSolve).size();

    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
    {
        log(QString("Could not locate image HDU."));
        return false;
    }

    if (fits_get_img_param(fptr, 3, &(stats.bitpix), &(stats.ndim), naxes, &status))
    {
        log(QString("FITS file open error (fits_get_img_param)."));
        return false;
    }

    if (stats.ndim < 2)
    {
        errMessage = "1D FITS images are not supported.";
        QMessageBox::critical(nullptr,"Message",errMessage);
        log(errMessage);
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
            log(errMessage);
            return false;
    }

    if (stats.ndim < 3)
        naxes[2] = 1;

    if (naxes[0] == 0 || naxes[1] == 0)
    {
        errMessage = QString("Image has invalid dimensions %1x%2").arg(naxes[0]).arg(naxes[1]);
        QMessageBox::critical(nullptr,"Message",errMessage);
        log(errMessage);
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
        log(QString("FITSData: Not enough memory for image_buffer channel. Requested: %1 bytes ").arg(m_ImageBufferSize));
        clearImageBuffers();
        return false;
    }

    long nelements = stats.samples_per_channel * m_Channels;

    if (fits_read_img(fptr, static_cast<uint16_t>(m_DataType), 1, nelements, nullptr, m_ImageBuffer, &anynull, &status))
    {
        errMessage = "Error reading image.";
        QMessageBox::critical(nullptr,"Message",errMessage);
        log(errMessage);
        return false;
    }

    if(checkDebayer())
        debayer();

    return true;
}

//This method I wrote combining code from the fits loading method above, the fits debayering method below, and QT
bool MainWindow::loadOtherFormat()
{
    QImageReader fileReader(fileToSolve.toLatin1());

    if (QImageReader::supportedImageFormats().contains(fileReader.format()) == false)
    {
        log("Failed to convert" + fileToSolve + "to FITS since format, " + fileReader.format() + ", is not supported in Qt");
        return false;
    }

    QString errMessage;
    QImage imageFromFile;
    if(!imageFromFile.load(fileToSolve.toLatin1()))
    {
        log("Failed to open image.");
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
                log(errMessage);
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
        log(QString("FITSData: Not enough memory for image_buffer channel. Requested: %1 bytes ").arg(m_ImageBufferSize));
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
//This was copied and pasted from fitsdata in KStars
bool MainWindow::saveAsFITS()
{
    QFileInfo fileInfo(fileToSolve.toLatin1());
    QString newFilename = QDir::tempPath() + "/" + fileInfo.baseName() + "_solve.fits";
    qDebug() << newFilename;

    int status = 0, exttype = 0;
    fitsfile * new_fptr;
    long  fpixel = 1, naxis = rawImage.allGray() ? 2 : 3, nelements, exposure;
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
        log(QString("fits_create_img failed: %1").arg(error_status));
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

    log("Saved FITS file:" + fileToSolve);

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
        log("Only 8 and 16 bits bayered images supported.");
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
        log(QString("Unsupported bayer pattern %1.").arg(pattern));
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
        log("Unable to allocate memory for temporary bayer buffer.");
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
        log(QString("Debayer failed (%1)").arg(error_code));
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
            log("Unable to allocate memory for temporary bayer buffer.");
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
        log("Unable to allocate memory for temporary bayer buffer.");
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
        log(QString("Debayer failed (%1)").arg(error_code));
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
            log("Unable to allocate memory for temporary bayer buffer.");
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

//This method was copied and pasted from Fitsdata in KStars
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
    currentZoom *= 1.5;
    updateImage();
}

void MainWindow::zoomOut()
{
    currentZoom /= 1.5;
    updateImage();
}

//This method was copied and pasted and modified from Fitsdata in KStars
void MainWindow::autoScale()
{
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

void MainWindow::updateImage()
{
    int w = (stats.width + sampling - 1) / sampling;
    int h = (stats.height + sampling - 1) / sampling;
    currentWidth  = static_cast<int> (w * (currentZoom));
    currentHeight = static_cast<int> (h * (currentZoom));
    doStretch(&rawImage);

    scaledImage = rawImage.scaled(currentWidth, currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap renderedImage = QPixmap::fromImage(scaledImage);
    QPainter p(&renderedImage);
    p.setPen(QColor("red"));
    p.setOpacity(0.6);
    for(int star = 0 ; star < stars.size() ; star++)
    {
        int starx = stars.at(star).x() * currentWidth / stats.width ;
        int stary = stars.at(star).y() * currentHeight / stats.height ;
        int r = 10 * currentWidth / stats.width ;

        p.drawEllipse(starx - r, stary - r , r*2, r*2);
    }
    p.end();

    ui->Image->setPixmap(renderedImage);
}

//This code is copied and pasted from KStars
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

bool MainWindow::sextractAndDisplay()
{
    sextract();
    getSextractorTable();
}

//This method is copied and pasted and modified from the code I wrote to use sextractor in KStars
bool MainWindow::sextract()
{

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
    log("Starting sextractor...");
    log(sextractorBinaryPath + " " + sextractorArgs.join(' '));
    sextractorProcess->waitForFinished();
    log(sextractorProcess->readAllStandardError().trimmed());

    return true;

}

//This method is copied and modified from tablist.c in astrometry.net
bool MainWindow::getSextractorTable()
{
     QFile sextractorFile(sextractorFilePath);
     if(!sextractorFile.exists())
     {
         log("Can't display sextractor file since it doesn't exist.");
         return false;
     }

    fitsfile * new_fptr;
    char error_status[512];

    /* FITS file pointer, defined in fitsio.h */
    char *val, value[1000], nullstr[]="*";
    char keyword[FLEN_KEYWORD], colname[FLEN_VALUE];
    int status = 0;   /*  CFITSIO status value MUST be initialized to zero!  */
    int hdunum, hdutype = ANY_HDU, ncols, ii, anynul, dispwidth[1000];
    long nelements[1000];
    int firstcol, lastcol = 1, linewidth;
    int max_linewidth = 80;
    int elem, firstelem, lastelem = 0, nelems;
    long jj, nrows, kk;
    int quiet = 0;



    if (fits_open_diskfile(&new_fptr, sextractorFilePath.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        log(QString::fromUtf8(error_status));
        return false;
    }

    if ( fits_get_hdu_num(new_fptr, &hdunum) == 1 )
        /* This is the primary array;  try to move to the */
        /* first extension and see if it is a table */
        fits_movabs_hdu(new_fptr, 2, &hdutype, &status);
    else
        fits_get_hdu_type(new_fptr, &hdutype, &status); /* Get the HDU type */

    if (!(hdutype == ASCII_TBL || hdutype == BINARY_TBL)) {
        printf("Error: this program only displays tables, not images\n");
        return -1;
    }

    fits_get_num_rows(new_fptr, &nrows, &status);
    fits_get_num_cols(new_fptr, &ncols, &status);

    for (jj=1; jj<=ncols; jj++)
        fits_get_coltype(new_fptr, jj, NULL, &nelements[jj], NULL, &status);
    //printf("nelements[%i] = %i.\n", (int)jj, (int)nelements[jj]);

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
            fits_get_coltype(new_fptr, lastcol, &typecode, NULL, NULL, &status);
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

        ui->starList->clear();
        stars.clear();

        ui->starList->setColumnCount(4);
        ui->starList->setRowCount(nrows);

        /* print column names as column headers */
        //if (!quiet) {
            printf("\n    ");
            for (ii = firstcol; ii <= lastcol; ii++) {
                int maxelem;
                fits_make_keyn("TTYPE", ii, keyword, &status);
                fits_read_key(new_fptr, TSTRING, keyword, colname, NULL, &status);
                colname[dispwidth[ii]] = '\0';  /* truncate long names */
                kk = ((ii == firstcol) ? firstelem : 1);
                maxelem = ((ii == lastcol) ? lastelem : nelements[ii]);

                for (; kk <= maxelem; kk++) {
                    if (kk > 1) {
                        char buf[32];
                        int len = snprintf(buf, 32, "(%li)", kk);
                        printf("%*.*s%s ",dispwidth[ii]-len, dispwidth[ii]-len, colname, buf);
                        ui->starList->setItem(0,ii,new QTableWidgetItem(QString(colname)));
                    }
                    else
                    {
                        ui->starList->setItem(0,ii,new QTableWidgetItem(QString(colname)));
                        printf("%*s ",dispwidth[ii], colname);
                    }

                }
            }
            printf("\n");  /* terminate header line */
        //}



        /* print each column, row by row (there are faster ways to do this) */
        val = value;
        for (jj = 1; jj <= nrows && !status; jj++) {
            //if (!quiet)
                printf("%4d ", (int)jj);
                ui->starList->setItem(jj,0,new QTableWidgetItem(QString::number(jj)));
            double starx = 0;
            double stary = 0;
            for (ii = firstcol; ii <= lastcol; ii++)
                {
                    kk = ((ii == firstcol) ? firstelem : 1);
                    nelems = ((ii == lastcol) ? lastelem : nelements[ii]);
                    for (; kk <= nelems; kk++)
                        {
                            /* read value as a string, regardless of intrinsic datatype */
                            if (fits_read_col_str (new_fptr,ii,jj,kk, 1, nullstr,
                                                   &val, &anynul, &status) )
                                break;  /* jump out of loop on error */
                            printf("%-*s ",dispwidth[ii], value);
                            ui->starList->setItem(jj,ii,new QTableWidgetItem(QString(value)));
                            if(ii == 2)
                                starx = QString(value).trimmed().toDouble();
                            if(ii == 3)
                                stary = QString(value).trimmed().toDouble();
                        }
                }
            printf("\n");

            stars.append(QPoint(starx, stary));
        }

        if (!breakout)
            break;
    }
    fits_close_file(new_fptr, &status);

    ui->splitter_2->setSizes(QList<int>() << ui->splitter_2->width() / 2 << ui->splitter_2->width() / 2 );
    updateImage();

    if (status) fits_report_error(stderr, status); /* print any error message */
    return(status);
}

QStringList MainWindow::getSolverOptionsFromFITS()
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

    int status = 0, fits_ccd_width, fits_ccd_height, fits_binx = 1, fits_biny = 1;
    char comment[128], error_status[512];
    fitsfile *fptr = nullptr;
    double ra = 0, dec = 0, fits_fov_x, fits_fov_y, fov_lower, fov_upper, fits_ccd_hor_pixel = -1,
           fits_ccd_ver_pixel = -1, fits_focal_length = -1;
    QString fov_low, fov_high;

    status = 0;

    // Use open diskfile as it does not use extended file names which has problems opening
    // files with [ ] or ( ) in their names.
    if (fits_open_diskfile(&fptr, fileToSolve.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        log(QString::fromUtf8(error_status));
        return solverArgs;
    }

    status = 0;
    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        log(QString::fromUtf8(error_status));
        return solverArgs;
    }

    status = 0;
    if (fits_read_key(fptr, TINT, "NAXIS1", &fits_ccd_width, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        log("FITS header: cannot find NAXIS1.");
        return solverArgs;
    }

    status = 0;
    if (fits_read_key(fptr, TINT, "NAXIS2", &fits_ccd_height, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        log("FITS header: cannot find NAXIS2.");
        return solverArgs;
    }

    solverArgs << "--width" << QString::number(fits_ccd_width);
    solverArgs << "--height" << QString::number(fits_ccd_height);
    solverArgs << "--x-column" << "X_IMAGE";
    solverArgs << "--y-column" << "Y_IMAGE";
    solverArgs << "--sort-column" << "MAG_AUTO";
    solverArgs << "--sort-ascending";

                //Note This set of items is NOT NEEDED for Sextractor, it is needed to avoid python usage
                //This may need to be changed later, but since the goal for using sextractor is to avoid python, this is placed here.
    solverArgs << "--no-remove-lines";
    solverArgs << "--uniformize" << "0";

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
            log(QString("FITS header: cannot find OBJCTRA (%1).").arg(QString(error_status)));
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
            log(QString("FITS header: cannot find OBJCTDEC (%1).").arg(QString(error_status)));
        }
    }
    else
    {
        dms deDMS = dms::fromString(objectde_str, true);
        dec       = deDMS.Degrees();
    }

    if (coord_ok)
        solverArgs << "-3" << QString::number(ra * 15.0) << "-4" << QString::number(dec) << "-5" << "15";

    status = 0;
    double pixelScale = 0;
    // If we have pixel scale in arcsecs per pixel then lets use that directly
    // instead of calculating it from FOCAL length and other information
    if (fits_read_key(fptr, TDOUBLE, "SCALE", &pixelScale, comment, &status) == 0)
    {
        fov_low  = QString::number(0.9 * pixelScale);
        fov_high = QString::number(1.1 * pixelScale);

        //if (Options::astrometryUseImageScale())
            solverArgs << "-L" << fov_low << "-H" << fov_high << "-u"
                        << "app";

        return solverArgs;
    }

    if (fits_read_key(fptr, TDOUBLE, "FOCALLEN", &fits_focal_length, comment, &status))
    {
        int integer_focal_length = -1;
        if (fits_read_key(fptr, TINT, "FOCALLEN", &integer_focal_length, comment, &status))
        {
            fits_report_error(stderr, status);
            fits_get_errstatus(status, error_status);
            log(QString("FITS header: cannot find FOCALLEN: (%1).").arg(QString(error_status)));
            return solverArgs;
        }
        else
            fits_focal_length = integer_focal_length;
    }

    status = 0;
    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE1", &fits_ccd_hor_pixel, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        log(QString("FITS header: cannot find PIXSIZE1 (%1).").arg(QString(error_status)));
        return solverArgs;
    }

    status = 0;
    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE2", &fits_ccd_ver_pixel, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        log(QString("FITS header: cannot find PIXSIZE2 (%1).").arg(QString(error_status)));
        return solverArgs;
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

    fov_low  = QString::number(fov_lower);
    fov_high = QString::number(fov_upper);

    //if (Options::astrometryUseImageScale())
        solverArgs << "-L" << fov_low << "-H" << fov_high << "-u"
                    << "aw";

    return solverArgs;
}

bool MainWindow::solveField()
{
    QStringList solverArgs=getSolverOptionsFromFITS();

    QString confPath = "/Applications/KStars.app/Contents/MacOS/astrometry/bin/astrometry.cfg";
#if defined(Q_OS_OSX)
        confPath = "/Applications/KStars.app/Contents/MacOS/astrometry/bin/astrometry.cfg";
#elif defined(Q_OS_LINUX)
        confPath = "$HOME/.local/share/kstars/astrometry/astrometry.cfg";
#endif
    solverArgs << "--config" << confPath;

    QString solutionFile = QDir::tempPath() + "/solution.wcs";
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

    //connect(solver, SIGNAL(finished(int)), this, SLOT(solverComplete(int)));
    solver->setProcessChannelMode(QProcess::MergedChannels);
    connect(solver, SIGNAL(readyReadStandardOutput()), this, SLOT(logSolver()));

    solver->start(solverPath, solverArgs);

    log("Starting solver...");


    QString command = solverPath + ' ' + solverArgs.join(' ');

    log(command);
    return true;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::logSextractor()
{
    QString rawText(sextractorProcess->readAll().trimmed());
    log(rawText.remove("[1M>").remove("[1A"));
}

void MainWindow::logSolver()
{
     log(solver->readAll().trimmed());
}

void MainWindow::clearLog()
{
    ui->logDisplay->clear();
}

void MainWindow::log(QString text)
{
     ui->logDisplay->append(text);
}
