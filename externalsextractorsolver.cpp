#include "externalsextractorsolver.h"
#include <QTextStream>
#include <QMessageBox>
#include <qmath.h>

ExternalSextractorSolver::ExternalSextractorSolver(Statistic imagestats, uint8_t *imageBuffer, QObject *parent) : SexySolver(imagestats, imageBuffer, parent)
{
    stats = imagestats;

#if defined(Q_OS_OSX)
    sextractorBinaryPath = "/usr/local/bin/sex";
#elif defined(Q_OS_LINUX)
    sextractorBinaryPath = "/usr/bin/sextractor";
#else //Windows
    sextractorBinaryPath = "C:/cygwin64/bin/sextractor";
#endif

#if defined(Q_OS_OSX)
    confPath = "/Applications/KStars.app/Contents/MacOS/astrometry/bin/astrometry.cfg";
#elif defined(Q_OS_LINUX)
    confPath = "$HOME/.local/share/kstars/astrometry/astrometry.cfg";
#else //Windows
    //confPath = "C:/cygwin64/usr/etc/astrometry.cfg";
    confPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/etc/astrometry/backend.cfg";
#endif

#if defined(Q_OS_OSX)
    solverPath = "/usr/local/bin/solve-field";
#elif defined(Q_OS_LINUX)
    solverPath = "/usr/bin/solve-field";
#else //Windows
    //solverPath = "C:/cygwin64/bin/solve-field";
    solverPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/lib/astrometry/bin/solve-field.exe";
#endif

#if defined(Q_OS_OSX)
    wcsPath = "/usr/local/bin/wcsinfo";
#elif defined(Q_OS_LINUX)
    wcsPath = "/usr/bin/wcsinfo";
#else //Windows
    //wcsPath = "C:/cygwin64/bin/wcsinfo";
    wcsPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/lib/astrometry/bin/wcsinfo.exe";
#endif

}

//This is the method that runs the solver or sextractor.  Do not call it, use start() instead, so that it can start a new thread.
void ExternalSextractorSolver::runExternalSextractorAndSolver()
{
    if(sextract(false))
    {
        solveField();
    }
}


//This is the abort method.  The way that it works is that it creates a file.  Astrometry.net is monitoring for this file's creation in order to abort.
void ExternalSextractorSolver::abort()
{
    if(!solver.isNull())
        solver->kill();
    if(!sextractorProcess.isNull())
        sextractorProcess->kill();
    emit logNeedsUpdating("Aborted");
}


//This method is copied and pasted and modified from the code I wrote to use sextractor in OfflineAstrometryParser in KStars
bool ExternalSextractorSolver::sextract(bool justSextract)
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

    sextractorFilePath = tempPath + QDir::separator() + "SextractorList.xyls";
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

    QString paramPath =  tempPath + QDir::separator() + "default.param";
    QFile paramFile(paramPath);
    if(!paramFile.exists())
    {
        if (paramFile.open(QIODevice::WriteOnly) == false)
            QMessageBox::critical(nullptr,"Message","Sextractor file write error.");
        else
        {
            //Note, if you change the parameters here, make sure you delete the default.param file from your temp directory
            //Since it will only get created if it doesn't exist.
            //The program will try to do this at launch,but if you choose a different directory, it won't be able to.
            QTextStream out(&paramFile);
            out << "MAG_AUTO                 Kron-like elliptical aperture magnitude                   [mag]\n";
            out << "FLUX_AUTO                Flux within a Kron-like elliptical aperture               [count]\n";
            out << "X_IMAGE                  Object position along x                                   [pixel]\n";
            out << "Y_IMAGE                  Object position along y                                   [pixel]\n";
            out << "CXX_IMAGE                Cxx object ellipse parameter                              [pixel**(-2)]\n";
            out << "CYY_IMAGE                Cyy object ellipse parameter                              [pixel**(-2)]\n";
            out << "CXY_IMAGE                Cxy object ellipse parameter                              [pixel**(-2)]\n";
            paramFile.close();
        }
    }
    sextractorArgs << "-PARAMETERS_NAME" << paramPath;


    //sextractor needs a default.conv file in the working directory
    //This creates the default one

    QString convPath =  tempPath + QDir::separator() + "default.conv";
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
    sextractorProcess = new QProcess();

    sextractorProcess->setWorkingDirectory(tempPath);
    sextractorProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(sextractorProcess, &QProcess::readyReadStandardOutput, this, &ExternalSextractorSolver::logSextractor);

    emit logNeedsUpdating("Starting external sextractor...");
    emit logNeedsUpdating(sextractorBinaryPath + " " + sextractorArgs.join(' '));
    sextractorProcess->start(sextractorBinaryPath, sextractorArgs);
    if(justSextract)
        connect(sextractorProcess, QOverload<int>::of(&QProcess::finished), this, [this](int x){printSextractorOutput(); getSextractorTable();  emit starsFound(); emit sextractorFinished(x);});
    else
    {
        sextractorProcess->waitForFinished();
        printSextractorOutput();
    }

    return true;

}

void ExternalSextractorSolver::printSextractorOutput()
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating(sextractorProcess->readAllStandardError().trimmed());
}

//The code for this method is copied and pasted and modified from OfflineAstrometryParser in KStars
bool ExternalSextractorSolver::solveField()
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    sextractorFilePath = tempPath + QDir::separator() + "SextractorList.xyls";
    QFile sextractorFile(sextractorFilePath);
    if(!sextractorFile.exists())
    {
        emit logNeedsUpdating("Please Sextract the image first");
    }

    QStringList solverArgs=getSolverArgsList();

    solverArgs << "--backend-config" << confPath;

    QString solutionFile = tempPath + QDir::separator() + "SextractorList.wcs";
    solverArgs << "-W" << solutionFile;

    solverArgs << sextractorFilePath;

    solver.clear();
    solver = new QProcess();

    solver->setProcessChannelMode(QProcess::MergedChannels);
    connect(solver, &QProcess::readyReadStandardOutput, this, &ExternalSextractorSolver::logSolver);
    connect(solver, QOverload<int>::of(&QProcess::finished), this, [this](int x){getSolutionInformation();emit finished(x);});

#ifdef _WIN32 //This will set up the environment so that the ANSVR internal solver will work
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString path            = env.value("Path", "");
        QString ansvrPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/";
        QString pathsToInsert =ansvrPath + "bin;";
        pathsToInsert += ansvrPath + "lib/lapack;";
        pathsToInsert += ansvrPath + "lib/astrometry/bin;";
        env.insert("Path", pathsToInsert + path);
        solver->setProcessEnvironment(env);
#endif

    solver->start(solverPath, solverArgs);

    emit logNeedsUpdating("Starting external Astrometry.net solver...");
    emit logNeedsUpdating("Command: " + solverPath + solverArgs.join(" "));


    QString command = solverPath + ' ' + solverArgs.join(' ');

    emit logNeedsUpdating(command);
    return true;
}

//This method is copied and pasted and modified from getSolverOptionsFromFITS in Align in KStars
//Then it was split in two parts
//Tnis part generates the argument list from the options for the external solver only
QStringList ExternalSextractorSolver::getSolverArgsList()
{
    QStringList solverArgs;

    // Start with always-used arguments
    solverArgs << "-O"
                << "--no-plots";

    // Now go over boolean options
    solverArgs << "--no-verify";
#ifndef _WIN32  //For Windows it is already sorted.
    solverArgs << "--resort";
#endif

    // downsample
    solverArgs << "--downsample" << QString::number(2);


    solverArgs << "--width" << QString::number(stats.width);
    solverArgs << "--height" << QString::number(stats.height);
    solverArgs << "--x-column" << "X_IMAGE";
    solverArgs << "--y-column" << "Y_IMAGE";
#ifndef _WIN32 //For Windows it is guaranteed to be already sorted since it uses the internal sextractor.
    solverArgs << "--sort-column" << "MAG_AUTO";
    solverArgs << "--sort-ascending";
#endif

                //Note This set of items is NOT NEEDED for Sextractor, it is needed to avoid python usage
                //This may need to be changed later, but since the goal for using sextractor is to avoid python, this is placed here.
    solverArgs << "--no-remove-lines";
    solverArgs << "--uniformize" << "0";

    if (use_scale)
        solverArgs << "-L" << QString::number(scalelo) << "-H" << QString::number(scalehi) << "-u" << units;

    if (use_position)
        solverArgs << "-3" << QString::number(search_ra) << "-4" << QString::number(search_dec) << "-5" << QString::number(search_radius);

    return solverArgs;
}

//These methods are for the logging of information to the textfield at the bottom of the window.
//They are used by everything

void ExternalSextractorSolver::logSextractor()
{
    QString rawText(sextractorProcess->readAll().trimmed());
    emit logNeedsUpdating(rawText.remove("[1M>").remove("[1A"));
}

void ExternalSextractorSolver::logSolver()
{
     emit logNeedsUpdating(solver->readAll().trimmed());
}

//This method is copied and pasted and modified from tablist.c in astrometry.net
//This is needed to load in the stars sextracted by an extrnal sextractor to get them into the table
bool ExternalSextractorSolver::getSextractorTable()
{
     QFile sextractorFile(sextractorFilePath);
     if(!sextractorFile.exists())
     {
         emit logNeedsUpdating("Can't display sextractor file since it doesn't exist.");
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
        emit logNeedsUpdating(QString::fromUtf8(error_status));
        return false;
    }

    if ( fits_get_hdu_num(new_fptr, &hdunum) == 1 )
        /* This is the primary array;  try to move to the */
        /* first extension and see if it is a table */
        fits_movabs_hdu(new_fptr, 2, &hdutype, &status);
    else
        fits_get_hdu_type(new_fptr, &hdutype, &status); /* Get the HDU type */

    if (!(hdutype == ASCII_TBL || hdutype == BINARY_TBL)) {
        emit logNeedsUpdating("Wrong type of file");
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
               // ui->starList->setItem(jj,0,new QTableWidgetItem(QString::number(jj)));
            float starx = 0;
            float stary = 0;
            float mag = 0;
            float flux = 0;
            float xx = 0;
            float yy = 0;
            float xy = 0;
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
                                flux = QString(value).trimmed().toFloat();
                            if(ii == 3)
                                starx = QString(value).trimmed().toFloat();
                            if(ii == 4)
                                stary = QString(value).trimmed().toFloat();
                            if(ii == 5)
                                xx = QString(value).trimmed().toFloat();
                            if(ii == 6)
                                yy = QString(value).trimmed().toFloat();
                            if(ii == 7)
                                xy = QString(value).trimmed().toFloat();
                        }
                }

            //  xx  xy      or     a   b
            //  xy  yy             b   c
            //Note, I got this translation from these two sources which agree:
            //https://books.google.com/books?id=JNEn23UyHuAC&pg=PA84&lpg=PA84&dq=ellipse+xx+yy+xy&source=bl&ots=ynAWge4jlb&sig=ACfU3U1pqZTkx8Teu9pBTygI9F-WcTncrg&hl=en&sa=X&ved=2ahUKEwj0s-7C3I7oAhXblnIEHacAAf0Q6AEwBHoECAUQAQ#v=onepage&q=ellipse%20xx%20yy%20xy&f=false
            //https://cookierobotics.com/007/
            float thing = sqrt( pow(xx - yy, 2) + 4 * pow(xy, 2) );
            float lambda1 = (xx + yy + thing) / 2;
            float lambda2 = (xx + yy - thing) / 2;
            float a = sqrt(lambda1);
            float b = sqrt(lambda2);
            float theta = qRadiansToDegrees(atan(xy / (lambda1 - yy)));

            Star star = {starx, stary, mag, flux, a, b, theta};

            stars.append(star);
        }

        if (!breakout)
            break;
    }
    fits_close_file(new_fptr, &status);

    if (status) fits_report_error(stderr, status); /* print any error message */
    return true;
}

bool ExternalSextractorSolver::getSolutionInformation()
{
    QString solutionFile = tempPath + QDir::separator() + "SextractorList.wcs";
    QFileInfo solutionInfo(solutionFile);
    if(!solutionInfo.exists())
    {
        emit logNeedsUpdating("Solution file doesn't exist");
        return false;
    }
    QProcess wcsProcess;

#ifdef _WIN32 //This will set up the environment so that the ANSVR internal wcsinfo will work
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString path            = env.value("Path", "");
        QString ansvrPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/";
        QString pathsToInsert = ansvrPath + "bin;";
        pathsToInsert += ansvrPath + "lib/lapack;";
        pathsToInsert += ansvrPath + "lib/astrometry/bin;";
        env.insert("Path", pathsToInsert + path);
        wcsProcess.setProcessEnvironment(env);
#endif

    wcsProcess.start(wcsPath, QStringList(solutionFile));
    wcsProcess.waitForFinished();
    QString wcsinfo_stdout = wcsProcess.readAllStandardOutput();

    //This is a quick way to find out what keys are available
   // logOutput(wcsinfo_stdout);

    QStringList wcskeys = wcsinfo_stdout.split(QRegExp("[\n]"));

    QStringList key_value;

    double ra = 0, dec = 0, orient = 0;
    double fieldw = 0, fieldh = 0;
    QString rastr, decstr;

    for (auto &key : wcskeys)
    {
        key_value = key.split(' ');

        if (key_value.size() > 1)
        {
            if (key_value[0] == "ra_center")
                ra = key_value[1].toDouble();
            else if (key_value[0] == "dec_center")
                dec = key_value[1].toDouble();
            else if (key_value[0] == "orientation_center")
                orient = key_value[1].toDouble();
            else if (key_value[0] == "fieldw")
                fieldw = key_value[1].toDouble();
            else if (key_value[0] == "fieldh")
                fieldh = key_value[1].toDouble();
            else if (key_value[0] == "ra_center_hms")
                rastr = key_value[1];
            else if (key_value[0] == "dec_center_dms")
                decstr = key_value[1];

            //else if (key_value[0] == "pixscale")
                //pixscale = key_value[1].toDouble();
            //else if (key_value[0] == "parity")
               // parity = (key_value[1].toInt() == 0) ? "pos" : "neg";
        }
    }
    solution = {fieldw,fieldh,ra,dec,rastr,decstr,orient};
    return true;
}

