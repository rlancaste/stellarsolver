#include "fileio.h"
#include <QFileInfo>


fileio::fileio()
{
    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;
}

fileio::~fileio()
{
    if(m_ImageBuffer && !imageBufferTaken)
        deleteImageBuffer();
}

void fileio::deleteImageBuffer()
{
    if(m_ImageBuffer)
    {
        delete[] m_ImageBuffer;
        m_ImageBuffer = nullptr;
    }
}

bool fileio::loadImage(QString fileName)
{
    justLoadBuffer = false;
    QFileInfo newFileInfo(fileName);
    bool success = false;
    if(newFileInfo.suffix() == "fits" || newFileInfo.suffix() == "fit")
        success = loadFits(fileName);
    else
        success = loadOtherFormat(fileName);
    if(success)
        generateQImage();
    return success;
}

bool fileio::loadImageBufferOnly(QString fileName)
{
    justLoadBuffer = true;
    QFileInfo newFileInfo(fileName);
    bool success = false;
    if(newFileInfo.suffix() == "fits" || newFileInfo.suffix() == "fit")
        success = loadFits(fileName);
    else
        success = loadOtherFormat(fileName);

    return success;
}

//This method was copied and pasted and modified from the method privateLoad in fitsdata in KStars
//It loads a FITS file, reads the FITS Headers, and loads the data from the image
bool fileio::loadFits(QString fileName)
{   
    file = fileName;
    int status = 0, anynullptr = 0;
    long naxes[3];

    // Use open diskfile as it does not use extended file names which has problems opening
    // files with [ ] or ( ) in their names.
    if (fits_open_diskfile(&fptr, file.toLocal8Bit(), READONLY, &status))
    {
        logIssue(QString("Error opening fits file %1").arg(file));
        return false;
    }
    else
        stats.size = QFile(file).size();

    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
    {
        logIssue(QString("Could not locate image HDU."));
        fits_close_file(fptr, &status);
        return false;
    }

    int fitsBitPix = 0;
    if (fits_get_img_param(fptr, 3, &fitsBitPix, &(stats.ndim), naxes, &status))
    {
        logIssue(QString("FITS file open error (fits_get_img_param)."));
        fits_close_file(fptr, &status);
        return false;
    }

    if (stats.ndim < 2)
    {
        logIssue("1D FITS images are not supported.");
        fits_close_file(fptr, &status);
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
            logIssue(QString("Bit depth %1 is not supported.").arg(fitsBitPix));
            fits_close_file(fptr, &status);
            return false;
    }

    if (stats.ndim < 3)
        naxes[2] = 1;

    if (naxes[0] == 0 || naxes[1] == 0)
    {
        logIssue(QString("Image has invalid dimensions %1x%2").arg(naxes[0]).arg(naxes[1]));
    }

    stats.width               = static_cast<uint16_t>(naxes[0]);
    stats.height              = static_cast<uint16_t>(naxes[1]);
    stats.channels            = static_cast<uint8_t>(naxes[2]);
    stats.samples_per_channel = stats.width * stats.height;

    m_ImageBufferSize = stats.samples_per_channel * stats.channels * static_cast<uint16_t>(stats.bytesPerPixel);
    deleteImageBuffer();
    m_ImageBuffer = new uint8_t[m_ImageBufferSize];
    if (m_ImageBuffer == nullptr)
    {
        logIssue(QString("FITSData: Not enough memory for image_buffer channel. Requested: %1 bytes ").arg(m_ImageBufferSize));
        fits_close_file(fptr, &status);
        return false;
    }

    long nelements = stats.samples_per_channel * stats.channels;

    if (fits_read_img(fptr, static_cast<uint16_t>(stats.dataType), 1, nelements, nullptr, m_ImageBuffer, &anynullptr, &status))
    {
        logIssue("Error reading image.");
        fits_close_file(fptr, &status);
        return false;
    }

    if( !justLoadBuffer )
    {
        if(checkDebayer())
            debayer();

        getSolverOptionsFromFITS();

        parseHeader();
    }

    fits_close_file(fptr, &status);

    return true;
}

//This method I wrote combining code from the fits loading method above, the fits debayering method below, and QT
//I also consulted the ImageToFITS method in fitsdata in KStars
//The goal of this method is to load the data from a file that is not FITS format
bool fileio::loadOtherFormat(QString fileName)
{
    file = fileName;
    QImageReader fileReader(file.toLocal8Bit());
    
    if (!fileReader.canRead()) {
        logIssue(QString("Cannot open '%1' (%2)\n").arg(file, fileReader.errorString()));
        return false;
    }


    QImage imageFromFile;
    if(!fileReader.read(&imageFromFile))
    {
        logIssue("Failed to open image.\n");
        return false;
    }

    imageFromFile = imageFromFile.convertToFormat(QImage::Format_RGB32);

    // because we convert it to 32bit-packed RGB with each color 8 bit, we can hardcode this here
    stats.bytesPerPixel = 1;
    stats.dataType      = SEP_TBYTE;


    stats.width = static_cast<uint16_t>(imageFromFile.width());
    stats.height = static_cast<uint16_t>(imageFromFile.height());
    stats.channels = 3;
    stats.ndim = 3;
    stats.samples_per_channel = stats.width * stats.height;
    m_ImageBufferSize = stats.samples_per_channel * stats.channels * static_cast<uint16_t>(stats.bytesPerPixel);
    deleteImageBuffer();
    m_ImageBuffer = new uint8_t[m_ImageBufferSize];
    if (m_ImageBuffer == nullptr)
    {
        logIssue(QString("FITSData: Not enough memory for image_buffer channel. Requested: %1 bytes ").arg(m_ImageBufferSize));
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
bool fileio::checkDebayer()
{
    int status = 0;
    char bayerPattern[64];

    // Let's search for BAYERPAT keyword, if it's not found we return as there is no bayer pattern in this image
    if (fits_read_keyword(fptr, "BAYERPAT", bayerPattern, nullptr, &status))
        return false;

    if (stats.dataType != TUSHORT && stats.dataType != SEP_TBYTE)
    {
        logIssue("Only 8 and 16 bits bayered images supported.");
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
        logIssue(QString("Unsupported bayer pattern %1.").arg(pattern));
        return false;
    }

    fits_read_key(fptr, TINT, "XBAYROFF", &debayerParams.offsetX, nullptr, &status);
    fits_read_key(fptr, TINT, "YBAYROFF", &debayerParams.offsetY, nullptr, &status);

    //HasDebayer = true;

    return true;
}

//This method was copied and pasted from Fitsdata in KStars
//It debayers the image using the methods below
bool fileio::debayer()
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
bool fileio::debayer_8bit()
{
    dc1394error_t error_code;

    uint32_t rgb_size = stats.samples_per_channel * 3 * stats.bytesPerPixel;
    auto * destinationBuffer = new uint8_t[rgb_size];

    auto * bayer_source_buffer      = reinterpret_cast<uint8_t *>(m_ImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<uint8_t *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        logIssue("Unable to allocate memory for temporary bayer buffer.");
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

    error_code = dc1394_bayer_decoding_8bit(dc1394_source, bayer_destination_buffer, stats.width, ds1394_height,
                                            debayerParams.filter,
                                            debayerParams.method);

    if (error_code != DC1394_SUCCESS)
    {
        logIssue(QString("Debayer failed (%1)").arg(error_code));
        stats.channels = 1;
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
            logIssue("Unable to allocate memory for temporary bayer buffer.");
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
bool fileio::debayer_16bit()
{
    dc1394error_t error_code;

    uint32_t rgb_size = stats.samples_per_channel * 3 * stats.bytesPerPixel;
    auto * destinationBuffer = new uint8_t[rgb_size];

    auto * bayer_source_buffer      = reinterpret_cast<uint16_t *>(m_ImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<uint16_t *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        logIssue("Unable to allocate memory for temporary bayer buffer.");
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

    error_code = dc1394_bayer_decoding_16bit(dc1394_source, bayer_destination_buffer, stats.width, ds1394_height,
                 debayerParams.filter,
                 debayerParams.method, 16);

    if (error_code != DC1394_SUCCESS)
    {
        logIssue(QString("Debayer failed (%1)").arg(error_code));
        stats.channels = 1;
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
            logIssue("Unable to allocate memory for temporary bayer buffer.");
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

//This method is copied and pasted and modified from getSolverOptionsFromFITS in Align in KStars
//Then it was split in two parts, the other part was sent to the ExternalExtractorSolver class since the internal solver doesn't need it
//This part extracts the options from the FITS file and prepares them for use by the internal or external solver
bool fileio::getSolverOptionsFromFITS()
{
    int status = 0, fits_ccd_width, fits_ccd_height, fits_binx = 1, fits_biny = 1;
    char comment[128], error_status[512];
    fitsfile *fptr = nullptr;

    double fits_fov_x, fits_fov_y, fov_lower, fov_upper, fits_ccd_hor_pixel = -1,
                                                         fits_ccd_ver_pixel = -1, fits_focal_length = -1;

    status = 0;

    // Use open diskfile as it does not use extended file names which has problems opening
    // files with [ ] or ( ) in their names.
    if (fits_open_diskfile(&fptr, file.toLocal8Bit(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logIssue(QString::fromUtf8(error_status));
        return false;
    }

    status = 0;
    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logIssue(QString::fromUtf8(error_status));
        fits_close_file(fptr, &status);
        return false;
    }

    status = 0;
    if (fits_read_key(fptr, TINT, "NAXIS1", &fits_ccd_width, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logIssue("FITS header: cannot find NAXIS1.");
        fits_close_file(fptr, &status);
        return false;
    }

    status = 0;
    if (fits_read_key(fptr, TINT, "NAXIS2", &fits_ccd_height, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logIssue("FITS header: cannot find NAXIS2.");
        fits_close_file(fptr, &status);
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
            logIssue(QString("FITS header: cannot find OBJCTRA (%1).").arg(QString(error_status)));
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
            logIssue(QString("FITS header: cannot find OBJCTDEC (%1).").arg(QString(error_status)));
        }
    }
    else
    {
        dms deDMS = dms::fromString(objectde_str, true);
        dec       = deDMS.Degrees();
    }

    if(coord_ok)
        position_given = true;

    status = 0;
    double pixelScale = 0;
    // If we have pixel scale in arcsecs per pixel then lets use that directly
    // instead of calculating it from FOCAL length and other information
    if (fits_read_key(fptr, TDOUBLE, "SCALE", &pixelScale, comment, &status) == 0)
    {
        double fov_low  = 0.8 * pixelScale;
        double fov_high = 1.2 * pixelScale;

        scale_given = true;
        scale_low = fov_low;
        scale_high = fov_high;
        scale_units = SSolver::ARCSEC_PER_PIX;

        fits_close_file(fptr, &status);
        return true;
    }

    if (fits_read_key(fptr, TDOUBLE, "FOCALLEN", &fits_focal_length, comment, &status))
    {
        int integer_focal_length = -1;
        if (fits_read_key(fptr, TINT, "FOCALLEN", &integer_focal_length, comment, &status))
        {
            fits_report_error(stderr, status);
            fits_get_errstatus(status, error_status);
            logIssue(QString("FITS header: cannot find FOCALLEN: (%1).").arg(QString(error_status)));
            fits_close_file(fptr, &status);
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
        logIssue(QString("FITS header: cannot find PIXSIZE1 (%1).").arg(QString(error_status)));
        fits_close_file(fptr, &status);
        return false;
    }

    status = 0;
    if (fits_read_key(fptr, TDOUBLE, "PIXSIZE2", &fits_ccd_ver_pixel, comment, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        logIssue(QString("FITS header: cannot find PIXSIZE2 (%1).").arg(QString(error_status)));
        fits_close_file(fptr, &status);
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

    scale_given = true;
    scale_low = fov_lower;
    scale_high = fov_upper;
    scale_units = SSolver::ARCMIN_WIDTH;

    fits_close_file(fptr, &status);

    return true;
}
// This method was copied from KStars Fitsdata to get the fits headers for saving.
bool fileio::parseHeader()
{
    char * header = nullptr;
    int status = 0, nkeys = 0;

    if (fits_hdr2str(fptr, 0, nullptr, 0, &header, &nkeys, &status))
    {
        fits_report_error(stderr, status);
        free(header);
        return false;
    }

    m_HeaderRecords.clear();
    QString recordList = QString(header);

    for (int i = 0; i < nkeys; i++)
    {
        Record oneRecord;
        // Quotes cause issues for simplified below so we're removing them.
        QString record = recordList.mid(i * 80, 80).remove("'");
        QStringList properties = record.split(QRegExp("[=/]"));
        // If it is only a comment
        if (properties.size() == 1)
        {
            oneRecord.key = properties[0].mid(0, 7);
            oneRecord.comment = properties[0].mid(8).simplified();
        }
        else
        {
            oneRecord.key = properties[0].simplified();
            oneRecord.value = properties[1].simplified();

            // Just in case the comment had / or = in it.
            for(int prop = 2; prop<properties.size(); prop++)
            {
                oneRecord.comment += properties[prop].simplified();
                if(prop < properties.size() - 1)
                    oneRecord.comment += " ";
            }

            // Try to guess the value.
            // Test for integer & double. If neither, then leave it as "string".
            bool ok = false;

            // Is it Integer?
            oneRecord.value.toInt(&ok);
            if (ok)
                oneRecord.value.convert(QMetaType::Int);
            else
            {
                // Is it double?
                oneRecord.value.toDouble(&ok);
                if (ok)
                    oneRecord.value.convert(QMetaType::Double);
            }
        }

        m_HeaderRecords.append(oneRecord);
    }

#ifndef _WIN32 //This prevents a crash on windows. This should be fixed since the header should be freed!
    free(header);
#endif

    return true;
}

//This was copied and pasted and modified from ImageToFITS and injectWCS in fitsdata in KStars
bool fileio::saveAsFITS(QString fileName, FITSImage::Statistic &imageStats, uint8_t *imageBuffer, FITSImage::Solution solution, QList<Record> &records, bool hasSolution)
{
    int status = 0;
    fitsfile * new_fptr;

    //I am hoping that this is correct.
    //I"m trying to set these two variables based on the ndim variable since this class doesn't have access to these variables.
    long naxis;
    int channels;
    if (imageStats.ndim < 3)
    {
        channels = 1;
        naxis = 2;
    }
    else
    {
        channels = 3;
        naxis = 3;
    }

    long nelements;
    long naxes[3] = { imageStats.width, imageStats.height, channels };
    char error_status[512] = {0};

    QFileInfo newFileInfo(fileName);
    if(newFileInfo.exists())
        QFile(fileName).remove();

    nelements = imageStats.samples_per_channel * channels;

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, fileName.toLocal8Bit(), &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    int bitpix;
    switch(imageStats.dataType)
    {
    case SEP_TBYTE:
        bitpix = BYTE_IMG;
        break;
    case TSHORT:
        bitpix = SHORT_IMG;
        break;
    case TUSHORT:
        bitpix = USHORT_IMG;
        break;
    case TLONG:
        bitpix = LONG_IMG;
        break;
    case TULONG:
        bitpix = ULONG_IMG;
        break;
    case TFLOAT:
        bitpix = FLOAT_IMG;
        break;
    case TDOUBLE:
        bitpix = DOUBLE_IMG;
        break;
    default:
        bitpix = BYTE_IMG;
    }

    fitsfile *fptr = new_fptr;
    if (fits_create_img(fptr, bitpix, naxis, naxes, &status))
    {
        emit logOutput(QString("fits_create_img failed: %1").arg(error_status));
        status = 0;
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        return false;
    }

    /* Write Data */
    if (fits_write_img(fptr, imageStats.dataType, 1, nelements, const_cast<void *>(reinterpret_cast<const void *>(imageBuffer)), &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    // Copies all the headers except the first few, which CFITSIO writes automatically
    for (int i = 0; i < records.count(); i++)
    {
        QString key = records[i].key;
        QVariant value = records[i].value;
        QString comment = records[i].comment;

        // This should skip the first few headers, which CFITSIO will write anyway.
        // So it avoids duplicating these headers.
        // Note: the two standard COMMENT fields CFITSIO writes are handled below in the case-switch
        if(key == "SIMPLE" ||
                key == "BITPIX" ||
                key.startsWith("NAXIS") ||
                key == "EXTEND" ||
                key == "BZERO" ||
                key == "BSCALE")
            continue;

        switch (value.type())
        {
            case QVariant::Int:
            {
                int number = value.toInt();
                fits_write_key(fptr, TINT, key.toLatin1().constData(), &number, comment.toLatin1().constData(), &status);
            }
            break;

            case QVariant::Double:
            {
                double number = value.toDouble();
                fits_write_key(fptr, TDOUBLE, key.toLatin1().constData(), &number, comment.toLatin1().constData(), &status);
            }
            break;

            case QVariant::String:
            default:
            {
                if(key == "COMMENT" && (value.toString().contains("FITS (Flexible Image Transport System) format") ||
                   value.toString().contains("volume 376")))
                {
                    // Don't duplicate the two standard comment fields CFITSIO writes to the file
                }
                else
                {
                    if(key == "COMMENT")
                    {
                        fits_write_comment(fptr, comment.toLatin1().constData(), &status);
                    }
                    else if(key == "HISTORY")
                    {
                        fits_write_history(fptr, comment.toLatin1().constData(), &status);
                    }
                    else if(key.simplified() == "END")
                    {
                        // Don't write the end or the solution won't get written!
                    }
                    else
                    {
                        fits_write_key(fptr, TSTRING, key.toLatin1().constData(), value.toString().toLatin1().data(), comment.toLatin1().constData(), &status);
                    }
                }
            }
        }
    }

    /* Write keywords */

    //exposure = 1;
    //fits_update_key(fptr, TLONG, "EXPOSURE", &exposure, "Total Exposure Time", &status);

    // NAXIS1
    if (fits_update_key(fptr, TUSHORT, "NAXIS1", &(imageStats.width), "length of data axis 1", &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    // NAXIS2
    if (fits_update_key(fptr, TUSHORT, "NAXIS2", &(imageStats.height), "length of data axis 2", &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    if(hasSolution)
    {

        fits_update_key(fptr, TDOUBLE, "OBJCTRA", &solution.ra, "Object RA", &status);
        fits_update_key(fptr, TDOUBLE, "OBJCTDEC", &solution.dec, "Object DEC", &status);

        int epoch = 2000;

        fits_update_key(fptr, TINT, "EQUINOX", &epoch, "Equinox", &status);

        fits_update_key(fptr, TDOUBLE, "CRVAL1", &solution.ra, "CRVAL1", &status);
        fits_update_key(fptr, TDOUBLE, "CRVAL2", &solution.dec, "CRVAL1", &status);

        char radecsys[8] = "FK5";
        char ctype1[16]  = "RA---TAN";
        char ctype2[16]  = "DEC--TAN";

        fits_update_key(fptr, TSTRING, "RADECSYS", radecsys, "RADECSYS", &status);
        fits_update_key(fptr, TSTRING, "CTYPE1", ctype1, "CTYPE1", &status);
        fits_update_key(fptr, TSTRING, "CTYPE2", ctype2, "CTYPE2", &status);

        double crpix1 = imageStats.width / 2.0;
        double crpix2 = imageStats.height / 2.0;

        fits_update_key(fptr, TDOUBLE, "CRPIX1", &crpix1, "CRPIX1", &status);
        fits_update_key(fptr, TDOUBLE, "CRPIX2", &crpix2, "CRPIX2", &status);

        // Arcsecs per Pixel
        double secpix1 = solution.parity == FITSImage::NEGATIVE ? solution.pixscale : -solution.pixscale;
        double secpix2 = solution.pixscale;

        fits_update_key(fptr, TDOUBLE, "SECPIX1", &secpix1, "SECPIX1", &status);
        fits_update_key(fptr, TDOUBLE, "SECPIX2", &secpix2, "SECPIX2", &status);

        double degpix1 = secpix1 / 3600.0;
        double degpix2 = secpix2 / 3600.0;

        fits_update_key(fptr, TDOUBLE, "CDELT1", &degpix1, "CDELT1", &status);
        fits_update_key(fptr, TDOUBLE, "CDELT2", &degpix2, "CDELT2", &status);

        // Rotation is CW, we need to convert it to CCW per CROTA1 definition
        double rotation = 360 - solution.orientation;
        if (rotation > 360)
            rotation -= 360;

        fits_update_key(fptr, TDOUBLE, "CROTA1", &rotation, "CROTA1", &status);
        fits_update_key(fptr, TDOUBLE, "CROTA2", &rotation, "CROTA2", &status);
    }

    // ISO Date
    if (fits_write_date(fptr, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    fits_flush_file(fptr, &status);

    if(fits_close_file(fptr, &status))
    {
        emit logOutput(QString("Error closing file."));
        return false;
    }

    emit logOutput("Saved FITS file:" + fileName);

    return true;
}

//This method was copied and pasted from Fitsview in KStars
//It sets up the image that will be displayed on the screen
void fileio::generateQImage()
{
    int sampling = 2;
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

    Stretch stretch(static_cast<int>(stats.width),
                    static_cast<int>(stats.height),
                    stats.channels, static_cast<uint16_t>(stats.dataType));

    // Compute new auto-stretch params.
    StretchParams stretchParams = stretch.computeParams(m_ImageBuffer);

    stretch.setParams(stretchParams);
    stretch.run(m_ImageBuffer, &rawImage, sampling);
}

void fileio::logIssue(QString message){

    if(logToSignal)
        emit logOutput(message);
    else
        printf("%s", message.toUtf8().data());
}

uint8_t *fileio::getImageBuffer()
{
    imageBufferTaken = true;
    return m_ImageBuffer;
}


