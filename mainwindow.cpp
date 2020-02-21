#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QUuid>

MainWindow::MainWindow() :
    QMainWindow(),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);


   this->show();

    connect(ui->LoadImage,&QAbstractButton::clicked, this, &MainWindow::loadImage );
    connect(ui->SolveImage,&QAbstractButton::clicked, this, &MainWindow::solveImage );
    connect(ui->Abort,&QAbstractButton::clicked, this, &MainWindow::abort );
    connect(ui->ClearLog,&QAbstractButton::clicked, this, &MainWindow::clearLog );
    connect(ui->zoomIn,&QAbstractButton::clicked, this, &MainWindow::zoomIn );
    connect(ui->zoomOut,&QAbstractButton::clicked, this, &MainWindow::zoomOut );
    connect(ui->AutoScale,&QAbstractButton::clicked, this, &MainWindow::autoScale );

    ui->splitter->setSizes(QList<int>() << ui->splitter->height() << 0 );

    dirPath=QDir::homePath();
}

bool MainWindow::loadImage()
{
    if(loadFits())
    {
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

bool MainWindow::loadFits()
{
    QString fileURL = QFileDialog::getOpenFileName(nullptr, "Load Image", dirPath,
                                               "Images (*.fits *.fit *.jpg *.jpeg)");
    if (fileURL.isEmpty())
        return false;
    QFileInfo fileInfo(fileURL);
    QString newFileURL=QDir::tempPath() + "/" + fileInfo.fileName().remove(" ");
    QFile::copy(fileURL, newFileURL);
    QFileInfo newFileInfo(newFileURL);
    dirPath = fileInfo.absolutePath();
    fileToSolve = newFileURL;

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

    // Channels always set to #1 if we are not required to process 3D Cubes
    // Or if mode is not FITS_NORMAL (guide, focus..etc)
    //if (m_Mode != FITS_NORMAL || !Options::auto3DCube())
        m_Channels = 1;

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

    return true;
}
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

    doStretch(&rawImage);
    autoScale();

}

void MainWindow::zoomIn()
{
    int w = (stats.width + sampling - 1) / sampling;
    int h = (stats.height + sampling - 1) / sampling;
    currentZoom *= 1.5;
    currentWidth  = static_cast<int> (w * (currentZoom));
    currentHeight = static_cast<int> (h * (currentZoom));
    scaledImage = rawImage.scaled(currentWidth, currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->Image->setPixmap(QPixmap::fromImage(scaledImage));
}

void MainWindow::zoomOut()
{
    int w = (stats.width + sampling - 1) / sampling;
    int h = (stats.height + sampling - 1) / sampling;
    currentZoom /= 1.5;
    currentWidth  = static_cast<int> (w * (currentZoom));
    currentHeight = static_cast<int> (h * (currentZoom));
    scaledImage = rawImage.scaled(currentWidth, currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->Image->setPixmap(QPixmap::fromImage(scaledImage));
}

void MainWindow::autoScale()
{
    int w = (stats.width + sampling - 1) / sampling;
    int h = (stats.height + sampling - 1) / sampling;

    double width = ui->scrollArea->rect().width();
    double height = ui->scrollArea->rect().height();

    // Find the zoom level which will enclose the current FITS in the current window size
    double zoomX                  = ( width / w);
    double zoomY                  = ( height / h);
    (zoomX < zoomY) ? currentZoom = zoomX : currentZoom = zoomY;

    currentWidth  = static_cast<int> (w * (currentZoom));
    currentHeight = static_cast<int> (h * (currentZoom));
    scaledImage = rawImage.scaled(currentWidth, currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->Image->setPixmap(QPixmap::fromImage(scaledImage));
}

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

void MainWindow::clearImageBuffers()
{
    delete[] m_ImageBuffer;
    m_ImageBuffer = nullptr;
    //m_BayerBuffer = nullptr;
}

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
