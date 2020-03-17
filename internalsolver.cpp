#include "internalsolver.h"
#include "qmath.h"

InternalSolver::InternalSolver(QString file, QString sextractorFile, Statistic imagestats, uint8_t *imageBuffer, bool sextractOnly, QObject *parent) : QThread(parent)
{
 stats=imagestats;
 m_ImageBuffer=imageBuffer;
 justSextract=sextractOnly;
 fileToSolve=file;

 augment_xylist_init(allaxy);

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

//These are some utility functions that can be used in all the code below

//I had to create this method because i was having some difficulty turning a QString into a char* that would persist long enough to be used in the program.
char* charQStr(QString in) {
    std::string fname = QString(in).toStdString();
    char* cstr;
    cstr = new char [fname.size()+1];
    strcpy( cstr, fname.c_str() );
    return cstr;
}

//I created this method because I wanted to remove some temp files and display the output and the code was getting repetitive.
void InternalSolver::removeTempFile(char * fileName)
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
    axy->tempdir = charQStr(QDir::temp().path());
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

//This method was adapted from a combination of the main method in augment-xylist-main.c and the method augment_xylist in augment-xylist.c in astrometry.net
bool InternalSolver::augmentXYList()
{

    emit logNeedsUpdating("Attempting to make an AXY file from the xyls file");
    int i, I;

    qfits_header* hdr = nullptr;

    allaxy->xylsfn = charQStr(sextractorFilePath);


    QString basedir = basePath + QDir::separator() + "SextractorList";

    allaxy->axyfn    = charQStr(QString("%1.axy").arg(basedir));
    allaxy->matchfn  = charQStr(QString("%1.match").arg(basedir));
    allaxy->rdlsfn   = charQStr(QString("%1.rdls").arg(basedir));
    allaxy->solvedfn = charQStr(QString("%1.solved").arg(basedir));
    allaxy->wcsfn    = charQStr(QString("%1.wcs").arg(basedir));
    allaxy->corrfn   = charQStr(QString("%1.corr").arg(basedir));

    emit logNeedsUpdating("Deleting Temp files");
    removeTempFile(allaxy->axyfn);
    removeTempFile(allaxy->matchfn);
    removeTempFile(allaxy->rdlsfn);
    removeTempFile(allaxy->solvedfn);
    removeTempFile(allaxy->wcsfn);
    removeTempFile(allaxy->corrfn);

    allaxy->resort = resort;
    allaxy->sort_ascending = sort_ascending;
    allaxy->xcol = xcol;
    allaxy->ycol = ycol;
    allaxy->sortcol = sortcol;
    allaxy->W = stats.width;
    allaxy->H = stats.height;

    if(use_scale)
    {
        allaxy->scalehi = scalehi;
        allaxy->scalelo = scalelo;
        allaxy->scaleunit = scaleunit;
    }

    if(use_position)
    {
        allaxy->ra_center = ra_center;
        allaxy->dec_center = dec_center;
        allaxy->search_radius = search_radius;
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
        uint8_t do_tabsort = TRUE;

        if (!allaxy->sortcol)
            allaxy->sortcol = "FLUX";
        if (!allaxy->bgcol)
            allaxy->bgcol = "BACKGROUND";

        if (!sortedxylsfn) {
            sortedxylsfn = create_temp_file("sorted", allaxy->tempdir);
            sl_append_nocopy(tempfiles, sortedxylsfn);
        }
        //We aren't now using this because it requires a library not available on Windows.  Also it uses background which is not a column we use in sextractor right now.
        /**
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
**/
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
        return false;
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
emit logNeedsUpdating("Hello, here it is:");
    I = 0;
    for (i=0; i<sl_size(allaxy->verifywcs); i++) {
        emit logNeedsUpdating("nope it isn't");
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
        emit logNeedsUpdating("yes it is");
        fits_header_add_double(hdr, "ANDPIX0", allaxy->predistort->wcstan.crpix[0], "Pre-distortion ref pix x");
        fits_header_add_double(hdr, "ANDPIX1", allaxy->predistort->wcstan.crpix[1], "Pre-distortion ref pix y");
        add_sip_coeffs(hdr, "AND", allaxy->predistort);
    }

    fout = fopen(allaxy->axyfn, "wb");
    if (!fout) {
        emit logNeedsUpdating(QString("Failed to open output file %1").arg(allaxy->axyfn));
        return false;
    }

    emit logNeedsUpdating(QString("Writing headers to file %1\n").arg( allaxy->axyfn));

    if (qfits_header_dump(hdr, fout)) {
        emit logNeedsUpdating(QString("Failed to write FITS header"));
        return false;
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
            return false;
        }
        offset = anqfits_header_start(anq, ext);
        nbytes = anqfits_header_size(anq, ext) + anqfits_data_size(anq, ext);

        emit logNeedsUpdating(QString("Copying data block of file %1 to output %2.\n").arg(
                                  allaxy->xylsfn).arg( allaxy->axyfn));
        anqfits_close(anq);

        fin = fopen(allaxy->xylsfn, "rb");
        if (!fin) {
            emit logNeedsUpdating(QString("Failed to open xyls file \"%1\"").arg( allaxy->xylsfn));
            return false;
        }

        if (pipe_file_offset(fin, offset, nbytes, fout)) {
            emit logNeedsUpdating(QString("Failed to copy the data segment of xylist file %1 to %2").arg(
                                      allaxy->xylsfn).arg (allaxy->axyfn));
            return false;
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


bool InternalSolver::prepare_job() {
    blind_t* bp = &(job->bp);
    solver_t* sp = &(bp->solver);

    job->scales = dl_new(8);
    job->depths = il_new(8);
    job->ra_center = ra_center;
    job->dec_center = dec_center;
    job->search_radius = search_radius;
    job->use_radec_center = use_position ? TRUE : FALSE;

    double dnil = -HUGE_VAL;
    char *pstr;
    int n;
    anbool run;

    anbool default_tweak = TRUE;
    int default_tweakorder = 2;
    double default_odds_toprint = 1e6;
    double default_odds_tokeep = 1e9;
    double default_odds_tosolve = 1e9;
    double default_odds_totune = 1e6;
    //double default_image_fraction = 1.0;
    char* fn;
    double val;
    char pretty[FITS_LINESZ+1];

    blind_init(bp);
    // must be in this order because init_parameters handily zeros out sp
    solver_set_default_values(sp);

    // Here we assume that the field's pixel coordinataes go from zero to IMAGEW,H.

    sp->field_maxx = stats.width;
    sp->field_maxy = stats.height;

    sp->verify_uniformize = allaxy->verify_uniformize;
    sp->verify_dedup = allaxy->verify_dedup;

    sp->verify_pix = allaxy->pixelerr;
   // sp->codetol = allaxy->codetol;  //Causes it to not solve????
    //sp->distractor_ratio


    blind_set_solvedout_file  (bp, allaxy->solvedfn);
    blind_set_match_file   (bp, allaxy->matchfn);
    blind_set_rdls_file    (bp, allaxy->rdlsfn);
    blind_set_wcs_file     (bp, allaxy->wcsfn);
    blind_set_corr_file    (bp, allaxy->corrfn);
    blind_set_cancel_file  (bp, allaxy->cancelfn);
    blind_set_xcol(bp, allaxy->xcol);
    blind_set_ycol(bp, allaxy->ycol);

    //bp->timelimit    Is this needed?
    bp->cpulimit = solverTimeLimit;


    //I need to get these right!  Right now this does not solve
/**
    bp->logratio_tosolve = default_odds_tosolve;
    emit logNeedsUpdating(QString("Set odds ratio to solve to %1 (log = %2)\n").arg( exp(bp->logratio_tosolve)).arg( bp->logratio_tosolve));
    sp->logratio_toprint = default_odds_toprint;
    sp->logratio_tokeep = default_odds_tokeep;
    sp->logratio_totune = allaxy->odds_to_tune_up;
    sp->logratio_bail_threshold = allaxy->odds_to_bail;


    val = 0.5;
    if (val > 0.0)
        sp->logratio_stoplooking = log(val);

    bp->best_hit_only = TRUE;

    // gotta keep it to solve it!
    sp->logratio_tokeep = MIN(sp->logratio_tokeep, bp->logratio_tosolve);
    // gotta print it to keep it (so what if that doesn't make sense)!
    sp->logratio_toprint = MIN(sp->logratio_toprint, sp->logratio_tokeep);
 **/

    // job->image_fraction    This was commented out in the original code?
    job->include_default_scales = 0;

    sp->parity = allaxy->parity;

    sp->set_crpix = allaxy->set_crpix;
    if (sp->set_crpix) {
        if (allaxy->set_crpix_center) {
            sp->set_crpix_center = allaxy->set_crpix_center;
        } else {
            sp->crpix[0] = allaxy->crpix[0];
            sp->crpix[1] = allaxy->crpix[1];
        }
    }

    sp->do_tweak = allaxy->tweak;
    if(sp->do_tweak == TRUE)
    {
        sp->do_tweak = allaxy->tweak;
        sp->tweak_aborder = allaxy->tweakorder;
        sp->tweak_abporder = allaxy->tweakorder;
    }
    else
    {
        sp->tweak_aborder = sp->tweak_abporder = 1;
    }

/**
    val = qfits_header_getdouble(hdr, "ANQSFMIN", 0.0);
    if (val > 0.0)
        bp->quad_size_fraction_lo = val;
    val = qfits_header_getdouble(hdr, "ANQSFMAX", 0.0);
    if (val > 0.0)
        bp->quad_size_fraction_hi = val;
**/
    // tag-along columns

    /**
    bp->rdls_tagalong_all = qfits_header_getboolean(hdr, "ANTAGALL", FALSE);
    if (!bp->rdls_tagalong_all) {
        n = 1;
        while (1) {
            char key[64];
            char* val;
            sprintf(key, "ANTAG%i", n);
            val = fits_get_dupstring(hdr, key);
            if (!val)
                break;
            if (!bp->rdls_tagalong)
                bp->rdls_tagalong = sl_new(16);
            sl_append_nocopy(bp->rdls_tagalong, val);
            n++;
        }
    }
    **/

    // sort RDLS column
    bp->sort_rdls = allaxy->sort_rdls;

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
        dl_append(job->scales, appl);
        dl_append(job->scales, appu);
        blind_add_field_range(bp, appl, appu);
    }


    //Depths?
/**
    n = 1;
    while (1) {
        char key[64];
        int dlo, dhi;
        sprintf(key, "ANDPL%i", n);
        dlo = qfits_header_getint(hdr, key, 0);
        sprintf(key, "ANDPU%i", n);
        dhi = qfits_header_getint(hdr, key, 0);
        if (dlo == 0 && dhi == 0)
            break;
        if ((dlo < 1) || (dlo > dhi)) {
            logerr("Depth range %i to %i is invalid: min must be >= 1, max must be >= min.\n", dlo, dhi);
            goto bailout;
        }
        il_append(job->depths, dlo);
        il_append(job->depths, dhi);
        n++;
    }
**/

/**

    n = 1;
    while (1) {
        char key[64];
        sip_t wcs;
        char* keys[] = { "ANW%iPIX1", "ANW%iPIX2", "ANW%iVAL1", "ANW%iVAL2",
                         "ANW%iCD11", "ANW%iCD12", "ANW%iCD21", "ANW%iCD22" };
        double* vals[] = { &(wcs.wcstan. crval[0]), &(wcs.wcstan.crval[1]),
                           &(wcs.wcstan.crpix[0]), &(wcs.wcstan.crpix[1]),
                           &(wcs.wcstan.cd[0][0]), &(wcs.wcstan.cd[0][1]),
                           &(wcs.wcstan.cd[1][0]), &(wcs.wcstan.cd[1][1]) };
        int j;
        int bail = 0;
        memset(&wcs, 0, sizeof(wcs));
        for (j = 0; j < 8; j++) {
            sprintf(key, keys[j], n);
            *(vals[j]) = qfits_header_getdouble(hdr, key, dnil);
            if (*(vals[j]) == dnil) {
                bail = 1;
                break;
            }
        }
        if (bail)
            break;

        // SIP terms
        sprintf(key, "ANW%i", n);
        parse_sip_coeffs(hdr, key, &wcs);

        sip_ensure_inverse_polynomials(&wcs);

        blind_add_verify_wcs(bp, &wcs);
        n++;
    }
    **/
    if(allaxy->predistort)
    {
        do {
            sip_t dsip = *allaxy->predistort;
            double p0, p1;
            memset(&dsip, 0, sizeof(sip_t));
            p0 = allaxy->predistort->wcstan.crpix[0];
            if (p0 == dnil)
                break;
            p1 =allaxy->predistort->wcstan.crpix[1];
            if (p1 == dnil)
                break;
            dsip.wcstan.crpix[0] = p0;
            dsip.wcstan.crpix[1] = p1;
            if ((dsip.a_order > 1 && dsip.b_order > 1) ||
                (dsip.ap_order > 1 && dsip.bp_order > 1)) {
                sp->predistort = (sip_t*)malloc(sizeof(sip_t));
                memcpy(sp->predistort, &dsip, sizeof(sip_t));
            }
        } while (0);
}

    // Default: solve first field.

        blind_add_field(bp, 1);


    return true;

 bailout:
    return false;
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

    augmentXYList();

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

    engine->inparallel = inParallel ? TRUE : FALSE;
    engine->cpulimit = solverTimeLimit;
    engine->minwidth = minwidth;
    engine->maxwidth = maxwidth;

  //  log_to(stderr);
  //  log_init(LOG_ALL);

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
    struct timeval tv1, tv2;

    gettimeofday(&tv1, nullptr);

     prepare_job();

     blind_t* bp = &(job->bp);

     blind_set_field_file(bp, charQStr(sextractorFilePath));
       // blind_set_field_file(bp, charQStr(theAXYFile));

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

    if (basedir) {
            emit logNeedsUpdating(QString("Setting job's output base directory to %1\n").arg( basedir));
            job_set_output_base_dir(job, basedir);
    }

    emit logNeedsUpdating("Starting Internal Solver Engine!");

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

    if (fin)
        fclose(fin);

    emit finished(0);

    return 0;
}
