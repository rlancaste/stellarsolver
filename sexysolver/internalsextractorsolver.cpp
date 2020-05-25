/*  InternalSextractorSolver, SexySolver Internal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#if defined(__APPLE__)
#include <sys/stat.h>
#elif defined(_WIN32)
#include "windows.h"
#else //Linux
#include <sys/stat.h>
#endif

#include "internalsextractorsolver.h"
#include "qmath.h"

InternalSextractorSolver::InternalSextractorSolver(ProcessType type, Statistic imagestats, uint8_t const *imageBuffer, QObject *parent) : SextractorSolver(type, imagestats, imageBuffer, parent)
{
    processType = type;
    stats=imagestats;
    m_ImageBuffer=imageBuffer;

    //This sets the base name used for the temp files.
    srand(time(NULL));
    baseName = "internalSextractorSolver_" + QString::number(rand());

}

InternalSextractorSolver::~InternalSextractorSolver()
{
    if(usingDownsampledImage)
        delete downSampledBuffer;
}

//This is the abort method.  The way that it works is that it creates a file.  Astrometry.net is monitoring for this file's creation in order to abort.
void InternalSextractorSolver::abort()
{
    QFile file(cancelfn);
    if(QFileInfo(file).dir().exists())
    {
        file.open(QIODevice::WriteOnly);
        file.write("Cancel");
        file.close();
    }
    if(!isChildSolver)
        emit logOutput("Aborting...");
    wasAborted = true;
}

//This method generates child solvers with the options of the current solver
SextractorSolver* InternalSextractorSolver::spawnChildSolver(int n)
{
    InternalSextractorSolver *solver = new InternalSextractorSolver(processType, stats, m_ImageBuffer, nullptr);
    solver->stars = stars;
    solver->basePath = basePath;
    //They will all share the same basename
    solver->hasSextracted = true;
    solver->isChildSolver = true;
    solver->params = params;
    solver->indexFolderPaths = indexFolderPaths;
    //Set the log level one less than the main solver
    if(logLevel == LOG_MSG || logLevel == LOG_NONE)
        solver->logLevel = LOG_NONE;
    if(logLevel == LOG_VERB)
        solver->logLevel = LOG_MSG;
    if(logLevel == LOG_ALL)
        solver->logLevel = LOG_VERB;
    if(use_scale)
        solver->setSearchScale(scalelo, scalehi, scaleunit);
    if(use_position)
        solver->setSearchPositionInDegrees(search_ra, search_dec);
    if(logLevel != LOG_NONE)
        connect(solver, &SextractorSolver::logOutput, this,  &SextractorSolver::logOutput);
    //This way they all share a solved and cancel fn
    solver->cancelfn = cancelfn;
    solver->solvedfn = solvedfn;
    solver->usingDownsampledImage = usingDownsampledImage;
    return solver;
}

int InternalSextractorSolver::sextract()
{
    if(processType == INT_SEP_HFR)
        calculateHFR = true;
    if(processType == INT_SEP)
        calculateHFR = false;
    return(runSEPSextractor());
}

//This is the method that runs the solver or sextractor.  Do not call it, use the methods above instead, so that it can start a new thread.
void InternalSextractorSolver::run()
{
    if(logLevel != LOG_NONE && logToFile)
    {
        if(logFileName == "")
            logFileName = basePath + "/" + baseName + ".log.txt";
        if(QFile(logFileName).exists())
            QFile(logFileName).remove();
    }

    if(cancelfn == "")
        cancelfn = basePath + "/" + baseName + ".cancel";
    if(solvedfn == "")
        solvedfn = basePath + "/" + baseName + ".solved";

    switch(processType)
    {
        case INT_SEP:
        case INT_SEP_HFR:
            emit finished(sextract());
        break;

        case SEXYSOLVER:
        {
            if(!hasSextracted)
                sextract();
            if(hasSextracted)
                emit finished(runInternalSolver());
            else
                emit finished(-1);
        }
        break;

        default: break;
    }
    if(!isChildSolver)
    {
        QFile(solvedfn).remove();
        QFile(cancelfn).remove();
    }
}


//The code in this section is my attempt at running an internal sextractor program based on SEP
//I used KStars and the SEP website as a guide for creating these functions
int InternalSextractorSolver::runSEPSextractor()
{
    if(params.convFilter.size() == 0)
    {
        emit logOutput("No convFilter included.");
        return -1;
    }
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting Internal SexySolver Sextractor. . .");

    //Only downsample images before SEP if the Sextraction is being used for plate solving
    if(processType == SEXYSOLVER && params.downsample != 1)
        downsampleImage(params.downsample);

    int x = 0, y = 0, w = stats.width, h = stats.height, maxRadius = 50;
    if(useSubframe)
    {
         x = subframe.x();
         w = subframe.width();
         y = subframe.y();
         h = subframe.height();
    }

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
            memcpy(data, m_ImageBuffer, sizeof(float)*w*h);
            break;
        case TDOUBLE:
            getFloatBuffer<double>(data, x, y, w, h);
            break;
        default:
            delete [] data;
            return -1;
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

    //Saving some background information
    background.bh = bkg->bh;
    background.bw = bkg->bw;
    background.global = bkg->global;
    background.globalrms = bkg->globalrms;

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
        if(processType == INT_SEP_HFR)
        {
            //Get HFR
            sep_flux_radius(&im, catalog->x[i], catalog->y[i], maxRadius, params.subpix, 0, &flux, requested_frac, 2, flux_fractions, &flux_flag);
            HFR = flux_fractions[0];
        }

        Star star = {xPos + x, yPos + y, mag, (float)sum, (float)peak, HFR, a, b, qRadiansToDegrees(theta), 0, 0,"",""};

        stars.append(star);
    }

    applyStarFilters();

    hasSextracted = true;

    if (stats.dataType != TFLOAT)
        delete [] data;
    sep_bkg_free(bkg);
    sep_catalog_free(catalog);
    free(imback);
    free(fluxerr);
    free(area);
    free(flag);
    return 0;

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
        emit logOutput(errorMessage);
        return status;
    }

    return -1;
}

void InternalSextractorSolver::applyStarFilters()
{
    if(stars.size() > 1)
    {
        emit logOutput(QString("Stars Found before Filtering: %1").arg(stars.size()));
        if(params.resort)
        {
            //Note that a star is dimmer when the mag is greater!
            //We want to sort in decreasing order though!
            std::sort(stars.begin(), stars.end(), [](const Star &s1, const Star &s2)
            {
                return s1.mag < s2.mag;
            });
        }

        if(params.maxSize > 0.0)
        {
            emit logOutput(QString("Removing stars wider than %1 pixels").arg(params.maxSize));
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

        if(params.minSize > 0.0)
        {
            emit logOutput(QString("Removing stars smaller than %1 pixels").arg(params.minSize));
            for(int i = 0; i<stars.size();i++)
            {
                Star star = stars.at(i);
                if(star.a < params.minSize || star.b < params.minSize)
                {
                    stars.removeAt(i);
                    i--;
                }
            }
        }

        if(params.resort && params.removeBrightest > 0.0 && params.removeBrightest < 100.0)
        {
            int numToRemove = stars.count() * (params.removeBrightest/100.0);
            emit logOutput(QString("Removing the %1 brightest stars").arg(numToRemove));
            if(numToRemove > 1)
            {
                for(int i = 0; i<numToRemove;i++)
                    stars.removeFirst();
            }
        }

        if(params.resort && params.removeDimmest > 0.0 && params.removeDimmest < 100.0)
        {
            int numToRemove = stars.count() * (params.removeDimmest/100.0);
            emit logOutput(QString("Removing the %1 dimmest stars").arg(numToRemove));
            if(numToRemove > 1)
            {
                for(int i = 0; i<numToRemove;i++)
                    stars.removeLast();
            }
        }

        if(params.maxEllipse > 1)
        {
            emit logOutput(QString("Removing the stars with a/b ratios greater than %1").arg(params.maxEllipse));
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
                emit logOutput("Skipping Saturation filter");
            }
            else
            {
                emit logOutput(QString("Removing the saturated stars with peak values greater than %1 Percent of %2").arg(params.saturationLimit).arg(maxSizeofDataType));
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

        if(params.resort && params.keepNum > 0.0)
        {
            emit logOutput(QString("Keeping just the %1 brightest stars").arg(params.keepNum));
            int numToRemove = stars.size() - params.keepNum;
            if(numToRemove > 1)
            {
                for(int i = 0; i<numToRemove;i++)
                    stars.removeLast();
            }
        }
        emit logOutput(QString("Stars Found after Filtering: %1").arg(stars.size()));
    }
}

template <typename T>
void InternalSextractorSolver::getFloatBuffer(float * buffer, int x, int y, int w, int h)
{
    auto * rawBuffer = reinterpret_cast<T const *>(m_ImageBuffer);
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

void InternalSextractorSolver::downsampleImage(int d)
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
void InternalSextractorSolver::downSampleImageType(int d)
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
    auto * sourceBuffer = reinterpret_cast<T const *>(m_ImageBuffer);
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

//This method prepares the job file.  It is based upon the methods parse_job_from_qfits_header and engine_read_job_file in engine.c of astrometry.net
//as well as the part of the method augment_xylist in augment_xylist.c where it handles xyls files
bool InternalSextractorSolver::prepare_job() {
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

    blind_set_cancel_file(bp, cancelfn.toLatin1().constData());
    blind_set_solved_file(bp,solvedfn.toLatin1().constData());

    //Logratios for Solving
    bp->logratio_tosolve = params.logratio_tosolve;
    emit logOutput(QString("Set odds ratio to solve to %1 (log = %2)\n").arg( exp(bp->logratio_tosolve)).arg( bp->logratio_tosolve));
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
        case DEG_WIDTH:
            emit logOutput(QString("Scale range: %1 to %2 degrees wide\n").arg(scalelo).arg(scalehi));
            appl = deg2arcsec(scalelo) / (double)stats.width;
            appu = deg2arcsec(scalehi) / (double)stats.width;
            emit logOutput(QString("Image width %1 pixels; arcsec per pixel range %2 %3\n").arg( stats.width).arg (appl).arg( appu));
            break;
        case ARCMIN_WIDTH:
            emit logOutput(QString("Scale range: %1 to %2 arcmin wide\n").arg (scalelo).arg(scalehi));
            appl = arcmin2arcsec(scalelo) / (double)stats.width;
            appu = arcmin2arcsec(scalehi) / (double)stats.width;
            emit logOutput(QString("Image width %1 pixels; arcsec per pixel range %2 %3\n").arg (stats.width).arg( appl).arg (appu));
            break;
        case ARCSEC_PER_PIX:
            emit logOutput(QString("Scale range: %1 to %2 arcsec/pixel\n").arg (scalelo).arg (scalehi));
            appl = scalelo;
            appu = scalehi;
            break;
        case FOCAL_MM:
            emit logOutput(QString("Scale range: %1 to %2 mm focal length\n").arg (scalelo).arg (scalehi));
            // "35 mm" film is 36 mm wide.
            appu = rad2arcsec(atan(36. / (2. * scalelo))) / (double)stats.width;
            appl = rad2arcsec(atan(36. / (2. * scalehi))) / (double)stats.width;
            emit logOutput(QString("Image width %1 pixels; arcsec per pixel range %2 %3\n").arg (stats.width).arg (appl).arg (appu));
            break;
        default:
            emit logOutput(QString("Unknown scale unit code %1\n").arg (scaleunit));
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
int InternalSextractorSolver::runInternalSolver()
{
   emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Configuring SexySolver");

    //This creates and sets up the engine
    engine_t* engine = engine_new();

    //This sets some basic engine settings
    engine->inparallel = params.inParallel ? TRUE : FALSE;
    engine->minwidth = params.minwidth;
    engine->maxwidth = params.maxwidth;

    if(isChildSolver)
    {
        if(logLevel == LOG_VERB || logLevel == LOG_ALL)
            log_init(logLevel);
    }
    else
        log_init(logLevel);

    if(logLevel != LOG_NONE)
    {
        if(logToFile)
        {
            logFile = fopen(logFileName.toLatin1().constData(),"w");
        }
        else
        {
        #ifndef _WIN32 //Windows does not support FIFO files
            logFileName = basePath + "/" + baseName + ".logFIFO.txt";
            int mkFifoSuccess = 0; //Note if the return value of the command is 0 it succeeded, -1 means it failed.
            if ((mkFifoSuccess = mkfifo(logFileName.toLatin1(), S_IRUSR | S_IWUSR) == 0))
            {
                startLogMonitor();
                logFile = fopen(logFileName.toLatin1().constData(), "r+");
            }
            else
            {
                emit logOutput("Error making FIFO file");
                logLevel = LOG_NONE; //No need to completely fail the sovle just because of a fifo error
            }
        #endif
        }
        if(logFile)
            log_to(logFile);
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
        emit logOutput(QString("\n\n"
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

    int i = 0;
    foreach(Star star, stars)
    {
        xArray[i] = star.x;
        yArray[i] = star.y;
        i++;
    }

    starxy_t* fieldToSolve = (starxy_t*)calloc(1, sizeof(starxy_t));
    fieldToSolve->x = xArray;
    fieldToSolve->y = yArray;
    fieldToSolve->N = stars.size();
    fieldToSolve->flux = nullptr;
    fieldToSolve->background = nullptr;
    bp->solver.fieldxy = fieldToSolve;

    if(depthlo != -1 && depthhi != -1)
    {
        il_append(job->depths, depthlo);
        il_append(job->depths, depthhi);
    }
    else
    {
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
    }

    //This makes sure the min and max widths for the engine make sense, aborting if not.
    if (engine->minwidth <= 0.0 || engine->maxwidth <= 0.0 || engine->minwidth > engine->maxwidth) {
        emit logOutput(QString("\"minwidth\" and \"maxwidth\" must be positive and the maxwidth must be greater!\n"));
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
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting Internal SexySolver Astrometry.net based Engine. . .");

    //This runs the job in the engine in the file engine.c
    if (engine_run_job(engine, job))
        emit logOutput("Failed to run job");

    //Needs to be done whether FIFO or regular file
    if(logLevel != LOG_NONE && logFile)
        fclose(logFile);

    //These things need to be done if it is running off the FIFO file
    #ifndef _WIN32
        if(!logToFile && logLevel != LOG_NONE)
        {
            if(logMonitor)
                logMonitor->quit();

            //Wait up to 10 seconds for the log to finish writing
            int maxLogTime = 10000;
            int time = 0;
            int inc = 10;
            while(logMonitorRunning && time < maxLogTime)
            {
                msleep(inc);
                time+=inc;
            }
        }
    #endif

    //This deletes or frees the items that are no longer needed.
    engine_free(engine);
    bl_free(job->scales);
    dl_free(job->depths);
    free(fieldToSolve);
    delete[] xArray;
    delete[] yArray;

    //Note: I can only get these items after the solve because I made a couple of small changes to the Astrometry.net Code.
    //I made it return in solve_fields in blind.c before it ran "cleanup".  I also had it wait to clean up solutions, blind and solver in engine.c.  We will do that after we get the solution information.

    match = bp->solver.best_match;
    int returnCode = 0;
    if(match.sip)
    {
        wcs = *match.sip;
        hasWCS = true;
        double ra, dec, fieldw, fieldh, pixscale;
        char rastr[32], decstr[32];
        QString parity;
        char* fieldunits;

        // print info about the field.

        double orient;
        sip_get_radec_center(&wcs, &ra, &dec);
        sip_get_radec_center_hms_string(&wcs, rastr, decstr);
        sip_get_field_size(&wcs, &fieldw, &fieldh, &fieldunits);
        orient = sip_get_orientation(&wcs);

        // Note, negative determinant = positive parity.
        double det = sip_det_cd(&wcs);
        parity = (det < 0 ? "pos" : "neg");
        if(usingDownsampledImage)
            pixscale = sip_pixel_scale(&wcs) / params.downsample;
        else
            pixscale = sip_pixel_scale(&wcs);

        double raErr = 0;
        double decErr = 0;
        if(use_position)
        {
            raErr = (search_ra - ra) * 3600;
            decErr = (search_dec - dec) * 3600;
        }

        emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logOutput(QString("Solve Log Odds:  %1").arg(bp->solver.best_logodds));
        emit logOutput(QString("Number of Matches:  %1").arg(match.nmatch));
        emit logOutput(QString("Solved with index:  %1").arg(match.indexid));
        emit logOutput(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
        emit logOutput(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr).arg( decstr));
        if(use_position)
            emit logOutput(QString("Field is: (%1, %2) deg from search coords.").arg( raErr).arg( decErr));
        emit logOutput(QString("Field size: %1 x %2 %3").arg( fieldw).arg( fieldh).arg( fieldunits));
        emit logOutput(QString("Pixel Scale: %1\"").arg( pixscale));
        emit logOutput(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
        emit logOutput(QString("Field parity: %1\n").arg( parity));

        solution = {fieldw,fieldh,ra,dec,rastr,decstr,orient, pixscale, parity, raErr, decErr};
        hasSolved = true;
        returnCode = 0;
    }
    else
    {
        if(!isChildSolver)
            emit logOutput("Solver was aborted, timed out, or failed, so no solution was found");
        returnCode = -1;
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

    return returnCode;
}


wcs_point * InternalSextractorSolver::getWCSCoord()
{
    if(!hasWCS)
    {
        emit logOutput("There is no WCS Data.");
        return nullptr;
    }
    //We have to upscale back to the full size image
    int d = params.downsample;
    int w = stats.width * d;
    int h = stats.height * d;


    wcs_point *wcs_coord = new wcs_point[w * h];
    wcs_point * p = wcs_coord;

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            double ra;
            double dec;
            sip_pixelxy2radec(&wcs, x/d, y/d, &ra, &dec);
            p->ra = ra;
            p->dec = dec;
            p++;
        }
    }
    return wcs_coord;
}

QList<Star> InternalSextractorSolver::appendStarsRAandDEC(QList<Star> stars)
{
    if(!hasWCS)
    {
        emit logOutput("There is no WCS Data");
        return stars;
    }

    QList<Star> refinedStars;
    int d = params.downsample;
    foreach(Star star, stars)
    {
        double ra = HUGE_VAL;
        double dec = HUGE_VAL;
        sip_pixelxy2radec(&wcs, star.x / d, star.y / d, &ra, &dec);
        char rastr[32], decstr[32];
        ra2hmsstring(ra, rastr);
        dec2dmsstring(dec, decstr);

        //We do need to correct for all the downsampling as well as add RA/DEC info
        Star refinedStar = {star.x, star.y, star.mag, star.flux, star.peak, star.HFR, star.a, star.b, star.theta, (float)ra, (float)dec, rastr, decstr};
        refinedStars.append(refinedStar);
    }
    return refinedStars;
}

//This starts a monitor for the FIFO file if on a Mac or Linux Machine
//That way the output can be logged.
void InternalSextractorSolver::startLogMonitor()
{
    logMonitor = new QThread();
    connect(logMonitor, &QThread::started, [=]()
    {
        QString message;
        FILE *FIFOReader = fopen(logFileName.toLatin1().constData(), "r");
        if(FIFOReader)
        {
            char c;
            while((c=getc(FIFOReader)) != EOF)
            {
                if( c != '\n')
                    message += c;
                else
                {
                    emit logOutput(message);
                    message = "";
                    //If we don't do this, it could freeze your program it is so much output
                    if(logLevel == LOG_ALL)
                        msleep(1);
                }
            }
            fclose(FIFOReader);
            emit logOutput(message);
            logMonitorRunning = false;
        }
    });
    logMonitor->start();
    logMonitorRunning = true;
}
