/*  SexySolver Internal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
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
void SexySolver::createConvFilterFromFWHM(double fwhm)
{
    this->fwhm = fwhm;
    convFilter.clear();
    double a = 1;
    int size = abs(ceil(fwhm * 0.6));
    for(int y = -size; y <= size; y++ )
    {
        for(int x = -size; x <= size; x++ )
        {
            double value = a * exp( ( -4.0 * log(2.0) * pow(sqrt( pow(x,2) + pow(y,2) ),2) ) / pow(fwhm,2));
            convFilter.append(value);
        }
    }
}

//The code in this section is my attempt at running an internal sextractor program based on SEP
//I used KStars and the SEP website as a guide for creating these functions
bool SexySolver::runSEPSextractor()
{
    if(convFilter.size() == 0)
    {
        emit logNeedsUpdating("No convFilter included.");
        return false;
    }
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating("Starting Internal SexySolver Sextractor. . .");

    //Only downsample images before SEP if the Sextraction is being used for plate solving
    if(!justSextract && downsample != 1)
        downsampleImage(downsample);

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
    status = sep_extract(&im, 2 * bkg->globalrms, SEP_THRESH_ABS, minarea, convFilter.data(), sqrt(convFilter.size()), sqrt(convFilter.size()), SEP_FILTER_CONV, deblend_thresh, deblend_contrast, clean, clean_param, &catalog);
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
        float xPos = catalog->x[i] + 1.5;
        float yPos = catalog->y[i] + 1.5;
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
        if(apertureShape != SHAPE_CIRCLE)
        {
            //Finding the kron radius for the sextraction
            sep_kron_radius(&im, xPos, yPos, a, b, theta, r, &kronrad, &flag);
        }

        bool use_circle;

        switch(apertureShape)
        {
            case SHAPE_AUTO:
                use_circle = kronrad * sqrt(a * b) < r_min;
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
            sep_sum_circle(&im, xPos, yPos, r_min, subpix, inflags, &sum, &sumerr, &area, &flag);
        }
        else
        {
            sep_sum_ellipse(&im, xPos, yPos, a, b, theta, kron_fact*kronrad, subpix, inflags, &sum, &sumerr, &area, &flag);
        }

        float mag = magzero - 2.5 * log10(sum);

        float HFR = 0;
        if(calculateHFR)
        {
            //Get HFR
            sep_flux_radius(&im, catalog->x[i], catalog->y[i], maxRadius, subpix, 0, &flux, requested_frac, 2, flux_fractions, &flux_flag);
            HFR = flux_fractions[0];
        }

        Star star = {xPos , yPos , mag, (float)sum, (float)peak, HFR, a, b, qRadiansToDegrees(theta)};

        stars.append(star);

    }

    emit logNeedsUpdating(QString("Stars Found before Filtering: %1").arg(stars.size()));

    if(stars.size() > 1)
    {
        if(resort)
        {
            //Note that a star is dimmer when the mag is greater!
            //We want to sort in decreasing order though!
            std::sort(stars.begin(), stars.end(), [](const Star &s1, const Star &s2)
            {
                return s1.mag < s2.mag;
            });
        }

        if(resort && removeBrightest > 0.0)
        {
            int numToRemove = stars.count() * (removeBrightest/100.0);
            emit logNeedsUpdating(QString("Removing the %1 brightest stars").arg(numToRemove));
            if(numToRemove > 1)
            {
                for(int i = 0; i<numToRemove;i++)
                    stars.removeFirst();
            }
        }

        if(resort && removeDimmest > 0.0)
        {
            int numToRemove = stars.count() * (removeDimmest/100.0);
            emit logNeedsUpdating(QString("Removing the %1 dimmest stars").arg(numToRemove));
            if(numToRemove > 1)
            {
                for(int i = 0; i<numToRemove;i++)
                    stars.removeLast();
            }
        }

        if(maxEllipse > 1)
        {
            emit logNeedsUpdating(QString("Removing the stars with a/b ratios greater than %1").arg(maxEllipse));
            for(int i = 0; i<stars.size();i++)
            {
                Star star = stars.at(i);
                double ratio = star.a/star.b;
                if(ratio>maxEllipse)
                {
                    stars.removeAt(i);
                    i--;
                }
            }
        }

        if(saturationLimit > 0.0)
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
                emit logNeedsUpdating(QString("Removing the saturated stars with peak values greater than %1 Percent of %2").arg(saturationLimit).arg(maxSizeofDataType));
                for(int i = 0; i<stars.size();i++)
                {
                    Star star = stars.at(i);
                    if(star.peak > (saturationLimit/100.0) * maxSizeofDataType)
                    {
                        stars.removeAt(i);
                        i--;
                    }
                }
            }
        }

    }

    emit logNeedsUpdating(QString("Stars Found after Filtering: %1").arg(stars.size()));

    if(justSextract)
        emit finished(0);
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
    int newBufferSize = oldBufferSize / d;
    uint8_t * newBuffer =new uint8_t[newBufferSize];
    auto * sourceBuffer = reinterpret_cast<T *>(m_ImageBuffer);
    auto * destinationBuffer = reinterpret_cast<T *>(newBuffer);

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
            //The location of the starting pixel in the source image to start sampling to calculate the value for the new pixel
            int sampleSpot = x + y * w;

            for(int y2 = 0; y2 < d; y2++)
            {
                //The offset for the current line of the sample to take
                int currentLine = w * 3 * y2;
                //The total offset for the sample line
                int offset = currentLine + sampleSpot;
                for(int x2 = 0; x2 < d; x2++)
                {
                     //This iterates the sample spots to the right,
                    total += rSource[offset + x2];
                    //This only samples frome the G and B spots if it is an RGB image
                    if(numChannels == 3)
                    {
                        total += gSource[offset + x2];
                        total += bSource[offset + x2];
                    }
                }
            }
            //This calculates the average pixel value and puts it in the new downsampled image.
            int pixel = (x/d) + (y/d) * (w/d);
            destinationBuffer[pixel] = total / (d * d) / numChannels;
        }
    }

    m_ImageBuffer = newBuffer;
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
void SexySolver::setSearchScale(double fov_low, double fov_high, QString units)
{
    use_scale = true;
    //L
    scalelo = fov_low;
    //H
    scalehi = fov_high;
    //u
    this->units = units;

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
void SexySolver::setSearchPosition(double ra, double dec, double rad)
{
    use_position = true;
    //3
    search_ra = ra * 15.0;
    //4
    search_dec = dec;
    //5
    search_radius = rad;
}

//This sets the folders that Astrometry.net should search for index files to a QStringList
void SexySolver::setIndexFolderPaths(QStringList paths)
{
    indexFolderPaths = paths;
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

//This clears the folders that Astrometry.net should search for index files
void SexySolver::clearIndexFolderPaths()
{
    indexFolderPaths.clear();
}

//This adds a folder that Astrometry.et should search for index files to the list
void SexySolver::addIndexFolderPath(QString pathToAdd)
{
    indexFolderPaths.append(pathToAdd);
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
        job->search_radius = search_radius;
    }

    //These initialize the blind and solver objects, and they MUST be in this order according to astrometry.net
    blind_init(bp);
    solver_set_default_values(sp);

    //These set the width and the height of the image in the solver
    sp->field_maxx = stats.width;
    sp->field_maxy = stats.height;

    QDir temp(basePath);
    cancelfn       = basePath + "/" + baseName + ".cancel";
    solvedfn       = basePath + "/" + baseName + ".solved";

    blind_set_cancel_file(bp, cancelfn.toLatin1().constData());
    blind_set_solved_file(bp,solvedfn.toLatin1().constData());

    //Logratios for Solving
    bp->logratio_tosolve = logratio_tosolve;
    emit logNeedsUpdating(QString("Set odds ratio to solve to %1 (log = %2)\n").arg( exp(bp->logratio_tosolve)).arg( bp->logratio_tosolve));
    sp->logratio_tokeep = logratio_tokeep;
    sp->logratio_totune = logratio_totune;
    sp->logratio_bail_threshold = log(DEFAULT_BAIL_THRESHOLD);

    bp->best_hit_only = TRUE;

    // gotta keep it to solve it!
    sp->logratio_tokeep = MIN(sp->logratio_tokeep, bp->logratio_tosolve);

    job->include_default_scales = 0;
    sp->parity = search_parity;

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

    //This creates and sets up the engine
    engine_t* engine = engine_new();

    //This sets some basic engine settings
    engine->inparallel = inParallel ? TRUE : FALSE;
    engine->minwidth = minwidth;
    engine->maxwidth = maxwidth;

    //This sets the logging level and log file based on the user's preferences.
    if(logToFile)
    {
        if(logFile == "")
            logFile = basePath + "/" + baseName + ".log.txt";
        if(QFile(logFile).exists())
            QFile(logFile).remove();
        FILE *log = fopen(logFile.toLatin1().constData(),"wb");
        if(log)
        {
            log_init((log_level)logLevel);
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
    bp->timelimit = solverTimeLimit;
#ifndef _WIN32
    bp->cpulimit = solverTimeLimit;
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

    //This deletes the engine object since it is no longer needed.
    engine_free(engine);

    //This deletes the temporary files
    QDir temp(basePath);
    temp.remove(cancelfn);
    temp.remove(solvedfn);

    //Note: I can only get these items after the solve because I made a small change to the
    //Astrometry.net Code.  I made it return in solve_fields in blind.c before it ran "cleanup"
    MatchObj match =bp->solver.best_match;
    sip_t *wcs = match.sip;

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
            pixscale = sip_pixel_scale(wcs) / downsample;
        else
            pixscale = sip_pixel_scale(wcs);

        emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logNeedsUpdating(QString("Solve Log Odds:  %1").arg(bp->solver.best_logodds));
        emit logNeedsUpdating(QString("Number of Matches:  %1").arg(match.nmatch));
        emit logNeedsUpdating(QString("Solved with index:  %1").arg(match.indexid));
        emit logNeedsUpdating(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
        emit logNeedsUpdating(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr).arg( decstr));
        emit logNeedsUpdating(QString("Field size: %1 x %2 %3").arg( fieldw).arg( fieldh).arg( fieldunits));
        emit logNeedsUpdating(QString("Pixel Scale: %1\"").arg( pixscale));
        emit logNeedsUpdating(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));


        emit logNeedsUpdating(QString("Field parity: %1\n").arg( parity));

        solution = {fieldw,fieldh,ra,dec,rastr,decstr,orient, pixscale, parity};
        emit finished(0);
        return 0;
    }
    else
    {
        emit logNeedsUpdating("Solver was aborted, timed out, or failed, so no solution was found");
        emit finished(-1);
        return -1;
    }
}
