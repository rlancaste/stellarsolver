/*  ExternalSextractorSolver for SexySolver Tester Application, developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#include "externalsextractorsolver.h"
#include <QTextStream>
#include <QMessageBox>
#include <qmath.h>
#include <wcshdr.h>
#include <wcsfix.h>

ExternalSextractorSolver::ExternalSextractorSolver(Statistic imagestats, uint8_t *imageBuffer, QObject *parent) : SexySolver(imagestats, imageBuffer, parent)
{
    //This sets the base name used for the temp files.
    srand(time(NULL));
    baseName = "externalSextractorSolver_" + QString::number(rand());

    //The code below sets default paths for these key external file settings.

#if defined(Q_OS_OSX)
    if(QFile("/usr/local/bin/solve-field").exists())
        setMacHomebrewPaths();
    else
        setMacInternalPaths();
#elif defined(Q_OS_LINUX)
    setLinuxDefaultPaths();
#else //Windows
    setWinANSVRPaths();
#endif

    //This will automatically delete the temporary files if that option is selected
    connect(this, &SexySolver::finished, this, &ExternalSextractorSolver::cleanupTempFiles);

}

ExternalSextractorSolver::~ExternalSextractorSolver()
{
    delete xcol;
    delete ycol;
    delete magcol;
    delete colFormat;
    delete colUnits;
    delete magUnits;
}

//The following methods are available to set the default paths for different operating systems and configurations.

void ExternalSextractorSolver::setLinuxDefaultPaths()
{
    confPath = "/etc/astrometry.cfg";
    sextractorBinaryPath = "/usr/bin/sextractor";
    solverPath = "/usr/bin/solve-field";
    if(QFile("/bin/astap").exists())
        astapBinaryPath = "/bin/astap";
    else
        astapBinaryPath = "/opt/astap/astap";
    wcsPath = "/usr/bin/wcsinfo";
}

void ExternalSextractorSolver::setLinuxInternalPaths()
{
    confPath = "$HOME/.local/share/kstars/astrometry/astrometry.cfg";
    sextractorBinaryPath = "/usr/bin/sextractor";
    solverPath = "/usr/bin/solve-field";
    if(QFile("/bin/astap").exists())
        astapBinaryPath = "/bin/astap";
    else
        astapBinaryPath = "/opt/astap/astap";
    wcsPath = "/usr/bin/wcsinfo";
}

void ExternalSextractorSolver::setMacHomebrewPaths()
{
    confPath = "/usr/local/etc/astrometry.cfg";
    sextractorBinaryPath = "/usr/local/bin/sex";
    solverPath = "/usr/local/bin/solve-field";
    astapBinaryPath = "/Applications/ASTAP.app/Contents/MacOS/astap";
    wcsPath = "/usr/local/bin/wcsinfo";
}

void ExternalSextractorSolver::setMacInternalPaths()
{
    confPath = "/Applications/KStars.app/Contents/MacOS/astrometry/bin/astrometry.cfg";
    sextractorBinaryPath = "/Applications/KStars.app/Contents/MacOS/astrometry/bin/sex";
    solverPath = "/Applications/KStars.app/Contents/MacOS/astrometry/bin/solve-field";
    astapBinaryPath = "/Applications/ASTAP.app/Contents/MacOS/astap";
    wcsPath = "/Applications/KStars.app/Contents/MacOS/astrometry/bin/wcsinfo";
}

void ExternalSextractorSolver::setWinANSVRPaths()
{
    confPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/etc/astrometry/backend.cfg";
    sextractorBinaryPath = "";
    solverPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/lib/astrometry/bin/solve-field.exe";
    astapBinaryPath = "C:/Program Files/astap/astap.exe";
    wcsPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/lib/astrometry/bin/wcsinfo.exe";
}

void ExternalSextractorSolver::setWinCygwinPaths()
{
    confPath = "C:/cygwin64/usr/etc/astrometry.cfg";
    sextractorBinaryPath = "";
    solverPath = "C:/cygwin64/bin/solve-field";
    astapBinaryPath = "C:/Program Files/astap/astap.exe";
    wcsPath = "C:/cygwin64/bin/wcsinfo";
}

//This is the method you want to run to just sextract the image
void ExternalSextractorSolver::sextract()
{
    justSextract = true;
    runExternalSextractor();
}

//This is the method you want to run to just sextract the image
void ExternalSextractorSolver::sextractWithHFR()
{
    justSextract = true;
    calculateHFR = true;
    runExternalSextractor();
}

//This is the method you want to use to both sextract and solve an image
void ExternalSextractorSolver::sextractAndSolve()
{
    justSextract = false;
    if(runExternalSextractor())
        runExternalSolver();
}

//This is the method you want to use to both sextract and solve an image
void ExternalSextractorSolver::SEPAndSolve()
{
    justSextract = false;
    if(runSEPSextractor())
    {
        writeSextractorTable();
        runExternalSolver();
    }
}

//This is the method you want to use to solve the image externally using traditional astrometry.net
void ExternalSextractorSolver::classicSolve()
{
    this->runExternalClassicSolver();
}

//This is the method yu want to use to solve the image externally with ASTAP
void ExternalSextractorSolver::astapSolve()
{
   runExternalASTAPSolver();
}

//This is the abort method.  For the external sextractor and solver, it uses the kill method to abort the processes
void ExternalSextractorSolver::abort()
{
    if(!solver.isNull())
        solver->kill();
    if(!sextractorProcess.isNull())
        sextractorProcess->kill();

    //Just in case they don't stop, we will make the cancel file too
    QFile file(cancelfn);
    if(QFileInfo(file).dir().exists())
    {
        file.open(QIODevice::WriteOnly);
        file.write("Cancel");
        file.close();
    }
    emit logNeedsUpdating("Aborted");
}

//This determines whether one of the external processes is running or not.
bool ExternalSextractorSolver::isRunning()
{
    if(!sextractorProcess.isNull() && sextractorProcess->state() == QProcess::Running)
        return true;
    if(!solver.isNull() && solver->state() == QProcess::Running)
        return true;
    return false;
}

void ExternalSextractorSolver::cleanupTempFiles()
{
    if(cleanupTemporaryFiles)
    {
        QDir temp(basePath);
        temp.remove(baseName + ".param");
        temp.remove(baseName + ".conv");
        temp.remove(baseName + ".cfg");
        temp.remove(baseName + ".ini");
        temp.remove(baseName + ".corr");
        temp.remove(baseName + ".rdls");
        temp.remove(baseName + ".axy");
        temp.remove(baseName + ".corr");
        temp.remove(baseName + ".wcs");
        temp.remove(baseName + ".solved");
        temp.remove(baseName + ".match");
        temp.remove(baseName + "-indx.xyls");
        if(sextractorFilePathIsTempFile)
            QFile(sextractorFilePath).remove();
        if(fileToProcessIsTempFile)
            QFile(fileToProcess).remove();
    }
}

//This method is copied and pasted and modified from the code I wrote to use sextractor in OfflineAstrometryParser in KStars
//It creates key files needed to run Sextractor from the desired options, then runs the sextractor program using the options.
bool ExternalSextractorSolver::runExternalSextractor()
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    QFileInfo file(fileToProcess);
    if(!file.exists())
        return false;
    if(file.suffix() != "fits" && file.suffix() != "fit")
    {
        if(!saveAsFITS())
            return false;
    }

    if(sextractorFilePath == "")
    {
        sextractorFilePathIsTempFile = true;
        sextractorFilePath = basePath + "/" + baseName + ".xyls";
    }
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
    //This creates that file with the options we need for astrometry.net and sextractor

    QString paramPath =  basePath + "/" + baseName + ".param";
    QFile paramFile(paramPath);
    if (paramFile.open(QIODevice::WriteOnly) == false)
    {
        QMessageBox::critical(nullptr,"Message","Sextractor file write error.");
        return false;
    }
    else
    {
        QTextStream out(&paramFile);
        out << "X_IMAGE\n";//                  Object position along x                                   [pixel]
        out << "Y_IMAGE\n";//                  Object position along y                                   [pixel]
        out << "MAG_AUTO\n";//                 Kron-like elliptical aperture magnitude                   [mag]
        if(justSextract)
        {
            out << "FLUX_AUTO\n";//                Flux within a Kron-like elliptical aperture               [count]
            out << "FLUX_MAX\n";//                 Peak flux above background                                [count]
            out << "CXX_IMAGE\n";//                Cxx object ellipse parameter                              [pixel**(-2)]
            out << "CYY_IMAGE\n";//                Cyy object ellipse parameter                              [pixel**(-2)]
            out << "CXY_IMAGE\n";//                Cxy object ellipse parameter                              [pixel**(-2)]
            if(calculateHFR)
                out << "FLUX_RADIUS\n";//              Fraction-of-light radii                                   [pixel]
        }
        paramFile.close();
    }
    sextractorArgs << "-PARAMETERS_NAME" << paramPath;


    //sextractor needs a default.conv file in the working directory
    //This creates the default one

    QString convPath =  basePath + "/" + baseName + ".conv";
    QFile convFile(convPath);
    if (convFile.open(QIODevice::WriteOnly) == false)
    {
        QMessageBox::critical(nullptr,"Message","Sextractor CONV filter write error.");
        return false;
    }
    else
    {
        QTextStream out(&convFile);
        out << "CONV Filter Generated by SexySolver Internal Library\n";
        int c = 0;
        for(int i = 0; i < params.convFilter.size(); i++)
        {
            out << params.convFilter.at(i);

            //We want the last one before the sqrt of the size to spark a new line
            if(c < sqrt(params.convFilter.size()) - 1)
            {
                out << " ";
                c++;
            }
            else
            {
                out << "\n";
                c = 0;
            }
        }
        convFile.close();
    }

    //Arguments from the default.sex file
    //------------------------------- Extraction ----------------------------------
    sextractorArgs << "-DETECT_TYPE" << "CCD";
    sextractorArgs << "-DETECT_MINAREA" << QString::number(params.minarea);

    //sextractorArgs << "-DETECT_THRESH" << QString::number();
    //sextractorArgs << "-ANALYSIS_THRESH" << QString::number(minarea);

    sextractorArgs << "-FILTER" << "Y";
    sextractorArgs << "-FILTER_NAME" << convPath;

    sextractorArgs << "-DEBLEND_NTHRESH" << QString::number(params.deblend_thresh);
    sextractorArgs << "-DEBLEND_MINCONT" << QString::number(params.deblend_contrast);

    sextractorArgs << "-CLEAN" << ((params.clean == 1)? "Y" :"N");
    sextractorArgs << "-CLEAN_PARAM" << QString::number(params.clean_param);

    //------------------------------ Photometry -----------------------------------
    sextractorArgs << "-PHOT_AUTOPARAMS" << QString::number(params.kron_fact) + "," + QString::number(params.r_min);
    sextractorArgs << "-MAG_ZEROPOINT" << QString::number(params.magzero);

    sextractorArgs <<  fileToProcess;

    sextractorProcess.clear();
    sextractorProcess = new QProcess();

    sextractorProcess->setWorkingDirectory(basePath);
    sextractorProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(sextractorProcess, &QProcess::readyReadStandardOutput, this, &ExternalSextractorSolver::logSextractor);

    emit logNeedsUpdating("Starting external sextractor...");
    emit logNeedsUpdating(sextractorBinaryPath + " " + sextractorArgs.join(' '));
    sextractorProcess->start(sextractorBinaryPath, sextractorArgs);
    if(justSextract)
        connect(sextractorProcess, QOverload<int>::of(&QProcess::finished), this, [this](int x){
            printSextractorOutput();
            if(x!=0)
                emit finished(x);
             if(getSextractorTable())
                 emit finished(0);
             else
                 emit finished(-1);
        });
    else
    {
        sextractorProcess->waitForFinished();
        printSextractorOutput();
    }

    return true;

}

//This method prints the output from the external sextractor
void ExternalSextractorSolver::printSextractorOutput()
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logNeedsUpdating(sextractorProcess->readAllStandardError().trimmed());
}

//The code for this method is copied and pasted and modified from OfflineAstrometryParser in KStars
//It runs the astrometry.net external program using the options selected.
bool ExternalSextractorSolver::runExternalSolver()
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    QFile sextractorFile(sextractorFilePath);
    if(!sextractorFile.exists())
    {
        emit logNeedsUpdating("Please Sextract the image first");
    }

    if(params.inParallel && !enoughRAMisAvailableFor(indexFolderPaths))
    {
        emit logNeedsUpdating("Not enough RAM is available on this system for loading the index files you have in parallel");
        emit logNeedsUpdating("You may want to disable the inParallel option.");
    }

    QStringList solverArgs=getSolverArgsList();

    if(autoGenerateAstroConfig)
        generateAstrometryConfigFile();

    solverArgs << "--backend-config" << confPath;

    cancelfn = basePath + "/" + baseName + ".cancel";
    solverArgs << "--cancel" << cancelfn;

    QString solutionFile = basePath + "/" + baseName + ".wcs";
    solverArgs << "-W" << solutionFile;

    solverArgs << sextractorFilePath;

    solver.clear();
    solver = new QProcess();

    solver->setProcessChannelMode(QProcess::MergedChannels);
    connect(solver, &QProcess::readyReadStandardOutput, this, &ExternalSextractorSolver::logSolver);
    connect(solver, QOverload<int>::of(&QProcess::finished), this, [this](int x){
        if(x!=0)
            emit finished(x);
        else if(getSextractorTable())
        {
           if(getSolutionInformation())
           {
                if(loadWCS())
                {
                    hasSolved = true;
                    emit finished(0);
                }
                else
                    finished(-1);
           }
           else
               emit finished(-1);
        }
        else
            emit finished(-1);
    });

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
    emit logNeedsUpdating("Command: " + solverPath + " " + solverArgs.join(" "));

    return true;
}

//The code for this method is copied and pasted and modified from OfflineAstrometryParser in KStars
//It runs the astrometry.net external program using the options selected.
bool ExternalSextractorSolver::runExternalClassicSolver()
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

    if(params.inParallel && !enoughRAMisAvailableFor(indexFolderPaths))
    {
        emit logNeedsUpdating("Not enough RAM is available on this system for loading the index files you have in parallel");
        emit logNeedsUpdating("You may want to disable the inParallel option.");
    }

    QStringList solverArgs=getClassicSolverArgsList();

    if(autoGenerateAstroConfig)
        generateAstrometryConfigFile();

    solverArgs << "--backend-config" << confPath;

    cancelfn = basePath + "/" + baseName + ".cancel";
    solverArgs << "--cancel" << cancelfn;

    QString solutionFile = basePath + "/" + baseName + ".wcs";
    solverArgs << "-W" << solutionFile;

    solverArgs << fileToProcess;

    solver.clear();
    solver = new QProcess();

    solver->setProcessChannelMode(QProcess::MergedChannels);
    connect(solver, &QProcess::readyReadStandardOutput, this, &ExternalSextractorSolver::logSolver);
    connect(solver, QOverload<int>::of(&QProcess::finished), this, [this](int x){
        if(x!=0)
            emit finished(x);
        else if(getSolutionInformation())
        {
            if(loadWCS())
            {
                hasSolved = true;
                emit finished(0);
            }
            else
                finished(-1);
        }
        else
            emit finished(-1);
    });

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

#ifdef Q_OS_OSX
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path            = env.value("PATH", "");
    QString pythonExecPath = "/usr/local/opt/python/libexec/bin";

    env.insert("PATH", "/Applications/KStars.app/Contents/MacOS/netpbm/bin:" + pythonExecPath + ":/usr/local/bin:" + path);

    solver->setProcessEnvironment(env);

#endif

    solver->start(solverPath, solverArgs);

    emit logNeedsUpdating("Starting external Astrometry.net solver using python and without Sextractor first...");
    emit logNeedsUpdating("Command: " + solverPath + " " + solverArgs.join(" "));

    return true;
}

//The code for this method is copied and pasted and modified from OfflineAstrometryParser in KStars
//It runs the astrometry.net external program using the options selected.
bool ExternalSextractorSolver::runExternalASTAPSolver()
{
    emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");

    QStringList solverArgs;


    QString solutionFile = basePath + "/" + baseName + ".ini";
    solverArgs << "-o" << solutionFile;
    solverArgs << "-r" << QString::number(params.search_radius);
    solverArgs << "-speed" << "auto";
    solverArgs << "-f" << fileToProcess;

    solver.clear();
    solver = new QProcess();

    solver->setProcessChannelMode(QProcess::MergedChannels);
    connect(solver, &QProcess::readyReadStandardOutput, this, &ExternalSextractorSolver::logSolver);
    connect(solver, QOverload<int>::of(&QProcess::finished), this, [this](int x){
        if(x!=0)
            emit finished(x);
        else if(getASTAPSolutionInformation())
        {
            if(loadWCS())
            {
                hasSolved = true;
                emit finished(0);
            }
            else
                emit finished(-1);
        }
        else
            emit finished(-1);
    });

    solver->start(astapBinaryPath, solverArgs);

    emit logNeedsUpdating("Starting external ASTAP Solver...");
    emit logNeedsUpdating("Command: " + astapBinaryPath + " " + solverArgs.join(" "));

    return true;
}


//This method is copied and pasted and modified from getSolverOptionsFromFITS in Align in KStars
//Then it was split in two parts (The other part is located in the MainWindow class)
//This part generates the argument list from the options for the external solver only
QStringList ExternalSextractorSolver::getSolverArgsList()
{
    QStringList solverArgs;

    // Start with always-used arguments
    solverArgs << "-O"
                << "--no-plots";

    // Now go over boolean options
    solverArgs << "--no-verify";

    if(params.resort) //We only want the resort options if they click resort.
    {
        solverArgs << "--resort";
        solverArgs << "--sort-column" << "MAG_AUTO";
        solverArgs << "--sort-ascending";
    }

    // downsample
    if(params.downsample > 1)
        solverArgs << "--downsample" << QString::number(params.downsample);

    solverArgs << "--odds-to-solve" << QString::number(exp(params.logratio_tosolve));
    solverArgs << "--odds-to-tune-up" << QString::number(exp(params.logratio_totune));
    //solverArgs << "--odds-to-keep" << QString::number(logratio_tokeep);  I'm not sure if this is one we need.


    solverArgs << "--width" << QString::number(stats.width);
    solverArgs << "--height" << QString::number(stats.height);
    solverArgs << "--x-column" << "X_IMAGE";
    solverArgs << "--y-column" << "Y_IMAGE";

                //Note This set of items is NOT NEEDED for Sextractor, it is needed to avoid python usage
                //This may need to be changed later, but since the goal for using sextractor is to avoid python, this is placed here.
    solverArgs << "--no-remove-lines";
    solverArgs << "--uniformize" << "0";

    if (use_scale)
        solverArgs << "-L" << QString::number(scalelo) << "-H" << QString::number(scalehi) << "-u" << units;

    if (use_position)
        solverArgs << "-3" << QString::number(search_ra) << "-4" << QString::number(search_dec) << "-5" << QString::number(params.search_radius);

    return solverArgs;
}

//This method is copied and pasted and modified from getSolverOptionsFromFITS in Align in KStars
//Then it was split in two parts (The other part is located in the MainWindow class)
//This part generates the argument list from the options for the external solver only
//Unlike the copy of this above, the arguments for eliminating python and using sextractor have been removed.
QStringList ExternalSextractorSolver::getClassicSolverArgsList()
{
    QStringList solverArgs;

    // Start with always-used arguments
    solverArgs << "-O"
                << "--no-plots";

    // Now go over boolean options
    solverArgs << "--no-verify";

    // downsample
    if(params.downsample > 1)
        solverArgs << "--downsample" << QString::number(params.downsample);

    if (use_scale)
        solverArgs << "-L" << QString::number(scalelo) << "-H" << QString::number(scalehi) << "-u" << units;

    if (use_position)
        solverArgs << "-3" << QString::number(search_ra) << "-4" << QString::number(search_dec) << "-5" << QString::number(params.search_radius);

    return solverArgs;
}

bool ExternalSextractorSolver::generateAstrometryConfigFile()
{
    confPath =  basePath + "/" + baseName + ".cfg";
    QFile configFile(confPath);
    if (configFile.open(QIODevice::WriteOnly) == false)
    {
        QMessageBox::critical(nullptr,"Message","Config file write error.");
        return false;
    }
    else
    {
        QTextStream out(&configFile);
        if(params.inParallel)
            out << "inparallel\n";
        out << "minwidth " << params.minwidth << "\n";
        out << "maxwidth " << params.maxwidth << "\n";
        out << "cpulimit " << params.solverTimeLimit << "\n";
        out << "autoindex\n";
        foreach(QString folder, indexFolderPaths)
        {
            out << "add_path " << folder << "\n";
        }
        configFile.close();
    }
    return true;
}


//These methods are for the logging of information to the textfield at the bottom of the window.

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
    int status = 0;   /*  CFITSIO status value MUST be initialized to zero!  */
    int hdunum, hdutype = ANY_HDU, ncols, ii, anynul;
    long nelements[1000];
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

        stars.clear();

        /* read each column, row by row */
        val = value;
        for (jj = 1; jj <= nrows && !status; jj++) {
            float starx = 0;
            float stary = 0;
            float mag = 0;
            float flux = 0;
            float peak = 0;
            float xx = 0;
            float yy = 0;
            float xy = 0;
            float HFR = 0;

            for (ii = 1; ii <= ncols; ii++)
            {
                for (kk = 1; kk <= nelements[ii]; kk++)
                {
                    /* read value as a string, regardless of intrinsic datatype */
                    if (fits_read_col_str (new_fptr,ii,jj,kk, 1, nullptrstr,
                                           &val, &anynul, &status) )
                        break;  /* jump out of loop on error */
                    if(ii == 1)
                        starx = QString(value).trimmed().toFloat();
                    if(ii == 2)
                        stary = QString(value).trimmed().toFloat();
                    if(ii == 3)
                        mag = QString(value).trimmed().toFloat();
                    if(ii == 4)
                        flux = QString(value).trimmed().toFloat();
                    if(ii == 5)
                        peak = QString(value).trimmed().toFloat();
                    if(ii == 6)
                        xx = QString(value).trimmed().toFloat();
                    if(ii == 7)
                        yy = QString(value).trimmed().toFloat();
                    if(ii == 8)
                        xy = QString(value).trimmed().toFloat();
                    if(calculateHFR && ii == 9)
                        HFR = QString(value).trimmed().toFloat();
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

            Star star = {starx, stary, mag, flux, peak, HFR, a, b, theta};

            stars.append(star);
        }
    fits_close_file(new_fptr, &status);

    if (status) fits_report_error(stderr, status); /* print any error message */
    return true;
}

//This method was based on a method in KStars.
//It reads the information from the Solution file from Astrometry.net and puts it into the solution
bool ExternalSextractorSolver::getSolutionInformation()
{
    QString solutionFile = basePath + "/" + baseName + ".wcs";
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
   // emit logNeedsUpdating(wcsinfo_stdout);

    QStringList wcskeys = wcsinfo_stdout.split(QRegExp("[\n]"));

    QStringList key_value;

    double ra = 0, dec = 0, orient = 0;
    double fieldw = 0, fieldh = 0, pixscale = 0;
    QString rastr, decstr;
    QString parity;

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
            else if (key_value[0] == "pixscale")
                pixscale = key_value[1].toDouble();
            else if (key_value[0] == "parity")
                parity = (key_value[1].toInt() == 0) ? "pos" : "neg";
        }
    }

    if(usingDownsampledImage)
        pixscale /= params.downsample;

    double raErr = 0;
    double decErr = 0;
    if(use_position)
    {
        raErr = (search_ra - ra) * 3600;
        decErr = (search_dec - dec) * 3600;
    }

    solution = {fieldw,fieldh,ra,dec,rastr,decstr,orient,pixscale, parity, raErr, decErr};
    return true;
}

//This method was based on a method in KStars.
//It reads the information from the Solution file from Astrometry.net and puts it into the solution
bool ExternalSextractorSolver::getASTAPSolutionInformation()
{
    QFile results(basePath + "/" + baseName + ".ini");

        if (!results.open(QIODevice::ReadOnly))
        {
            emit logNeedsUpdating("Failed to open solution file" + basePath + "/" + baseName + ".ini");
            return false;
        }

        QTextStream in(&results);
        QString line = in.readLine();

        QStringList ini = line.split("=");
        if (ini[1] == "F")
        {
            emit logNeedsUpdating("Solver failed. Try again.");
            return false;
        }
        double ra = 0, dec = 0, orient = 0;
        double fieldw = 0, fieldh = 0, pixscale = 0;
        char rastr[32], decstr[32];
        QString parity = "";

        bool ok[4] = {false};

        line = in.readLine();
        while (!line.isNull())
        {
            QStringList ini = line.split("=");
            if (ini[0] == "CRVAL1")
                ra = ini[1].trimmed().toDouble(&ok[0]);
            else if (ini[0] == "CRVAL2")
                dec = ini[1].trimmed().toDouble(&ok[1]);
            else if (ini[0] == "CDELT1")
                pixscale = ini[1].trimmed().toDouble(&ok[2]) * 3600.0;
            else if (ini[0] == "CROTA2")
                orient = ini[1].trimmed().toDouble(&ok[3]);

            line = in.readLine();
        }

        if (ok[0] && ok[1] && ok[2] && ok[3])
        {
            ra2hmsstring(ra, rastr);
            dec2dmsstring(dec, decstr);
            fieldw = stats.width * pixscale / 60;
            fieldh = stats.height * pixscale / 60;

            double raErr = 0;
            double decErr = 0;
            if(use_position)
            {
                raErr = (search_ra - ra) * 3600;
                decErr = (search_dec - dec) * 3600;
            }

            solution = {fieldw,fieldh,ra,dec,rastr,decstr,orient, pixscale, parity, raErr, decErr};

            emit logNeedsUpdating("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
            emit logNeedsUpdating(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
            emit logNeedsUpdating(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr).arg( decstr));
            if(use_position)
                emit logNeedsUpdating(QString("Field is: (%1, %2) deg from search coords.").arg( raErr).arg( decErr));
            emit logNeedsUpdating(QString("Field size: %1 x %2").arg( fieldw).arg( fieldh));
            emit logNeedsUpdating(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
            emit logNeedsUpdating(QString("Pixel Scale: %1\"").arg( pixscale ));

            return true;
        }
        else
        {
            emit logNeedsUpdating("Solver failed. Try again.");
            return false;
        }
}

//This method writes the table to the file
//I had to create it from the examples on NASA's website
//When I first made this program, I needed it to generate an xyls file from the internal sextraction
//Now it is just used on Windows for the external solving because it needs to use the internal sextractor and the external solver.
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/quick/node10.html
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/cookbook/node16.html
bool ExternalSextractorSolver::writeSextractorTable()
{

    if(sextractorFilePath == "")
    {
        sextractorFilePathIsTempFile = true;
        sextractorFilePath = basePath + "/" + baseName + ".xyls";
    }

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

    //Columns: X_IMAGE, double, pixels, Y_IMAGE, double, pixels
    char* ttype[] = { xcol, ycol, magcol };
    char* tform[] = { colFormat, colFormat, colFormat };
    char* tunit[] = { colUnits, colUnits, magUnits };
    const char* extfile = "Sextractor_File";

    float *xArray = new float[stars.size()];
    float *yArray = new float[stars.size()];
    float *magArray = new float[stars.size()];

    for (int i = 0; i < stars.size(); i++)
    {
        xArray[i] = stars.at(i).x;
        yArray[i] = stars.at(i).y;
        magArray[i] = stars.at(i).mag;
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

    column = 3;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, magArray, &status))
    {
        emit logNeedsUpdating(QString("Could not write magnitudes in binary table."));
        return false;
    }

    if(fits_close_file(new_fptr, &status))
    {
        emit logNeedsUpdating(QString("Error closing file."));
        return false;
    }

    free(xArray);
    free(yArray);
    free(magArray);

    return true;
}

//This is very necessary for solving non-fits images with external Sextractor
//This was copied and pasted and modified from ImageToFITS in fitsdata in KStars
bool ExternalSextractorSolver::saveAsFITS()
{
    QFileInfo fileInfo(fileToProcess.toLatin1());
    QString newFilename = basePath + "/" + baseName + "_solve.fits";

    int status = 0;
    fitsfile * new_fptr;

    //I am hoping that this is correct.
    //I"m trying to set these two variables based on the ndim variable since this class doesn't have access to these variables.
    long naxis;
    int m_Channels;
    if (stats.ndim < 3)
    {
        m_Channels = 1;
        naxis = 2;
    }
    else
    {
        m_Channels = 3;
        naxis = 3;
    }

    long nelements, exposure;
    long naxes[3] = { stats.width, stats.height, m_Channels };
    char error_status[512] = {0};

    QFileInfo newFileInfo(newFilename);
    if(newFileInfo.exists())
        QFile(newFilename).remove();

    nelements = stats.samples_per_channel * m_Channels;

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, newFilename.toLatin1(), &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    fitsfile *fptr = new_fptr;

    if (fits_create_img(fptr, BYTE_IMG, naxis, naxes, &status))
    {
        emit logNeedsUpdating(QString("fits_create_img failed: %1").arg(error_status));
        status = 0;
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        return false;
    }

    /* Write Data */
    if (fits_write_img(fptr, stats.dataType, 1, nelements, m_ImageBuffer, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    /* Write keywords */

    exposure = 1;
    fits_update_key(fptr, TLONG, "EXPOSURE", &exposure, "Total Exposure Time", &status);

    // NAXIS1
    if (fits_update_key(fptr, TUSHORT, "NAXIS1", &(stats.width), "length of data axis 1", &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    // NAXIS2
    if (fits_update_key(fptr, TUSHORT, "NAXIS2", &(stats.height), "length of data axis 2", &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    // ISO Date
    if (fits_write_date(fptr, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    fileToProcess = newFilename;
    fileToProcessIsTempFile = true;

    fits_flush_file(fptr, &status);

    if(fits_close_file(fptr, &status))
    {
        emit logNeedsUpdating(QString("Error closing file."));
        return false;
    }

    emit logNeedsUpdating("Saved FITS file:" + fileToProcess);

    return true;
}

//This was essentially copied from KStars' loadWCS method and split in half with some modifications.
bool ExternalSextractorSolver::loadWCS()
{
    QString solutionFile = basePath + "/" + baseName + ".wcs";

    emit logNeedsUpdating("Loading WCS from file...");

    int status = 0;
    char * header;
    int nkeyrec, nreject, nwcs;

    fitsfile *fptr { nullptr };

    if (fits_open_diskfile(&fptr, fileToProcess.toLatin1(), READONLY, &status))
    {
        emit logNeedsUpdating(QString("Error opening fits file %1").arg(fileToProcess));
        return false;
    }

    if (fits_hdr2str(fptr, 1, nullptr, 0, &header, &nkeyrec, &status))
    {
        char errmsg[512];
        fits_get_errstatus(status, errmsg);
        emit logNeedsUpdating(QString("ERROR %1: %2.").arg(status).arg(wcshdr_errmsg[status]));
        return false;
    }

    if ((status = wcspih(header, nkeyrec, WCSHDR_all, -3, &nreject, &nwcs, &m_wcs)) != 0)
    {
        free(header);
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        emit logNeedsUpdating(QString("wcspih ERROR %1: %2.").arg(status).arg(wcshdr_errmsg[status]));
        return false;
    }

    free(header);

    if (m_wcs == nullptr)
    {
        emit logNeedsUpdating("No world coordinate systems found.");
        return false;
    }

    // FIXME: Call above goes through EVEN if no WCS is present, so we're adding this to return for now.
    if (m_wcs->crpix[0] == 0)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        emit logNeedsUpdating("No world coordinate systems found.");
        return false;
    }

    if ((status = wcsset(m_wcs)) != 0)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        emit logNeedsUpdating(QString("wcsset error %1: %2.").arg(status).arg(wcs_errmsg[status]));
        return false;
    }

    emit logNeedsUpdating("Finished Loading WCS...");

    return true;
}

//This was essentially copied from KStars' loadWCS method and split in half with some modifications
wcs_point *ExternalSextractorSolver::getWCSCoord()
{
    if(!m_wcs)
        return nullptr;

    int w  = stats.width;
    int h = stats.height;
    wcs_point * wcs_coord = new wcs_point[w * h];
    wcs_point * p = wcs_coord;
    double imgcrd[2], phi = 0, pixcrd[2], theta = 0, world[2];
    int status;
    int stat[2];

    for (int i = 0; i < h; i++)
    {
        for (int j = 0; j < w; j++)
        {
            pixcrd[0] = j;
            pixcrd[1] = i;

            if ((status = wcsp2s(m_wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0])) != 0)
            {
                emit logNeedsUpdating(QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]));
            }
            else
            {
                p->ra  = world[0];
                p->dec = world[1];

                p++;
            }
        }
    }
    return wcs_coord;
}

QList<Star> ExternalSextractorSolver::getStarsWithRAandDEC()
{
    if(!m_wcs)
        return stars;

    double imgcrd[2], phi = 0, pixcrd[2], theta = 0, world[2];
    int status;
    int stat[2];

    QList<Star> refinedStars;
    foreach(Star star, stars)
    {
        double ra = HUGE_VAL;
        double dec = HUGE_VAL;
        pixcrd[0] = star.x;
        pixcrd[1] = star.y;

        if ((status = wcsp2s(m_wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0])) != 0)
        {
            emit logNeedsUpdating(QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]));
        }
        else
        {
            ra  = world[0];
            dec = world[1];
        }

        char rastr[32], decstr[32];
        ra2hmsstring(ra, rastr);
        dec2dmsstring(dec, decstr);

        //We do need to correct for all the downsampling as well as add RA/DEC info
        Star refinedStar = {star.x, star.y, star.mag, star.flux, star.peak, star.HFR, star.a, star.b, star.theta, (float)ra, (float)dec, rastr, decstr};
        refinedStars.append(refinedStar);
    }
    return refinedStars;
}


