#include "sexysolver.h"
#include "qmath.h"
#include <QRandomGenerator>

SexySolver::SexySolver(Statistic imagestats, uint8_t *imageBuffer, bool sextractOnly, QObject *parent) : QThread(parent)
{
 stats=imagestats;
 m_ImageBuffer=imageBuffer;
 justSextract=sextractOnly;

}

//This is the method that runs the solver or sextractor.  Do not call it, use start() instead, so that it can start a new thread.
void SexySolver::run()
{
    if(justSextract)
        runInnerSextractor();
    else
    {
        if(runInnerSextractor())
        {
#ifndef Q_CC_MSVC //For now due to compilation issues in windows
            runAstrometryEngine();
#else
            emit logNeedsUpdating("Sextraction done, but for now on Windows, the internal solver is disabled due to compilation issues");
#endif
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
//It saves the output to a file in the temp directory.

bool SexySolver::runInnerSextractor()
{
    if(convFilter.size() == 0)
    {
        emit logNeedsUpdating("No convFilter included.");
        return false;
    }
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating("Starting SexySolver Sextractor");

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

        Star star = {xPos , yPos , mag, (float)sum, a, b, qRadiansToDegrees(theta)};

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

    }

    emit logNeedsUpdating(QString("Stars Found after Filtering: %1").arg(stars.size()));

    writeSextractorTable();

    emit starsFound();
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
        emit logNeedsUpdating(errorMessage);
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

//This method writes the table to the file
//I had to create it from the examples on NASA's website
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/quick/node10.html
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/cookbook/node16.html
bool SexySolver::writeSextractorTable()
{

    if(sextractorFilePath == "")
        sextractorFilePath = basePath + QDir::separator() + "sexySolver_" + QString::number(QRandomGenerator::global()->bounded(1, 1000)) + ".xyls";

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
        emit logNeedsUpdating(QString("Could not create binary table."));
        return false;
    }

    int firstrow  = 1;  /* first row in table to write   */
    int firstelem = 1;
    int column = 1;

    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, xArray, &status))
    {
        emit logNeedsUpdating(QString("Could not write x pixels in binary table."));
        return false;
    }

    column = 2;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, yArray, &status))
    {
        emit logNeedsUpdating(QString("Could not write y pixels in binary table."));
        return false;
    }

    if(fits_close_file(new_fptr, &status))
    {
        emit logNeedsUpdating(QString("Error closing file."));
        return false;
    }

    free(xArray);
    free(yArray);

    return true;
}


//All the code below is my attempt to implement astrometry.net code in such a way that it could run internally in a program instead of requiring
//external method calls.  This would be critical for running astrometry.net on windows and it might make the code more efficient on Linux and mac since it
//would not have to prepare command line options and parse them all the time.

void SexySolver::setSearchScale(double fov_low, double fov_high, QString units)
{
    use_scale = true;
    //L
    scalelo = fov_low;
    //H
    scalehi = fov_high;
    //u
    if(units == "app" || units == "arcsecperpix")
        scaleunit = SCALE_UNITS_ARCSEC_PER_PIX;
    if(units =="aw" || units =="amw" || units =="arcminwidth")
        scaleunit = SCALE_UNITS_ARCMIN_WIDTH;
    if(units =="dw" || units =="degw" || units =="degwidth")
        scaleunit = SCALE_UNITS_DEG_WIDTH;
    if(units =="focalmm")
        scaleunit = SCALE_UNITS_FOCAL_MM;
}

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

#ifndef Q_CC_MSVC //Commented out for now on Windows due to compilation issues.
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

    QString basedir = basePath + QDir::separator() + "AstrometrySolver";
    cancelfn       = QString("%1.cancel").arg(basedir);
    QFile(cancelfn).remove();
    blind_set_cancel_file(bp, cancelfn.toLatin1().constData());

    //This sets the x and y columns to read from the xyls file
    blind_set_xcol(bp, xcol);
    blind_set_ycol(bp, ycol);

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
int SexySolver::runAstrometryEngine()
{
    emit logNeedsUpdating("++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating("Configuring SexySolver");

    QFile sextractorFile(sextractorFilePath);
    if(!sextractorFile.exists())
    {
        emit logNeedsUpdating("Please Sextract the image first");
    }

    //This creates and sets up the engine
    engine_t* engine = engine_new();

    //This sets some basic engine settings
    engine->inparallel = inParallel ? TRUE : FALSE;
    engine->minwidth = minwidth;
    engine->maxwidth = maxwidth;

    //This sets the logging level and log file based on the user's preferences.
    if(logToFile)
    {
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

    blind_t* bp = &(job->bp);

    blind_set_field_file(bp, sextractorFilePath.toLatin1().constData());

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

    //This sets the directory for Astrometry.net
    job_set_output_base_dir(job, basePath.toLatin1().constData());

    emit logNeedsUpdating("Starting SexySolver Astrometry.net Engine!");

    //This runs the job in the engine in the file engine.c
    if (engine_run_job(engine, job))
        emit logNeedsUpdating("Failed to run_job()\n");

    //This deletes the engine object since it is no longer needed.
    engine_free(engine);

    //Note: I can only get these items after the solve because I made a small change to the
    //Astrometry.net Code.  I made it return in solve_fields in blind.c before it ran "cleanup"
    MatchObj match =bp->solver.best_match;
    sip_t *wcs = match.sip;

    if(wcs)
    {
        double ra, dec, fieldw, fieldh;
        char rastr[32], decstr[32];
        char* fieldunits;

        // print info about the field.

        double orient;
        sip_get_radec_center(wcs, &ra, &dec);
        sip_get_radec_center_hms_string(wcs, rastr, decstr);
        sip_get_field_size(wcs, &fieldw, &fieldh, &fieldunits);
        orient = sip_get_orientation(wcs);
        emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logNeedsUpdating(QString("Solve Log Odds:  %1").arg(bp->solver.best_logodds));
        emit logNeedsUpdating(QString("Number of Matches:  %1").arg(match.nmatch));
        emit logNeedsUpdating(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
        emit logNeedsUpdating(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr).arg( decstr));
        emit logNeedsUpdating(QString("Field size: %1 x %2 %3").arg( fieldw).arg( fieldh).arg( fieldunits));
        emit logNeedsUpdating(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
        // Note, negative determinant = positive parity.
        double det = sip_det_cd(wcs);
        emit logNeedsUpdating(QString("Field parity: %1\n").arg( (det < 0 ? "pos" : "neg")));

        solution = {fieldw,fieldh,ra,dec,rastr,decstr,orient};
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
#endif
