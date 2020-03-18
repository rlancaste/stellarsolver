#include "internalsolver.h"
#include "qmath.h"

InternalSolver::InternalSolver(QString file, QString sextractorFile, Statistic imagestats, uint8_t *imageBuffer, bool sextractOnly, QObject *parent) : QThread(parent)
{
 stats=imagestats;
 m_ImageBuffer=imageBuffer;
 justSextract=sextractOnly;
 fileToSolve=file;

 sextractorFilePath = sextractorFile;
}

void InternalSolver::run()
{
    if(justSextract)
        runInnerSextractor();
    else
    {
        if(runInnerSextractor())
        {
            if(writeSextractorTable())
                runAstrometryEngine();
        }
    }
}

void InternalSolver::abort()
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

//These are some utility functions that can be used in all the code below

//I had to create this method because i was having some difficulty turning a QString into a char* that would persist long enough to be used in the program.
char* charQStr(QString in) {
    std::string fname = QString(in).toStdString();
    char* cstr;
    cstr = new char [fname.size()+1];
    strcpy( cstr, fname.c_str() );
    return cstr;
}

//The code in this section is my attempt at running an internal sextractor program based on SEP
//I used KStars and the SEP website as a guide for creating these functions
//It saves the output to SextractorList.xyls in the temp directory.

bool InternalSolver::runInnerSextractor()
{
    if(convFilter.size() == 0)
    {
        emit logNeedsUpdating("No convFilter included.");
        return false;
    }
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating("Starting Internal Sextractor");

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

    if(stars.size() > 1)
    {
        //Note that a star is dimmer when the mag is greater!
        //We want to sort in decreasing order though!
        std::sort(stars.begin(), stars.end(), [](const Star &s1, const Star &s2)
        {
            return s1.mag < s2.mag;
        });
    }

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
void InternalSolver::getFloatBuffer(float * buffer, int x, int y, int w, int h)
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
bool InternalSolver::writeSextractorTable()
{
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
        emit logNeedsUpdating(QString("Could not create binary table."));
        return false;
    }

    int firstrow  = 1;  /* first row in table to write   */
    int firstelem = 1;
    int column = 1;

    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, magArray, &status))
    {
        emit logNeedsUpdating(QString("Could not write mag in binary table."));
        return false;
    }
    column = 2;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, xArray, &status))
    {
        emit logNeedsUpdating(QString("Could not write x pixels in binary table."));
        return false;
    }

    column = 3;
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

    return true;
}


//All the code below is my attempt to implement astrometry.net code in such a way that it could run internally in a program instead of requiring
//external method calls.  This would be critical for running astrometry.net on windows and it might make the code more efficient on Linux and mac since it
//would not have to prepare command line options and parse them all the time.

void InternalSolver::setSearchScale(double fov_low, double fov_high, QString units)
{
    use_scale = true;
    //L
    scalelo = fov_low;
    //H
    scalehi = fov_high;
    //u
    if(units == "app")
        scaleunit = SCALE_UNITS_ARCSEC_PER_PIX;
    if(units =="aw")
         scaleunit = SCALE_UNITS_ARCMIN_WIDTH;
}

void InternalSolver::setSearchPosition(double ra, double dec, double rad)
{
    use_position = true;
    //3
    ra_center = ra * 15.0;
    //4
    dec_center = dec;
    //5
    search_radius = rad;
}

void InternalSolver::setIndexFolderPaths(QStringList paths)
{
    indexFolderPaths = paths;
}
void InternalSolver::clearIndexFolderPaths()
{
    indexFolderPaths.clear();
}
void InternalSolver::addIndexFolderPath(QString pathToAdd)
{
    indexFolderPaths.append(pathToAdd);
}

//This method prepares the job file.  It is based upon the methods parse_job_from_qfits_header and engine_read_job_file in engine.c of astrometry.net
//as well as the part of the method augment_xylist in augment_xylist.c where it handles xyls files

bool InternalSolver::prepare_job() {
    blind_t* bp = &(job->bp);
    solver_t* sp = &(bp->solver);

    job->scales = dl_new(8);
    job->depths = il_new(8);

    job->use_radec_center = use_position ? TRUE : FALSE;
    if(use_position)
    {
        job->ra_center = ra_center;
        job->dec_center = dec_center;
        job->search_radius = search_radius;
    }

    anbool default_tweak = TRUE;
    int default_tweakorder = 2;
    double default_odds_toprint = 1e6;
    double default_odds_tokeep = 1e9;
    double default_odds_tosolve = 1e9;
    double default_odds_totune = 1e6;
    //double default_image_fraction = 1.0;

    blind_init(bp);
    // must be in this order because init_parameters handily zeros out sp
    solver_set_default_values(sp);

    // Here we assume that the field's pixel coordinataes go from zero to IMAGEW,H.
    sp->field_maxx = stats.width;
    sp->field_maxy = stats.height;

    //sp->verify_uniformize =
    //sp->verify_dedup =

    //sp->verify_pix=
   // sp->codetol =   //Causes it to not solve????
    //sp->distractor_ratio

    QString basedir = basePath + QDir::separator() + "SextractorList";

    //Files that we will need later
    solvedfn       = charQStr(QString("%1.solved").arg(basedir));
    wcsfn          = charQStr(QString("%1.wcs").arg(basedir));
    cancelfn       = charQStr(QString("%1.cancel").arg(basedir));
    QFile(solvedfn).remove();
    QFile(wcsfn).remove();
    QFile(cancelfn).remove();
    blind_set_solvedout_file  (bp, solvedfn);
    blind_set_wcs_file     (bp, wcsfn);
    blind_set_cancel_file(bp, cancelfn);

    blind_set_xcol(bp, xcol);
    blind_set_ycol(bp, ycol);

    //bp->timelimit    Is this needed?
    bp->cpulimit = solverTimeLimit;

    //Logratios for Solving
    bp->logratio_tosolve = log(default_odds_tosolve);
    emit logNeedsUpdating(QString("Set odds ratio to solve to %1 (log = %2)\n").arg( exp(bp->logratio_tosolve)).arg( bp->logratio_tosolve));
    sp->logratio_tokeep = log(default_odds_tokeep);
    sp->logratio_totune = log(default_odds_totune);
    sp->logratio_bail_threshold = log(DEFAULT_BAIL_THRESHOLD);

    bp->best_hit_only = TRUE;

    // gotta keep it to solve it!
    sp->logratio_tokeep = MIN(sp->logratio_tokeep, bp->logratio_tosolve);

    job->include_default_scales = 0;
    sp->parity = PARITY_BOTH;

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
int InternalSolver::runAstrometryEngine()
{
    emit logNeedsUpdating("++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating("Configuring Internal Solver");
    sextractorFilePath = QDir::tempPath() + "/SextractorList.xyls";
    QFile sextractorFile(sextractorFilePath);
    if(!sextractorFile.exists())
    {
        emit logNeedsUpdating("Please Sextract the image first");
    }

   // augmentXYList();

    int i;
    engine_t* engine;
    char* basedir = nullptr;
    sl* strings = sl_new(4);

    sl* inds = sl_new(4);

    engine = engine_new();

    engine->inparallel = inParallel ? TRUE : FALSE;
    engine->cpulimit = solverTimeLimit;
    engine->minwidth = minwidth;
    engine->maxwidth = maxwidth;

    if(logToFile)
    {
        FILE *log = fopen(charQStr(logFile),"wb");
        if(log)
        {
            log_init((log_level)logLevel);
            log_to(log);
        }
    }

    basedir = charQStr(QDir::tempPath());

    gslutils_use_error_system();

    foreach(QString path, indexFolderPaths)
    {
        engine_add_search_path(engine,charQStr(path));
    }

    engine_autoindex_search_paths(engine);


    if (!pl_size(engine->indexes)) {
        emit logNeedsUpdating(QString("\n\n"
               "---------------------------------------------------------------------\n"
               "You must include at least one index file in the index file directories\n\n"
               "See http://astrometry.net/use.html about how to get some index files.\n"
               "---------------------------------------------------------------------\n"
               "\n"));
        return -1;
    }

    if (engine->minwidth <= 0.0 || engine->maxwidth <= 0.0) {
        emit logNeedsUpdating(QString("\"minwidth\" and \"maxwidth\" must be positive!\n"));
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
    struct timeval tv1, tv2;

    gettimeofday(&tv1, nullptr);

     prepare_job();

     blind_t* bp = &(job->bp);

     blind_set_field_file(bp, charQStr(sextractorFilePath));

     if (il_size(job->depths) == 0) {
         if (engine->inparallel) {
             // no limit.
             il_append(job->depths, 0);
             il_append(job->depths, 0);
         } else
             il_append_list(job->depths, engine->default_depths);
     }

     if (!dl_size(job->scales) || job->include_default_scales) {
         double arcsecperpix;
         arcsecperpix = deg2arcsec(engine->minwidth) / stats.width;
         dl_append(job->scales, arcsecperpix);
         arcsecperpix = deg2arcsec(engine->maxwidth) / stats.height;
         dl_append(job->scales, arcsecperpix);
     }
     // The job can only decrease the CPU limit.
     if ((bp->cpulimit == 0.0) || bp->cpulimit > engine->cpulimit) {
         emit logNeedsUpdating(QString("Decreasing CPU time limit to the engine's limit of %1 seconds\n").arg(engine->cpulimit));
         bp->cpulimit = engine->cpulimit;
     }
     // If not running inparallel, set total limits = limits.
     if (!engine->inparallel) {
         bp->total_timelimit = bp->timelimit;
         bp->total_cpulimit  = bp->cpulimit ;
     }

    if (basedir)
            job_set_output_base_dir(job, basedir);

    emit logNeedsUpdating("Starting Internal Solver Engine!");

    if (engine_run_job(engine, job))
        emit logNeedsUpdating("Failed to run_job()\n");

    emit logNeedsUpdating(QString("Logodds: %1").arg(job->bp.solver.best_logodds));

    sip_t wcs;
        double ra, dec, fieldw, fieldh;
        char rastr[32], decstr[32];
        char* fieldunits;


    // print info about the field.
        emit logNeedsUpdating(QString("Solved Field: %1").arg(fileToSolve));
        if (file_exists (wcsfn)) {
            double orient;
            if (!sip_read_header_file (wcsfn, &wcs)) {
                emit logNeedsUpdating(QString("Failed to read WCS header from file %1").arg(wcsfn));
                return false;
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

            solution = {fieldw,fieldh,ra,dec,rastr,decstr,orient};

        } else {
            emit logNeedsUpdating("Did not solve (or no WCS file was written).\n");
        }

   // job_free(job);
    gettimeofday(&tv2, nullptr);
    //emit logNeedsUpdating(QString("Spent %1 seconds on this field.\n").arg(millis_between(&tv1, &tv2)/1000.0));

    engine_free(engine);
    sl_free2(strings);
    sl_free2(inds);

    emit finished(0);

    return 0;
}
