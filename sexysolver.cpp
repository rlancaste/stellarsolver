/*  SexySolver Internal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include "windows.h"
#else //Linux
#include <QProcess>
#endif
#include "sexysolver.h"
#include "qmath.h"

SexySolver::SexySolver(Statistic imagestats, uint8_t *imageBuffer, QObject *parent) : QThread(parent)
{
     stats=imagestats;
     m_ImageBuffer=imageBuffer;

     //This sets the base name used for the temp files.
     srand(time(NULL));
     baseName = "internalSextractorSolver_" + QString::number(rand());
}

SexySolver::~SexySolver()
{
    if(usingDownsampledImage)
        delete downSampledBuffer;
}

//This is the method you want to use to just sextract the image
void SexySolver::sextract()
{
    justSextract = true;
    this->start();
}

//This is the method you want to use to sextract and solve the image
void SexySolver::sextractAndSolve()
{
    justSextract = false;
    this->start();
}

//This is the method that runs the solver or sextractor.  Do not call it, use the methods above instead, so that it can start a new thread.
void SexySolver::run()
{
    if(justSextract)
        runSEPSextractor();
    else
    {
        if(runSEPSextractor())
        {
            runInternalSolver();
        }
    }
}

//This is the abort method.  The way that it works is that it creates a file.  Astrometry.net is monitoring for this file's creation in order to abort.
void SexySolver::abort()
{
    QFile file(cancelfn);
    if(QFileInfo(file).dir().exists())
    {
        file.open(QIODevice::WriteOnly);
        file.write("Cancel");
        file.close();
    }
    quit();
}

//This method uses a fwhm value to generate the conv filter the sextractor will use.
void SexySolver::createConvFilterFromFWHM(Parameters *params, double fwhm)
{
    params->fwhm = fwhm;
    params->convFilter.clear();
    double a = 1;
    int size = abs(ceil(fwhm * 0.6));
    for(int y = -size; y <= size; y++ )
    {
        for(int x = -size; x <= size; x++ )
        {
            double value = a * exp( ( -4.0 * log(2.0) * pow(sqrt( pow(x,2) + pow(y,2) ),2) ) / pow(fwhm,2));
            params->convFilter.append(value);
        }
    }
}

QList<SexySolver::Parameters> SexySolver::getOptionsProfiles()
{
    QList<SexySolver::Parameters> optionsList;

    SexySolver::Parameters smallStars;
    smallStars.listName = "SmallSizedStars";
    smallStars.maxEllipse = 1.2;
    createConvFilterFromFWHM(&smallStars, 1);
    smallStars.r_min = 2;
    smallStars.maxSize = 5;
    optionsList.append(smallStars);

    SexySolver::Parameters mid;
    mid.listName = "MidSizedStars";
    mid.maxEllipse = 1.2;
    mid.minarea = 20;
    createConvFilterFromFWHM(&mid, 4);
    mid.r_min = 5;
    mid.removeDimmest = 20;
    mid.maxSize = 10;
    optionsList.append(mid);

    SexySolver::Parameters big;
    big.listName = "BigSizedStars";
    big.maxEllipse = 1.2;
    big.minarea = 40;
    createConvFilterFromFWHM(&big, 8);
    big.r_min = 20;
    big.removeDimmest = 50;
    optionsList.append(big);

    SexySolver::Parameters down;
    down.listName = "DownSample2";
    down.downsample = 2;
    down.removeDimmest = 50;
    optionsList.append(down);

    return optionsList;
}

//The code in this section is my attempt at running an internal sextractor program based on SEP
//I used KStars and the SEP website as a guide for creating these functions
bool SexySolver::runSEPSextractor()
{
    if(params.convFilter.size() == 0)
    {
        emit logNeedsUpdating("No convFilter included.");
        return false;
    }
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating("Starting Internal SexySolver Sextractor. . .");

    //Only downsample images before SEP if the Sextraction is being used for plate solving
    if(!justSextract && params.downsample != 1)
        downsampleImage(params.downsample);

    int x = 0, y = 0, w = stats.width, h = stats.height, maxRadius = 50;

    auto * data = new float[w * h];

    switch (stats.dataType)
    {
        case SEP_TBYTE:
            getFloatBuffer<uint8_t>(data, x, y, w, h);
            break;
        case TSHORT:
            getFloatBuffer<int16_t>(data, x, y, w, h);
            break;
        case TUSHORT:
            getFloatBuffer<uint16_t>(data, x, y, w, h);
            break;
        case TLONG:
            getFloatBuffer<int32_t>(data, x, y, w, h);
            break;
        case TULONG:
            getFloatBuffer<uint32_t>(data, x, y, w, h);
            break;
        case TFLOAT:
            delete [] data;
            data = reinterpret_cast<float *>(m_ImageBuffer);
            break;
        case TDOUBLE:
            getFloatBuffer<double>(data, x, y, w, h);
            break;
        default:
            delete [] data;
            return false;
    }

    float *imback = nullptr;
    double *fluxerr = nullptr, *area = nullptr;
    short *flag = nullptr;
    int status = 0;
    sep_bkg *bkg = nullptr;
    sep_catalog * catalog = nullptr;

    //These are for the HFR
    double requested_frac[2] = { 0.5, 0.99 };
    double flux_fractions[2] = {0};
    short flux_flag = 0;

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
    status = sep_extract(&im, 2 * bkg->globalrms, SEP_THRESH_ABS, params.minarea, params.convFilter.data(), sqrt(params.convFilter.size()), sqrt(params.convFilter.size()), SEP_FILTER_CONV, params.deblend_thresh, params.deblend_contrast, params.clean, params.clean_param, &catalog);
    if (status != 0) goto exit;

    stars.clear();

    for (int i = 0; i < catalog->nobj; i++)
    {
        //Constant values
        double r = 6;  //The instructions say to use a fixed value of 6: https://sep.readthedocs.io/en/v1.0.x/api/sep.kron_radius.html

        //Variables that are obtained from the catalog
        //FOR SOME REASON, I FOUND THAT THE POSITIONS WERE OFF BY 1 PIXEL??
        //This might be because of this: https://sextractor.readthedocs.io/en/latest/Param.html
        //" Following the FITS convention, in SExtractor the center of the first image pixel has coordinates (1.0,1.0). "
        float xPos = catalog->x[i] + 1;
        float yPos = catalog->y[i] + 1;
        float a = catalog->a[i];
        float b = catalog->b[i];
        float theta = catalog->theta[i];
        double flux = catalog->flux[i];
        double peak = catalog->peak[i];

        //Variables that will be obtained through methods
        double kronrad;
        short flag;
        double sum;
        double sumerr;
        double area;

        //This will need to be done for both auto and ellipse
        if(params.apertureShape != SHAPE_CIRCLE)
        {
            //Finding the kron radius for the sextraction
            sep_kron_radius(&im, xPos, yPos, a, b, theta, r, &kronrad, &flag);
        }

        bool use_circle;

        switch(params.apertureShape)
        {
            case SHAPE_AUTO:
                use_circle = kronrad * sqrt(a * b) < params.r_min;
            break;

            case SHAPE_CIRCLE:
                use_circle = true;
            break;

            case SHAPE_ELLIPSE:
                use_circle = false;
            break;

        }

        if(use_circle)
        {
            sep_sum_circle(&im, xPos, yPos, params.r_min, params.subpix, params.inflags, &sum, &sumerr, &area, &flag);
        }
        else
        {
            sep_sum_ellipse(&im, xPos, yPos, a, b, theta, params.kron_fact*kronrad, params.subpix, params.inflags, &sum, &sumerr, &area, &flag);
        }

        float mag = params.magzero - 2.5 * log10(sum);

        float HFR = 0;
        if(params.calculateHFR)
        {
            //Get HFR
            sep_flux_radius(&im, catalog->x[i], catalog->y[i], maxRadius, params.subpix, 0, &flux, requested_frac, 2, flux_fractions, &flux_flag);
            HFR = flux_fractions[0];
        }

        Star star = {xPos , yPos , mag, (float)sum, (float)peak, HFR, a, b, qRadiansToDegrees(theta)};

        stars.append(star);

    }

    emit logNeedsUpdating(QString("Stars Found before Filtering: %1").arg(stars.size()));

    if(stars.size() > 1)
    {
        if(params.resort)
        {
            //Note that a star is dimmer when the mag is greater!
            //We want to sort in decreasing order though!
            std::sort(stars.begin(), stars.end(), [](const Star &s1, const Star &s2)
            {
                return s1.mag < s2.mag;
            });
        }

        if(params.resort && params.maxSize > 0.0)
        {
            emit logNeedsUpdating(QString("Removing stars wider than %1 pixels").arg(params.maxSize));
            for(int i = 0; i<stars.size();i++)
            {
                Star star = stars.at(i);
                if(star.a > params.maxSize || star.b > params.maxSize)
                {
                    stars.removeAt(i);
                    i--;
                }
            }
        }

        if(params.resort && params.removeBrightest > 0.0 && params.removeBrightest < 100.0)
        {
            int numToRemove = stars.count() * (params.removeBrightest/100.0);
            emit logNeedsUpdating(QString("Removing the %1 brightest stars").arg(numToRemove));
            if(numToRemove > 1)
            {
                for(int i = 0; i<numToRemove;i++)
                    stars.removeFirst();
            }
        }

        if(params.resort && params.removeDimmest > 0.0 && params.removeDimmest < 100.0)
        {
            int numToRemove = stars.count() * (params.removeDimmest/100.0);
            emit logNeedsUpdating(QString("Removing the %1 dimmest stars").arg(numToRemove));
            if(numToRemove > 1)
            {
                for(int i = 0; i<numToRemove;i++)
                    stars.removeLast();
            }
        }

        if(params.maxEllipse > 1)
        {
            emit logNeedsUpdating(QString("Removing the stars with a/b ratios greater than %1").arg(params.maxEllipse));
            for(int i = 0; i<stars.size();i++)
            {
                Star star = stars.at(i);
                double ratio = star.a/star.b;
                if(ratio>params.maxEllipse)
                {
                    stars.removeAt(i);
                    i--;
                }
            }
        }

        if(params.saturationLimit > 0.0 && params.saturationLimit < 100.0)
        {
            double maxSizeofDataType;
            if(stats.dataType == TSHORT || stats.dataType == TLONG || stats.dataType == TLONGLONG)
                maxSizeofDataType = pow(2, stats.bytesPerPixel * 8) / 2 - 1;
            else if(stats.dataType == TUSHORT || stats.dataType == TULONG)
                maxSizeofDataType = pow(2, stats.bytesPerPixel * 8) - 1;
            else // Float and Double Images saturation level is not so easy to determine, especially since they were probably processed by another program and the saturation level is now changed.
                maxSizeofDataType = -1;

            if(maxSizeofDataType == -1)
            {
                emit logNeedsUpdating("Skipping Saturation filter");
            }
            else
            {
                emit logNeedsUpdating(QString("Removing the saturated stars with peak values greater than %1 Percent of %2").arg(params.saturationLimit).arg(maxSizeofDataType));
                for(int i = 0; i<stars.size();i++)
                {
                    Star star = stars.at(i);
                    if(star.peak > (params.saturationLimit/100.0) * maxSizeofDataType)
                    {
                        stars.removeAt(i);
                        i--;
                    }
                }
            }
        }

    }

    emit logNeedsUpdating(QString("Stars Found after Filtering: %1").arg(stars.size()));
    hasSextracted = true;
    if(justSextract)
        emit finished(0);

    if (stats.dataType != TFLOAT)
        delete [] data;
    sep_bkg_free(bkg);
    sep_catalog_free(catalog);
    free(imback);
    free(fluxerr);
    free(area);
    free(flag);
    return true;

exit:
    if (stats.dataType != TFLOAT)
        delete [] data;
    sep_bkg_free(bkg);
    sep_catalog_free(catalog);
    free(imback);
    free(fluxerr);
    free(area);
    free(flag);

    if (status != 0)
    {
        char errorMessage[512];
        sep_get_errmsg(status, errorMessage);
        emit logNeedsUpdating(errorMessage);
        emit finished(-1);
        return false;
    }

    return false;
}

template <typename T>
void SexySolver::getFloatBuffer(float * buffer, int x, int y, int w, int h)
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

void SexySolver::downsampleImage(int d)
{
    switch (stats.dataType)
    {
        case SEP_TBYTE:
            downSampleImageType<uint8_t>(d);
            break;
        case TSHORT:
            downSampleImageType<int16_t>(d);
            break;
        case TUSHORT:
            downSampleImageType<uint16_t>(d);
            break;
        case TLONG:
            downSampleImageType<int32_t>(d);
            break;
        case TULONG:
            downSampleImageType<uint32_t>(d);
            break;
        case TFLOAT:
            downSampleImageType<float>(d);
            break;
        case TDOUBLE:
            downSampleImageType<double>(d);
            break;
        default:
            return;
    }

}

template <typename T>
void SexySolver::downSampleImageType(int d)
{
    int w = stats.width;
    int h = stats.height;

    int numChannels;
    if (stats.ndim < 3)
        numChannels = 1;
    else
        numChannels = 3;
    int oldBufferSize = stats.samples_per_channel * numChannels * stats.bytesPerPixel;
    int newBufferSize = oldBufferSize / (d * d); //It is d times smaller in width and height
    downSampledBuffer =new uint8_t[newBufferSize];
    auto * sourceBuffer = reinterpret_cast<T *>(m_ImageBuffer);
    auto * destinationBuffer = reinterpret_cast<T *>(downSampledBuffer);

    //The G pixels are after all the R pixels, Same for the B pixels
    auto * rSource = sourceBuffer;
    auto * gSource = sourceBuffer + (w * h);
    auto * bSource = sourceBuffer + (w * h * 2);

    for(int y = 0; y < h - d; y+=d)
    {
        for (int x = 0; x < w - d; x+=d)
        {
            //The sum of all the pixels in the sample
            double total = 0;

            for(int y2 = 0; y2 < d; y2++)
            {
                //The offset for the current line of the sample to take, since we have to sample different rows
                int currentLine = w * y2;

                //Shifting the R, G, and B Pointers to the sample location
                auto *rSample = rSource + currentLine + x;
                auto *gSample = gSource + currentLine + x;
                auto *bSample = bSource + currentLine + x;
                for(int x2 = 0; x2 < d; x2++)
                {
                     //This iterates the sample x2 spots to the right,
                    total += *rSample++;
                    //This only samples frome the G and B spots if it is an RGB image
                    if(numChannels == 3)
                    {
                        total += *gSample++;
                        total += *bSample++;
                    }
                }
            }
            //This calculates the average pixel value and puts it in the new downsampled image.
            int pixel = (x/d) + (y/d) * (w/d);
            destinationBuffer[pixel] = total / (d * d) / numChannels;
        }
        //Shifts each pointer by a whole line, d times
        rSource += w * d;
        gSource += w * d;
        bSource += w * d;
    }

    m_ImageBuffer = downSampledBuffer;
    stats.width /= d;
    stats.height /= d;
    scalelo *= d;
    scalehi *= d;
    usingDownsampledImage = true;
}


//All the code below is my attempt to implement astrometry.net code in such a way that it could run internally in a program instead of requiring
//external method calls.  This would be critical for running astrometry.net on windows and it might make the code more efficient on Linux and mac since it
//would not have to prepare command line options and parse them all the time.

//This is a convenience function used to set all the scale parameters based on the FOV high and low values wit their units.
void SexySolver::setSearchScale(double fov_low, double fov_high, QString scaleUnits)
{
    use_scale = true;
    //L
    scalelo = fov_low;
    //H
    scalehi = fov_high;
    //u
    units = scaleUnits;

    if(units == "app" || units == "arcsecperpix")
        scaleunit = SCALE_UNITS_ARCSEC_PER_PIX;
    if(units =="aw" || units =="amw" || units =="arcminwidth")
        scaleunit = SCALE_UNITS_ARCMIN_WIDTH;
    if(units =="dw" || units =="degw" || units =="degwidth")
        scaleunit = SCALE_UNITS_DEG_WIDTH;
    if(units =="focalmm")
        scaleunit = SCALE_UNITS_FOCAL_MM;
}

//This is a convenience function used to set all the search position parameters based on the ra, dec, and radius
//Warning!!  This method accepts the RA in decimal form and then will convert it to degrees for Astrometry.net
void SexySolver::setSearchPosition(double ra, double dec)
{
    use_position = true;
    //3
    search_ra = ra * 15.0;
    //4
    search_dec = dec;
}

void addPathToListIfExists(QStringList *list, QString path)
{
    if(list)
    {
        if(QFileInfo(path).exists())
            list->append(path);
    }
}

QStringList SexySolver::getDefaultIndexFolderPaths()
{
    QStringList indexFilePaths;
#if defined(Q_OS_OSX)
    //Mac Default location
    addPathToListIfExists(&indexFilePaths, QDir::homePath() + "/Library/Application Support/Astrometry");
    //Homebrew location
    addPathToListIfExists(&indexFilePaths, "/usr/local/share/astrometry");
#elif defined(Q_OS_LINUX)
    //Linux Default Location
    addPathToListIfExists(&indexFilePaths, "/usr/share/astrometry/");
    //Linux Local KStars Location
    addPathToListIfExists(&indexFilePaths, QDir::homePath() + "/.local/share/kstars/astrometry/");
#elif defined(_WIN32)
    //Windows Locations
    addPathToListIfExists(&indexFilePaths, QDir::homePath() + "/AppData/Local/cygwin_ansvr/usr/share/astrometry/data");
    addPathToListIfExists(&indexFilePaths, "C:/cygwin/usr/share/astrometry/data");
#endif
    return indexFilePaths;
}

//This method prepares the job file.  It is based upon the methods parse_job_from_qfits_header and engine_read_job_file in engine.c of astrometry.net
//as well as the part of the method augment_xylist in augment_xylist.c where it handles xyls files
bool SexySolver::prepare_job() {
    blind_t* bp = &(job->bp);
    solver_t* sp = &(bp->solver);

    job->scales = dl_new(8);
    job->depths = il_new(8);

    job->use_radec_center = use_position ? TRUE : FALSE;
    if(use_position)
    {
        job->ra_center = search_ra;
        job->dec_center = search_dec;
        job->search_radius = params.search_radius;
    }

    //These initialize the blind and solver objects, and they MUST be in this order according to astrometry.net
    blind_init(bp);
    solver_set_default_values(sp);

    //These set the width and the height of the image in the solver
    sp->field_maxx = stats.width;
    sp->field_maxy = stats.height;

    cancelfn       = basePath + "/" + baseName + ".cancel";
    solvedfn       = basePath + "/" + baseName + ".solved";

    blind_set_cancel_file(bp, cancelfn.toLatin1().constData());
    blind_set_solved_file(bp,solvedfn.toLatin1().constData());

    //Logratios for Solving
    bp->logratio_tosolve = params.logratio_tosolve;
    emit logNeedsUpdating(QString("Set odds ratio to solve to %1 (log = %2)\n").arg( exp(bp->logratio_tosolve)).arg( bp->logratio_tosolve));
    sp->logratio_tokeep = params.logratio_tokeep;
    sp->logratio_totune = params.logratio_totune;
    sp->logratio_bail_threshold = log(DEFAULT_BAIL_THRESHOLD);

    bp->best_hit_only = TRUE;

    // gotta keep it to solve it!
    sp->logratio_tokeep = MIN(sp->logratio_tokeep, bp->logratio_tosolve);

    job->include_default_scales = 0;
    sp->parity = params.search_parity;

    //These set the default tweak settings
    sp->do_tweak = TRUE;
    sp->tweak_aborder = 2;
    sp->tweak_abporder = 2;

    if (use_scale) {
        double appu, appl;
        switch (scaleunit) {
        case SCALE_UNITS_DEG_WIDTH:
            emit logNeedsUpdating(QString("Scale range: %1 to %1 degrees wide\n").arg(scalelo).arg(scalehi));
            appl = deg2arcsec(scalelo) / (double)stats.width;
            appu = deg2arcsec(scalehi) / (double)stats.width;
            emit logNeedsUpdating(QString("Image width %i pixels; arcsec per pixel range %1 %2\n").arg( stats.width).arg (appl).arg( appu));
            break;
        case SCALE_UNITS_ARCMIN_WIDTH:
            emit logNeedsUpdating(QString("Scale range: %1 to %2 arcmin wide\n").arg (scalelo).arg(scalehi));
            appl = arcmin2arcsec(scalelo) / (double)stats.width;
            appu = arcmin2arcsec(scalehi) / (double)stats.width;
            emit logNeedsUpdating(QString("Image width %i pixels; arcsec per pixel range %1 %2\n").arg (stats.width).arg( appl).arg (appu));
            break;
        case SCALE_UNITS_ARCSEC_PER_PIX:
            emit logNeedsUpdating(QString("Scale range: %1 to %2 arcsec/pixel\n").arg (scalelo).arg (scalehi));
            appl = scalelo;
            appu = scalehi;
            break;
        case SCALE_UNITS_FOCAL_MM:
            emit logNeedsUpdating(QString("Scale range: %1 to %2 mm focal length\n").arg (scalelo).arg (scalehi));
            // "35 mm" film is 36 mm wide.
            appu = rad2arcsec(atan(36. / (2. * scalelo))) / (double)stats.width;
            appl = rad2arcsec(atan(36. / (2. * scalehi))) / (double)stats.width;
            emit logNeedsUpdating(QString("Image width %i pixels; arcsec per pixel range %1 %2\n").arg (stats.width).arg (appl).arg (appu));
            break;
        default:
            emit logNeedsUpdating(QString("Unknown scale unit code %1\n").arg (scaleunit));
            return false;
        }
        dl_append(job->scales, appl);
        dl_append(job->scales, appu);
        blind_add_field_range(bp, appl, appu);
    }

    blind_add_field(bp, 1);


    return true;
}

//This method was adapted from the main method in engine-main.c in astrometry.net
int SexySolver::runInternalSolver()
{
    emit logNeedsUpdating("++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating("Configuring SexySolver");

    if(params.inParallel)
    {
        if(enoughRAMisAvailableFor(indexFolderPaths))
            emit logNeedsUpdating("There should be enough RAM to load the indexes in parallel.");
        else
        {
            emit logNeedsUpdating("Not enough RAM is available on this system for loading the index files you have in parallel");
            emit logNeedsUpdating("Disabling the inParallel option.");
            disableInparallel();
        }
    }

    //This creates and sets up the engine
    engine_t* engine = engine_new();

    //This sets some basic engine settings
    engine->inparallel = params.inParallel ? TRUE : FALSE;
    engine->minwidth = params.minwidth;
    engine->maxwidth = params.maxwidth;

    //This sets the logging level and log file based on the user's preferences.
    if(params.logToFile)
    {
        if(params.logFile == "")
            params.logFile = basePath + "/" + baseName + ".log.txt";
        if(QFile(params.logFile).exists())
            QFile(params.logFile).remove();
        FILE *log = fopen(params.logFile.toLatin1().constData(),"wb");
        if(log)
        {
            log_init((log_level)params.logLevel);
            log_to(log);
        }
    }

    //gslutils_use_error_system();

    //These set the folders in which Astrometry.net will look for index files, based on the folers set before the solver was started.
    foreach(QString path, indexFolderPaths)
    {
        engine_add_search_path(engine,path.toLatin1().constData());
    }

    //This actually adds the index files in the directories above.
    engine_autoindex_search_paths(engine);

    //This checks to see that index files were found in the paths above, if not, it prints this warning and aborts.
    if (!pl_size(engine->indexes)) {
        emit logNeedsUpdating(QString("\n\n"
               "---------------------------------------------------------------------\n"
               "You must include at least one index file in the index file directories\n\n"
               "See http://astrometry.net/use.html about how to get some index files.\n"
               "---------------------------------------------------------------------\n"
               "\n"));
        return -1;
    }

    prepare_job();
    engine->cancelfn = cancelfn.toLatin1().data();
    engine->solvedfn = solvedfn.toLatin1().data();

    blind_t* bp = &(job->bp);

    //This will set up the field file to solve as an xylist
    double *xArray = new double[stars.size()];
    double *yArray = new double[stars.size()];

    for (int i = 0; i < stars.size(); i++)
    {
        xArray[i] = stars.at(i).x;
        yArray[i] = stars.at(i).y;
    }

    starxy_t* fieldToSolve = (starxy_t*)calloc(1, sizeof(starxy_t));
    fieldToSolve->x = xArray;
    fieldToSolve->y = yArray;
    fieldToSolve->N = stars.size();
    fieldToSolve->flux = nullptr;
    fieldToSolve->background = nullptr;
    bp->solver.fieldxy = fieldToSolve;

    //This sets the depths for the job.
    if (!il_size(engine->default_depths)) {
        parse_depth_string(engine->default_depths,
                           "10 20 30 40 50 60 70 80 90 100 "
                           "110 120 130 140 150 160 170 180 190 200");
    }
    if (il_size(job->depths) == 0) {
        if (engine->inparallel) {
            // no limit.
            il_append(job->depths, 0);
            il_append(job->depths, 0);
        } else
            il_append_list(job->depths, engine->default_depths);
    }

    //This makes sure the min and max widths for the engine make sense, aborting if not.
    if (engine->minwidth <= 0.0 || engine->maxwidth <= 0.0 || engine->minwidth > engine->maxwidth) {
        emit logNeedsUpdating(QString("\"minwidth\" and \"maxwidth\" must be positive and the maxwidth must be greater!\n"));
        return -1;
    }
    ///This sets the scales based on the minwidth and maxwidth if the image scale isn't known
    if (!dl_size(job->scales)) {
        double arcsecperpix;
        arcsecperpix = deg2arcsec(engine->minwidth) / stats.width;
        dl_append(job->scales, arcsecperpix);
        arcsecperpix = deg2arcsec(engine->maxwidth) / stats.height;
        dl_append(job->scales, arcsecperpix);
    }

    // These set the time limits for the solver
    bp->timelimit = params.solverTimeLimit;
#ifndef _WIN32
    bp->cpulimit = params.solverTimeLimit;
#endif

    // If not running inparallel, set total limits = limits.
    if (!engine->inparallel) {
        bp->total_timelimit = bp->timelimit;
        bp->total_cpulimit  = bp->cpulimit ;
    }

    emit logNeedsUpdating("Starting Internal SexySolver Astrometry.net based Engine. . .");

    //This runs the job in the engine in the file engine.c
    if (engine_run_job(engine, job))
        emit logNeedsUpdating("Failed to run_job()\n");

    //This deletes or frees the items that are no longer needed.
    engine_free(engine);
    bl_free(job->scales);
    dl_free(job->depths);
    free(fieldToSolve);
    delete[] xArray;
    delete[] yArray;

    //This deletes the temporary files
    QDir temp(basePath);
    temp.remove(cancelfn);
    temp.remove(solvedfn);

    //Note: I can only get these items after the solve because I made a couple of small changes to the Astrometry.net Code.
    //I made it return in solve_fields in blind.c before it ran "cleanup".  I also had it wait to clean up solutions, blind and solver in engine.c.  We will do that after we get the solution information.
    MatchObj match =bp->solver.best_match;
    sip_t *wcs = match.sip;
    
    int returnCode = 0;

    if(wcs)
    {
        double ra, dec, fieldw, fieldh, pixscale;
        char rastr[32], decstr[32];
        QString parity;
        char* fieldunits;

        // print info about the field.

        double orient;
        sip_get_radec_center(wcs, &ra, &dec);
        sip_get_radec_center_hms_string(wcs, rastr, decstr);
        sip_get_field_size(wcs, &fieldw, &fieldh, &fieldunits);
        orient = sip_get_orientation(wcs);
        // Note, negative determinant = positive parity.
        double det = sip_det_cd(wcs);
        parity = (det < 0 ? "pos" : "neg");
        if(usingDownsampledImage)
            pixscale = sip_pixel_scale(wcs) / params.downsample;
        else
            pixscale = sip_pixel_scale(wcs);

        double raErr = 0;
        double decErr = 0;
        if(use_position)
        {
            raErr = (search_ra - ra) * 3600;
            decErr = (search_dec - dec) * 3600;
        }

        emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logNeedsUpdating(QString("Solve Log Odds:  %1").arg(bp->solver.best_logodds));
        emit logNeedsUpdating(QString("Number of Matches:  %1").arg(match.nmatch));
        emit logNeedsUpdating(QString("Solved with index:  %1").arg(match.indexid));
        emit logNeedsUpdating(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
        emit logNeedsUpdating(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr).arg( decstr));
        if(use_position)
            emit logNeedsUpdating(QString("Field is: (%1, %2) deg from search coords.").arg( raErr).arg( decErr));
        emit logNeedsUpdating(QString("Field size: %1 x %2 %3").arg( fieldw).arg( fieldh).arg( fieldunits));
        emit logNeedsUpdating(QString("Pixel Scale: %1\"").arg( pixscale));
        emit logNeedsUpdating(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
        emit logNeedsUpdating(QString("Field parity: %1\n").arg( parity));

        solution = {fieldw,fieldh,ra,dec,rastr,decstr,orient, pixscale, parity, raErr, decErr};
        hasSolved = true;
    }
    else
    {
        emit logNeedsUpdating("Solver was aborted, timed out, or failed, so no solution was found");
    }
    
    //This code was taken from engine.c and blind.c so that we can clean up all of the allocated memory after we get the solution information out of it so that we can prevent memory leaks.
    
    for (size_t i=0; i<bl_size(bp->solutions); i++) {
        MatchObj* mo = (MatchObj*)bl_access(bp->solutions, i);
        verify_free_matchobj(mo);
        blind_free_matchobj(mo);
    }
    bl_remove_all(bp->solutions);
    blind_clear_solutions(bp);
    solver_cleanup(&bp->solver);
    blind_cleanup(bp);
    
    emit finished(returnCode);
    return returnCode;
}

QMap<QString, QVariant> SexySolver::convertToMap(Parameters params)
{
    QMap<QString, QVariant> settingsMap;
    settingsMap.insert("listName", QVariant(params.listName));

    //These are to pass the parameters to the internal sextractor
    settingsMap.insert("calculateHFR", QVariant(params.calculateHFR));
    settingsMap.insert("apertureShape", QVariant(params.apertureShape));
    settingsMap.insert("kron_fact", QVariant(params.kron_fact));
    settingsMap.insert("subpix", QVariant(params.subpix));
    settingsMap.insert("r_min", QVariant(params.r_min));
    //params.inflags
    settingsMap.insert("magzero", QVariant(params.magzero));
    settingsMap.insert("minarea", QVariant(params.minarea));
    settingsMap.insert("deblend_thresh", QVariant(params.deblend_thresh));
    settingsMap.insert("deblend_contrast", QVariant(params.deblend_contrast));
    settingsMap.insert("clean", QVariant(params.clean));
    settingsMap.insert("clean_param", QVariant(params.clean_param));

    settingsMap.insert("fwhm", QVariant(params.fwhm));
    QStringList conv;
    foreach(float num, params.convFilter)
    {
        conv << QVariant(num).toString();
    }
    settingsMap.insert("convFilter", QVariant(conv.join(",")));

    //Star Filter Settings
    settingsMap.insert("maxSize", QVariant(params.maxSize));
    settingsMap.insert("maxEllipse", QVariant(params.maxEllipse));
    settingsMap.insert("removeBrightest", QVariant(params.removeBrightest));
    settingsMap.insert("removeDimmest", QVariant(params.removeDimmest ));
    settingsMap.insert("saturationLimit", QVariant(params.saturationLimit));

    //Settings that usually get set by the Astrometry config file
    settingsMap.insert("maxwidth", QVariant(params.maxwidth)) ;
    settingsMap.insert("minwidth", QVariant(params.minwidth)) ;
    settingsMap.insert("inParallel", QVariant(params.inParallel)) ;
    settingsMap.insert("solverTimeLimit", QVariant(params.solverTimeLimit));

    //Astrometry Basic Parameters
    settingsMap.insert("resort", QVariant(params.resort)) ;
    settingsMap.insert("downsample", QVariant(params.downsample)) ;
    settingsMap.insert("search_radius", QVariant(params.search_radius)) ;

    //Setting the settings to know when to stop or keep searching for solutions
    settingsMap.insert("logratio_tokeep", QVariant(params.logratio_tokeep)) ;
    settingsMap.insert("logratio_totune", QVariant(params.logratio_totune)) ;
    settingsMap.insert("logratio_tosolve", QVariant(params.logratio_tosolve)) ;

    //Setting the logging settings
    settingsMap.insert("logToFile", QVariant(params.logToFile)) ;
    settingsMap.insert("logLevel", QVariant(params.logLevel)) ;

    return settingsMap;

}

SexySolver::Parameters SexySolver::convertFromMap(QMap<QString, QVariant> settingsMap)
{
    SexySolver::Parameters params;
    params.listName = settingsMap.value("listName", params.listName).toString();

    //These are to pass the parameters to the internal sextractor

    params.calculateHFR = settingsMap.value("calculateHFR", params.listName).toDouble();
    params.apertureShape = (Shape)settingsMap.value("apertureShape", params.listName).toInt();
    params.kron_fact = settingsMap.value("kron_fact", params.listName).toDouble();
    params.subpix = settingsMap.value("subpix", params.listName).toInt();
    params.r_min= settingsMap.value("r_min", params.listName).toDouble();
    //params.inflags
    params.magzero = settingsMap.value("magzero", params.magzero).toDouble();
    params.minarea = settingsMap.value("minarea", params.minarea).toDouble();
    params.deblend_thresh = settingsMap.value("deblend_thresh", params.deblend_thresh).toInt();
    params.deblend_contrast = settingsMap.value("deblend_contrast", params.deblend_contrast).toDouble();
    params.clean = settingsMap.value("clean", params.clean).toInt();
    params.clean_param = settingsMap.value("clean_param", params.clean_param).toDouble();

    //The Conv Filter
    params.fwhm = settingsMap.value("fwhm",params.fwhm).toDouble();
    if(settingsMap.contains("convFilter"))
    {
        QStringList conv = settingsMap.value("convFilter", "").toString().split(",");
        QVector<float> filter;
        foreach(QString item, conv)
            filter.append(QVariant(item).toFloat());
        params.convFilter = filter;
    }

    //Star Filter Settings
    params.maxSize = settingsMap.value("maxSize", params.maxSize).toDouble();
    params.maxEllipse = settingsMap.value("maxEllipse", params.maxEllipse).toDouble();
    params.removeBrightest = settingsMap.value("removeBrightest", params.removeBrightest).toDouble();
    params.removeDimmest = settingsMap.value("removeDimmest", params.removeDimmest ).toDouble();
    params.saturationLimit = settingsMap.value("saturationLimit", params.saturationLimit).toDouble();

    //Settings that usually get set by the Astrometry config file
    params.maxwidth = settingsMap.value("maxwidth", params.maxwidth).toDouble() ;
    params.minwidth = settingsMap.value("minwidth", params.minwidth).toDouble() ;
    params.inParallel = settingsMap.value("inParallel", params.inParallel).toBool() ;
    params.solverTimeLimit = settingsMap.value("solverTimeLimit", params.solverTimeLimit).toInt();

    //Astrometry Basic Parameters
    params.resort = settingsMap.value("resort", params.resort).toBool();
    params.downsample = settingsMap.value("downsample", params.downsample).toBool() ;
    params.search_radius = settingsMap.value("search_radius", params.search_radius).toDouble() ;

    //Setting the settings to know when to stop or keep searching for solutions
    params.logratio_tokeep = settingsMap.value("logratio_tokeep", params.logratio_tokeep).toDouble() ;
    params.logratio_totune = settingsMap.value("logratio_totune", params.logratio_totune).toDouble() ;
    params.logratio_tosolve = settingsMap.value("logratio_tosolve", params.logratio_tosolve).toDouble();

    //Setting the logging settings
    params.logToFile = settingsMap.value("logToFile", params.logToFile).toBool() ;
    params.logLevel = settingsMap.value("logLevel", params.logLevel).toInt() ;

    return params;

}

//This function should get the system RAM in bytes.  I may revise it later to get the currently available RAM
//But from what I read, getting the Available RAM is inconsistent and buggy on many systems.
uint64_t SexySolver::getAvailableRAM()
{
    uint64_t RAM = 0;

#if defined(Q_OS_OSX)
    int mib[2];
    size_t length;
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(int64_t);
    if(sysctl(mib, 2, &RAM, &length, NULL, 0))
        return 0; // On Error
#elif defined(Q_OS_LINUX)
    QProcess p;
    p.start("awk", QStringList() << "/MemTotal/ { print $2 }" << "/proc/meminfo");
    p.waitForFinished();
    QString memory = p.readAllStandardOutput();
    RAM = memory.toLong() * 1024; //It is in kB on this system
    p.close();
#else
    MEMORYSTATUSEX memory_status;
    ZeroMemory(&memory_status, sizeof(MEMORYSTATUSEX));
    memory_status.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memory_status)) {
      RAM = memory_status.ullTotalPhys;
    } else {
      RAM = 0;
    }
#endif
    return RAM;
}

//This should determine if enough RAM is available to load all the index files in parallel
bool SexySolver::enoughRAMisAvailableFor(QStringList indexFolders)
{
    uint64_t totalSize = 0;

    foreach(QString folder, indexFolders)
    {
        QDir dir(folder);
        if(dir.exists())
        {
            dir.setNameFilters(QStringList()<<"*.fits"<<"*.fit");
            QFileInfoList indexInfoList = dir.entryInfoList();
            foreach(QFileInfo indexInfo, indexInfoList)
                totalSize += indexInfo.size();
        }

    }
    uint64_t availableRAM = getAvailableRAM();
    if(availableRAM == 0)
    {
        emit logNeedsUpdating("Unable to determine system RAM for inParallel Option");
        return false;
    }
    float bytesInGB = 1024 * 1024 * 1024; // B -> KB -> MB -> GB , float to make sure it reports the answer with any decimals
    emit logNeedsUpdating(QString("Evaluating Installed RAM for inParallel Option.  Total Size of Index files: %1 GB, Installed RAM: %2 GB").arg(totalSize / bytesInGB).arg(availableRAM / bytesInGB));
    return availableRAM > totalSize;
}


