/*  ExternalSextractorSolver, StellarSolver Internal Library developed by Robert Lancaster, 2020

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

static int solverNum = 1;

ExternalSextractorSolver::ExternalSextractorSolver(ProcessType type, ExtractorType sexType, SolverType solType,
        FITSImage::Statistic imagestats, uint8_t const *imageBuffer, QObject *parent) : InternalSextractorSolver(type, sexType,
                    solType, imagestats, imageBuffer, parent)
{

    //This sets the base name used for the temp files.
    baseName = "externalSextractorSolver_" + QString::number(solverNum++);

    //The code below sets default paths for these key external file settings.

#if defined(Q_OS_OSX)
    if(QFile("/usr/local/bin/solve-field").exists())
        setExternalFilePaths(getMacHomebrewPaths());
    else
        setExternalFilePaths(getMacInternalPaths());
#elif defined(Q_OS_LINUX)
    setExternalFilePaths(getLinuxDefaultPaths());
#else //Windows
    setExternalFilePaths(getWinANSVRPaths());
#endif

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

//The following methods are available to get the default paths for different operating systems and configurations.

ExternalProgramPaths ExternalSextractorSolver::getLinuxDefaultPaths()
{
    return ExternalProgramPaths
    {
        "/etc/astrometry.cfg",          //confPath
        "/usr/bin/sextractor",          //sextractorBinaryPath
        "/usr/bin/solve-field",         //solverPath
        (QFile("/bin/astap").exists()) ?
        "/bin/astap" :              //astapBinaryPath
        "/opt/astap/astap",
        "/usr/bin/wcsinfo"              //wcsPath
    };
}

ExternalProgramPaths ExternalSextractorSolver::getLinuxInternalPaths()
{
    return ExternalProgramPaths
    {
        "$HOME/.local/share/kstars/astrometry/astrometry.cfg",  //confPath
        "/usr/bin/sextractor",                                  //sextractorBinaryPath
        "/usr/bin/solve-field",                                 //solverPath
        (QFile("/bin/astap").exists()) ?
        "/bin/astap" :                                      //astapBinaryPath
        "/opt/astap/astap",
        "/usr/bin/wcsinfo"                                      //wcsPath
    };
}

ExternalProgramPaths ExternalSextractorSolver::getMacHomebrewPaths()
{
    return ExternalProgramPaths
    {
        "/usr/local/etc/astrometry.cfg",                //confPath
        "/usr/local/bin/sex",                           //sextractorBinaryPath
        "/usr/local/bin/solve-field",                   //solverPath
        "/Applications/ASTAP.app/Contents/MacOS/astap", //astapBinaryPath
        "/usr/local/bin/wcsinfo"                        //wcsPath
    };
}

ExternalProgramPaths ExternalSextractorSolver::getMacInternalPaths()
{
    return ExternalProgramPaths
    {
        "/Applications/KStars.app/Contents/MacOS/astrometry/bin/astrometry.cfg",    //confPath
        "/Applications/KStars.app/Contents/MacOS/astrometry/bin/sex",               //sextractorBinaryPath
        "/Applications/KStars.app/Contents/MacOS/astrometry/bin/solve-field",       //solverPath
        "/Applications/ASTAP.app/Contents/MacOS/astap",                             //astapBinaryPath
        "/Applications/KStars.app/Contents/MacOS/astrometry/bin/wcsinfo"            //wcsPath
    };
}

ExternalProgramPaths ExternalSextractorSolver::getWinANSVRPaths()
{
    return ExternalProgramPaths
    {
        QDir::homePath() + "/AppData/Local/cygwin_ansvr/etc/astrometry/backend.cfg",    //confPath
        "",                                                                             //sextractorBinaryPath
        QDir::homePath() + "/AppData/Local/cygwin_ansvr/lib/astrometry/bin/solve-field.exe",               //solverPath
        "C:/Program Files/astap/astap.exe",                                             //astapBinaryPath
        QDir::homePath() + "/AppData/Local/cygwin_ansvr/lib/astrometry/bin/wcsinfo.exe" //wcsPath
    };
}

ExternalProgramPaths ExternalSextractorSolver::getWinCygwinPaths()
{
    return ExternalProgramPaths
    {
        "C:/cygwin64/usr/etc/astrometry.cfg",   //confPath
        "",                                     //sextractorBinaryPath
        "C:/cygwin64/bin/solve-field",          //solverPath
        "C:/Program Files/astap/astap.exe",     //astapBinaryPath
        "C:/cygwin64/bin/wcsinfo"               //wcsPath
    };
}

void ExternalSextractorSolver::setExternalFilePaths(ExternalProgramPaths paths)
{
    confPath = paths.confPath;
    sextractorBinaryPath = paths.sextractorBinaryPath;
    solverPath = paths.solverPath;
    astapBinaryPath = paths.astapBinaryPath;
    wcsPath = paths.wcsPath;
}

int ExternalSextractorSolver::extract()
{
    if(m_ExtractorType == EXTRACTOR_EXTERNAL)
    {
#ifdef _WIN32  //Note that this is just a warning, if the user has Sextractor installed somehow on Windows, they could use it.
        emit logOutput("Sextractor is not easily installed on windows. Please select the Internal Sextractor and External Solver.");
#endif

        if(!QFileInfo(sextractorBinaryPath).exists())
        {
            emit logOutput("There is no sextractor at " + sextractorBinaryPath + ", Aborting");
            return -1;
        }
    }

    if(sextractorFilePath == "")
    {
        sextractorFilePathIsTempFile = true;
        sextractorFilePath = basePath + "/" + baseName + ".xyls";
    }

    if(m_ProcessType == EXTRACT_WITH_HFR || m_ProcessType == EXTRACT)
        return runExternalSextractor();
    else
    {
        int fail = 0;
        if(m_ExtractorType == EXTRACTOR_INTERNAL)
        {
            fail = runSEPSextractor();
            if(fail != 0)
                return fail;
            return(writeSextractorTable());
        }
        else if(m_ExtractorType == EXTRACTOR_EXTERNAL)
            return(runExternalSextractor());
    }
    return -1;
}

void ExternalSextractorSolver::run()
{
    if(computingWCS)
    {
        if(hasSolved)
        {
            computeWCSCoord();
            if(computeWCSForStars)
                appendStarsRAandDEC();
            emit finished(0);
        }
        else
            emit finished(-1);
        computingWCS = false;
        return;
    }

    if(astrometryLogLevel != LOG_NONE && logToFile)
    {
        if(logFileName == "")
            logFileName = basePath + "/" + baseName + ".log.txt";
        if(QFile(logFileName).exists())
            QFile(logFileName).remove();
    }

    if(cancelfn == "")
        cancelfn = basePath + "/" + baseName + ".cancel";
    if(solutionFile == "")
        solutionFile = basePath + "/" + baseName + ".wcs";

    if(QFile(cancelfn).exists())
        QFile(cancelfn).remove();
    if(QFile(solutionFile).exists())
        QFile(solutionFile).remove();

    //These are the solvers that use External Astrometry.
    if(m_SolverType == SOLVER_LOCALASTROMETRY)
    {
        if(!QFileInfo(solverPath).exists())
        {
            emit logOutput("There is no astrometry solver at " + solverPath + ", Aborting");
            emit finished(-1);
            return;
        }
#ifdef _WIN32
        if(params.inParallel)
        {
            emit logOutput("The external ANSVR solver on windows does not handle the inparallel option well, disabling it for this run.");
            params.inParallel = false;
        }
#endif
    }
    else if(m_SolverType == SOLVER_ASTAP)
    {
        if(!QFileInfo(astapBinaryPath).exists())
        {
            emit logOutput("There is no ASTAP solver at " + astapBinaryPath + ", Aborting");
            emit finished(-1);
            return;
        }
    }

    if(sextractorFilePath == "")
    {
        sextractorFilePathIsTempFile = true;
        sextractorFilePath = basePath + "/" + baseName + ".xyls";
    }

    switch(m_ProcessType)
    {
        case EXTRACT:
        case EXTRACT_WITH_HFR:
            emit finished(extract());
            break;

        case SOLVE:
        {
            if(m_ExtractorType == EXTRACTOR_BUILTIN && m_SolverType == SOLVER_LOCALASTROMETRY)
                emit finished(runExternalSolver());
            else if(m_ExtractorType == EXTRACTOR_BUILTIN && m_SolverType == SOLVER_ASTAP)
                emit finished(runExternalASTAPSolver());
            else
            {
                if(!hasSextracted)
                {
                    int fail = extract();
                    if(fail != 0)
                    {
                        emit finished(fail);
                        return;
                    }
                }

                if(hasSextracted)
                {
                    if(m_SolverType == SOLVER_ASTAP)
                        emit finished(runExternalASTAPSolver());
                    else
                        emit finished(runExternalSolver());
                }
                else
                    emit finished(-1);
            }

        }
        break;

        default:
            break;
    }

    cleanupTempFiles();
}

//This method generates child solvers with the options of the current solver
SextractorSolver* ExternalSextractorSolver::spawnChildSolver(int n)
{
    ExternalSextractorSolver *solver = new ExternalSextractorSolver(m_ProcessType, m_ExtractorType, m_SolverType, m_Statistics,
            m_ImageBuffer, nullptr);
    solver->stars = stars;
    solver->basePath = basePath;
    solver->baseName = baseName + "_" + QString::number(n);
    solver->hasSextracted = true;
    solver->sextractorFilePath = sextractorFilePath;
    solver->fileToProcess = fileToProcess;
    solver->sextractorBinaryPath = sextractorBinaryPath;
    solver->confPath = confPath;
    solver->solverPath = solverPath;
    solver->astapBinaryPath = astapBinaryPath;
    solver->wcsPath = wcsPath;
    solver->cleanupTemporaryFiles = cleanupTemporaryFiles;
    solver->autoGenerateAstroConfig = autoGenerateAstroConfig;

    solver->isChildSolver = true;
    solver->params = params;
    solver->indexFolderPaths = indexFolderPaths;
    //Set the log level one less than the main solver
    if(m_SSLogLevel == LOG_VERBOSE )
        solver->m_SSLogLevel = LOG_NORMAL;
    if(m_SSLogLevel == LOG_NORMAL || m_SSLogLevel == LOG_OFF)
        solver->m_SSLogLevel = LOG_OFF;
    if(use_scale)
        solver->setSearchScale(scalelo, scalehi, scaleunit);
    if(use_position)
        solver->setSearchPositionInDegrees(search_ra, search_dec);
    if(astrometryLogLevel != SSolver::LOG_NONE || m_SSLogLevel != SSolver::LOG_OFF)
        connect(solver, &SextractorSolver::logOutput, this, &SextractorSolver::logOutput);
    //This way they all share a solved and cancel fn
    solver->solutionFile = solutionFile;
    solver->cancelfn = cancelfn;
    //solver->solvedfn = basePath + "/" + baseName + ".solved";
    return solver;
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
    if(!isChildSolver)
        emit logOutput("Aborting ...");
    wasAborted = true;
}

void ExternalSextractorSolver::cleanupTempFiles()
{
    if(cleanupTemporaryFiles)
    {
        QDir temp(basePath);
        //Sextractor Files
        temp.remove(baseName + ".param");
        temp.remove(baseName + ".conv");
        temp.remove(baseName + ".cfg");

        //ASTAP files
        temp.remove(baseName + ".ini");

        //Astrometry Files
        temp.remove(baseName + ".corr");
        temp.remove(baseName + ".rdls");
        temp.remove(baseName + ".axy");
        temp.remove(baseName + ".corr");
        temp.remove(baseName + ".new");
        temp.remove(baseName + ".match");
        temp.remove(baseName + "-indx.xyls");
        temp.remove(baseName + ".solved");

        //Other Files
        if(!isChildSolver)
            QFile(solutionFile).remove();
        //if(!isChildSolver)
        //    QFile(cancelfn).remove();
        if(sextractorFilePathIsTempFile)
            QFile(sextractorFilePath).remove();
        if(fileToProcessIsTempFile)
            QFile(fileToProcess).remove();
    }
}

//This method is copied and pasted and modified from the code I wrote to use sextractor in OfflineAstrometryParser in KStars
//It creates key files needed to run Sextractor from the desired options, then runs the sextractor program using the options.
int ExternalSextractorSolver::runExternalSextractor()
{
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Configuring external Sextractor");
    QFileInfo file(fileToProcess);
    if(!file.exists())
        return -1;
    if(file.suffix() != "fits" && file.suffix() != "fit")
    {
        int ret = saveAsFITS();
        if(ret != 0)
            return ret;
    }
    else
    {
        QString newFileURL = basePath + "/" + baseName + "." + file.suffix();
        QFile::copy(fileToProcess, newFileURL);
        fileToProcess = newFileURL;
        fileToProcessIsTempFile = true;
    }

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
        QMessageBox::critical(nullptr, "Message", "Sextractor file write error.");
        return -1;
    }
    else
    {
        QTextStream out(&paramFile);
        out << "X_IMAGE\n";//                  Object position along x                                   [pixel]
        out << "Y_IMAGE\n";//                  Object position along y                                   [pixel]
        out << "MAG_AUTO\n";//                 Kron-like elliptical aperture magnitude                   [mag]
        out << "FLUX_AUTO\n";//                Flux within a Kron-like elliptical aperture               [count]
        out << "FLUX_MAX\n";//                 Peak flux above background                                [count]
        out << "CXX_IMAGE\n";//                Cxx object ellipse parameter                              [pixel**(-2)]
        out << "CYY_IMAGE\n";//                Cyy object ellipse parameter                              [pixel**(-2)]
        out << "CXY_IMAGE\n";//                Cxy object ellipse parameter                              [pixel**(-2)]
        if(m_ProcessType == EXTRACT_WITH_HFR)
            out << "FLUX_RADIUS\n";//              Fraction-of-light radii                                   [pixel]
        paramFile.close();
    }
    sextractorArgs << "-PARAMETERS_NAME" << paramPath;


    //sextractor needs a default.conv file in the working directory
    //This creates the default one

    QString convPath =  basePath + "/" + baseName + ".conv";
    QFile convFile(convPath);
    if (convFile.open(QIODevice::WriteOnly) == false)
    {
        QMessageBox::critical(nullptr, "Message", "Sextractor CONV filter write error.");
        return -1;
    }
    else
    {
        QTextStream out(&convFile);
        out << "CONV Filter Generated by StellarSolver Internal Library\n";
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

    sextractorArgs << "-CLEAN" << ((params.clean == 1) ? "Y" : "N");
    sextractorArgs << "-CLEAN_PARAM" << QString::number(params.clean_param);

    //------------------------------ Photometry -----------------------------------
    sextractorArgs << "-PHOT_AUTOPARAMS" << QString::number(params.kron_fact) + "," + QString::number(params.r_min);
    sextractorArgs << "-MAG_ZEROPOINT" << QString::number(params.magzero);

    sextractorArgs <<  fileToProcess;

    sextractorProcess.clear();
    sextractorProcess = new QProcess();

    sextractorProcess->setWorkingDirectory(basePath);
    sextractorProcess->setProcessChannelMode(QProcess::MergedChannels);
    if(m_SSLogLevel != LOG_OFF)
        connect(sextractorProcess, &QProcess::readyReadStandardOutput, this, &ExternalSextractorSolver::logSextractor);

    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting external sextractor with the " + params.listName + " profile...");
    emit logOutput(sextractorBinaryPath + " " + sextractorArgs.join(' '));

    sextractorProcess->start(sextractorBinaryPath, sextractorArgs);
    sextractorProcess->waitForFinished(30000); //Will timeout after 30 seconds
    emit logOutput(sextractorProcess->readAllStandardError().trimmed());

    if(sextractorProcess->exitCode() != 0 || sextractorProcess->exitStatus() == QProcess::CrashExit)
        return sextractorProcess->exitCode();

    int exitCode = getStarsFromXYLSFile();
    if(exitCode != 0)
        return exitCode;

    if(useSubframe)
    {
        for(int i = 0; i < stars.size(); i++)
        {
            FITSImage::Star star = stars.at(i);
            if(!subframe.contains(star.x, star.y))
            {
                stars.removeAt(i);
                i--;
            }
        }
    }

    applyStarFilters(stars);

    hasSextracted = true;

    return 0;

}

//The code for this method is copied and pasted and modified from OfflineAstrometryParser in KStars
//It runs the astrometry.net external program using the options selected.
int ExternalSextractorSolver::runExternalSolver()
{
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    if(m_ExtractorType == EXTRACTOR_BUILTIN)
        emit logOutput("Configuring external Astrometry.net solver classically: using python and without Sextractor first");
    else
        emit logOutput("Configuring external Astrometry.net solver using an xylist input");

    if(m_ExtractorType == EXTRACTOR_BUILTIN)
    {
        QFileInfo file(fileToProcess);
        if(!file.exists())
        {
            emit logOutput("The requested file to solve does not exist");
            return -1;
        }

        //Making a copy of the file and putting it in the temp directory
        //So that we can find all the temporary files and delete them later
        //That way we don't pollute the directory the original image is located in
        QString newFileURL = basePath + "/" + baseName + "." + file.suffix();
        QFile::copy(fileToProcess, newFileURL);
        fileToProcess = newFileURL;
        fileToProcessIsTempFile = true;
    }
    else
    {
        QFileInfo sextractorFile(sextractorFilePath);
        if(!sextractorFile.exists())
        {
            emit logOutput("Please Sextract the image first");
        }
        if(isChildSolver)
        {
            QString newFileURL = basePath + "/" + baseName + "." + sextractorFile.suffix();
            QFile::copy(sextractorFilePath, newFileURL);
            sextractorFilePath = newFileURL;
            sextractorFilePathIsTempFile = true;
        }
    }

    QStringList solverArgs = getSolverArgsList();

    if(m_ExtractorType == EXTRACTOR_BUILTIN)
    {
        solverArgs << "--keep-xylist" << sextractorFilePath;
        solverArgs << fileToProcess;
    }
    else
        solverArgs << sextractorFilePath;

    solver.clear();
    solver = new QProcess();

    solver->setProcessChannelMode(QProcess::MergedChannels);
    if(astrometryLogLevel != LOG_NONE)
        connect(solver, &QProcess::readyReadStandardOutput, this, &ExternalSextractorSolver::logSolver);

#ifdef _WIN32 //This will set up the environment so that the ANSVR internal solver will work when started from this program.  This is needed for all types of astrometry solvers using ANSVR
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path            = env.value("Path", "");
    QString ansvrPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/";
    QString pathsToInsert = ansvrPath + "bin;";
    pathsToInsert += ansvrPath + "lib/lapack;";
    pathsToInsert += ansvrPath + "lib/astrometry/bin;";
    env.insert("Path", pathsToInsert + path);
    solver->setProcessEnvironment(env);
#endif

#ifdef Q_OS_OSX //This is needed so that astrometry.net can find netpbm and python on Mac when started from this program.  It is not needed when using an alternate sextractor
    if(m_ExtractorType == SEXTRACTOR_BUILTIN && solverType == SOLVER_LOCALASTROMETRY)
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString path            = env.value("PATH", "");
        QString pythonExecPath = "/usr/local/opt/python/libexec/bin";
        env.insert("PATH", "/Applications/KStars.app/Contents/MacOS/netpbm/bin:" + pythonExecPath + ":/usr/local/bin:" + path);
        solver->setProcessEnvironment(env);
    }
#endif

    solver->start(solverPath, solverArgs);
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting external Astrometry.net solver with the " + params.listName + " profile...");
    emit logOutput("Command: " + solverPath + " " + solverArgs.join(" "));

    solver->waitForFinished(params.solverTimeLimit * 1000 * 1.2); //Set to timeout in a little longer than the timeout
    if(solver->error() == QProcess::Timedout)
    {
        emit logOutput("Solver timed out, aborting");
        abort();
        return solver->exitCode();
    }
    if(solver->exitCode() != 0)
        return solver->exitCode();
    if(solver->exitStatus() == QProcess::CrashExit)
        return -1;
    if(wasAborted)
        return -1;
    if(!getSolutionInformation())
        return -1;
    loadWCS(); //Attempt to Load WCS, but don't totally fail if you don't find it.
    hasSolved = true;
    return 0;
}

//The code for this method is copied and pasted and modified from OfflineAstrometryParser in KStars
//It runs the astrometry.net external program using the options selected.
int ExternalSextractorSolver::runExternalASTAPSolver()
{
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Configuring external ASTAP solver");

    if(m_ExtractorType != EXTRACTOR_BUILTIN)
    {
        QFileInfo file(fileToProcess);
        if(!file.exists())
            return -1;

        QString newFileURL = basePath + "/" + baseName + "." + file.suffix();
        QFile::copy(fileToProcess, newFileURL);
        fileToProcess = newFileURL;
        fileToProcessIsTempFile = true;
    }

    QStringList solverArgs;

    QString astapSolutionFile = basePath + "/" + baseName + ".ini";
    solverArgs << "-o" << astapSolutionFile;
    solverArgs << "-speed" << "auto";
    solverArgs << "-f" << fileToProcess;
    solverArgs << "-wcs";
    if(params.downsample > 1)
        solverArgs << "-z" << QString::number(params.downsample);
    if(use_scale)
    {
        double scalemid = (scalehi + scalelo) / 2;
        double degreesFOV = convertToDegreeHeight(scalemid);
        solverArgs << "-fov" << QString::number(degreesFOV);
    }
    if(use_position)
    {
        solverArgs << "-ra" << QString::number(search_ra / 15.0); //Convert ra to hours
        solverArgs << "-spd" << QString::number(search_dec + 90); //Convert dec to spd
        solverArgs << "-r" << QString::number(params.search_radius);
    }
    if(astrometryLogLevel == LOG_ALL || astrometryLogLevel == LOG_VERB)
        solverArgs << "-log";

    solver.clear();
    solver = new QProcess();

    solver->setProcessChannelMode(QProcess::MergedChannels);
    if(astrometryLogLevel != LOG_NONE)
        connect(solver, &QProcess::readyReadStandardOutput, this, &ExternalSextractorSolver::logSolver);

    solver->start(astapBinaryPath, solverArgs);

    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting external ASTAP Solver with the " + params.listName + " profile...");
    emit logOutput("Command: " + astapBinaryPath + " " + solverArgs.join(" "));

    solver->waitForFinished(params.solverTimeLimit * 1000 * 1.2); //Set to timeout in a little longer than the timeout
    if(solver->error() == QProcess::Timedout)
    {
        emit logOutput("Solver timed out, aborting");
        abort();
        return solver->exitCode();
    }
    if(solver->exitCode() != 0)
        return solver->exitCode();
    if(solver->exitStatus() == QProcess::CrashExit)
        return -1;
    if(!getASTAPSolutionInformation())
        return -1;
    loadWCS(); //Attempt to Load WCS, but don't totally fail if you don't find it.
    hasSolved = true;
    return 0;
}

//This method is copied and pasted and modified from getSolverOptionsFromFITS in Align in KStars
//Then it was split in two parts (The other part is located in the MainWindow class)
//This part generates the argument list from the options for the external solver only
QStringList ExternalSextractorSolver::getSolverArgsList()
{
    QStringList solverArgs;

    // Start with always-used arguments.  The way we are using Astrometry.net,
    //We really would prefer that it always overwrite existing files, that it not waste any time
    //writing plots to a file, and that it doesn't run the verification.
    //We would also like the Coordinates found to be the center of the image
    solverArgs << "-O" << "--no-plots" << "--no-verify" << "--crpix-center";

    //We would also prefer that it not write these temporary files, because they are a waste of
    //resources, time, hard disk space, and memory card life since we aren't using them.
    solverArgs << "--match" << "none";
    solverArgs << "--corr" << "none";
    solverArgs << "--new-fits" << "none";
    solverArgs << "--rdls" << "none";

    //This parameter controls whether to resort the stars or not.
    if(params.resort)
        solverArgs << "--resort";

    if(depthhi != -1 && depthlo != -1)
        solverArgs << "--depth" << QString("%1-%2").arg(depthlo).arg(depthhi);

    if(params.keepNum != 0)
        solverArgs << "--objs" << QString("%1").arg(params.keepNum);

    //This will shrink the image so that it is easier to solve.  It is only useful if you are sending an image.
    //It is not used if you are solving an xylist as in the classic astrometry.net solver
    if(params.downsample > 1 && m_ExtractorType == EXTRACTOR_BUILTIN)
        solverArgs << "--downsample" << QString::number(params.downsample);

    //I am not sure if we want to provide these options or not.  They do make a huge difference in solving time
    //But changing them can be dangerous because it can cause false positive solves.
    solverArgs << "--odds-to-solve" << QString::number(exp(params.logratio_tosolve));
    solverArgs << "--odds-to-tune-up" << QString::number(exp(params.logratio_totune));
    //solverArgs << "--odds-to-keep" << QString::number(logratio_tokeep);  I'm not sure if this is one we need.

    if (use_scale)
        solverArgs << "-L" << QString::number(scalelo) << "-H" << QString::number(scalehi) << "-u" << getScaleUnitString();

    if (use_position)
        solverArgs << "-3" << QString::number(search_ra) << "-4" << QString::number(search_dec) << "-5" << QString::number(
                       params.search_radius);

    //The following options are unnecessary if you are sending an image to Astrometry.net
    //And not an xylist
    if(m_ExtractorType != EXTRACTOR_BUILTIN)
    {
        //These options are required to use an xylist, so all solvers that don't send an image
        //to Astrometry.net must send these 4 options.
        solverArgs << "--width" << QString::number(m_Statistics.width);
        solverArgs << "--height" << QString::number(m_Statistics.height);
        solverArgs << "--x-column" << "X_IMAGE";
        solverArgs << "--y-column" << "Y_IMAGE";

        //These sort options are required to sort when you have an xylist input
        //We only want to send them if the resort option is selected though.
        if(params.resort)
        {
            solverArgs << "--sort-column" << "MAG_AUTO";
            solverArgs << "--sort-ascending";
        }

        //Note This set of items is NOT NEEDED for Sextractor, it is needed to avoid python usage
        //This may need to be changed later, but since the goal for using sextractor is to avoid python, this is placed here.
        solverArgs << "--no-remove-lines";
        solverArgs << "--uniformize" << "0";
    }

    //Don't need any argument for default level
    if(astrometryLogLevel == LOG_MSG || astrometryLogLevel == LOG_ERROR)
        solverArgs << "-v";
    else if(astrometryLogLevel == LOG_VERB || astrometryLogLevel == LOG_ALL)
        solverArgs << "-vv";

    if(autoGenerateAstroConfig || !QFile(confPath).exists())
        generateAstrometryConfigFile();

    //This sends the path to the config file.  Note that backend-config seems to be more universally recognized across
    //the different solvers than config
    solverArgs << "--backend-config" << confPath;

    //This sets the cancel filename for astrometry.net.  Astrometry will monitor for the creation of this file
    //In order to shut down and stop processing
    solverArgs << "--cancel" << cancelfn;

    //This sets the wcs file for astrometry.net.  This file will be very important for reading in WCS info later on
    solverArgs << "-W" << solutionFile;

    return solverArgs;
}

//This will generate a temporary Astrometry.cfg file to use for solving so that we have more control over these options
//for the external solvers from inside the program.
bool ExternalSextractorSolver::generateAstrometryConfigFile()
{
    confPath =  basePath + "/" + baseName + ".cfg";
    QFile configFile(confPath);
    if (configFile.open(QIODevice::WriteOnly) == false)
    {
        QMessageBox::critical(nullptr, "Message", "Config file write error.");
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
    if(sextractorProcess->canReadLine())
    {
        QString rawText(sextractorProcess->readLine().trimmed());
        QString cleanedString = rawText.remove("[1M>").remove("[1A");
        if(!cleanedString.isEmpty())
        {
            emit logOutput(cleanedString);
            if(logToFile)
            {
                QFile file(logFileName);
                if (file.open(QIODevice::Append | QIODevice::Text))
                {
                    QTextStream outstream(&file);
                    outstream << cleanedString << endl;
                    file.close();
                }
                else
                    emit logOutput(("Log File Write Error"));
            }
        }
    }
}

void ExternalSextractorSolver::logSolver()
{
    if(solver->canReadLine())
    {
        QString solverLine(solver->readLine().trimmed());
        if(!solverLine.isEmpty())
        {
            emit logOutput(solverLine);
            if(logToFile)
            {
                QFile file(logFileName);
                if (file.open(QIODevice::Append | QIODevice::Text))
                {
                    QTextStream outstream(&file);
                    outstream << solverLine << endl;
                    file.close();
                }
                else
                    emit logOutput(("Log File Write Error"));
            }
        }
    }
}

//This method is copied and pasted and modified from tablist.c in astrometry.net
//This is needed to load in the stars sextracted by an extrnal sextractor to get them into the table
int ExternalSextractorSolver::getStarsFromXYLSFile()
{
    QFile sextractorFile(sextractorFilePath);
    if(!sextractorFile.exists())
    {
        emit logOutput("Can't get sextractor file since it doesn't exist.");
        return -1;
    }

    fitsfile * new_fptr;
    char error_status[512];

    /* FITS file pointer, defined in fitsio.h */
    char *val, value[1000], nullptrstr[] = "*";
    int status = 0;   /*  CFITSIO status value MUST be initialized to zero!  */
    int hdunum, hdutype = ANY_HDU, ncols, ii, anynul;
    long nelements[1000];
    long jj, nrows, kk;

    if (fits_open_diskfile(&new_fptr, sextractorFilePath.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        emit logOutput(QString::fromUtf8(error_status));
        return status;
    }

    if ( fits_get_hdu_num(new_fptr, &hdunum) == 1 )
        /* This is the primary array;  try to move to the */
        /* first extension and see if it is a table */
        fits_movabs_hdu(new_fptr, 2, &hdutype, &status);
    else
        fits_get_hdu_type(new_fptr, &hdutype, &status); /* Get the HDU type */

    if (!(hdutype == ASCII_TBL || hdutype == BINARY_TBL))
    {
        emit logOutput("Wrong type of file");
        return -1;
    }

    fits_get_num_rows(new_fptr, &nrows, &status);
    fits_get_num_cols(new_fptr, &ncols, &status);

    for (jj = 1; jj <= ncols; jj++)
        fits_get_coltype(new_fptr, jj, nullptr, &nelements[jj], nullptr, &status);

    stars.clear();

    /* read each column, row by row */
    val = value;
    for (jj = 1; jj <= nrows && !status; jj++)
    {
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
                if (fits_read_col_str (new_fptr, ii, jj, kk, 1, nullptrstr,
                                       &val, &anynul, &status) )
                    break;  /* jump out of loop on error */
                if(m_SolverType == SOLVER_LOCALASTROMETRY || m_SolverType == SOLVER_ONLINEASTROMETRY)
                {
                    if(ii == 1)
                        starx = QString(value).trimmed().toFloat();
                    if(ii == 2)
                        stary = QString(value).trimmed().toFloat();
                    if(ii == 3)
                        flux = QString(value).trimmed().toFloat();
                }
                else if(m_SolverType == SOLVER_ASTAP)
                {
                    if(ii == 1)
                        starx = QString(value).trimmed().toFloat();
                    if(ii == 2)
                        stary = QString(value).trimmed().toFloat();
                }
                else
                {
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
                    if(m_ProcessType == EXTRACT_WITH_HFR && ii == 9)
                        HFR = QString(value).trimmed().toFloat();
                }
            }
        }

        //  xx  xy      or     a   b
        //  xy  yy             b   c
        //Note, I got this translation from these two sources which agree:
        //https://books.google.com/books?id=JNEn23UyHuAC&pg=PA84&lpg=PA84&dq=ellipse+xx+yy+xy&source=bl&ots=ynAWge4jlb&sig=ACfU3U1pqZTkx8Teu9pBTygI9F-WcTncrg&hl=en&sa=X&ved=2ahUKEwj0s-7C3I7oAhXblnIEHacAAf0Q6AEwBHoECAUQAQ#v=onepage&q=ellipse%20xx%20yy%20xy&f=false
        //https://cookierobotics.com/007/
        float a = 0;
        float b = 0;
        float theta = 0;
        if(m_SolverType != SOLVER_LOCALASTROMETRY && m_SolverType != SOLVER_ONLINEASTROMETRY && m_SolverType != SOLVER_ASTAP)
        {
            float thing = sqrt( pow(xx - yy, 2) + 4 * pow(xy, 2) );
            float lambda1 = (xx + yy + thing) / 2;
            float lambda2 = (xx + yy - thing) / 2;
            a = sqrt(lambda1);
            b = sqrt(lambda2);
            theta = qRadiansToDegrees(atan(xy / (lambda1 - yy)));
        }

        FITSImage::Star star = {starx, stary, mag, flux, peak, HFR, a, b, theta, 0, 0};

        stars.append(star);
    }
    fits_close_file(new_fptr, &status);

    if (status) fits_report_error(stderr, status); /* print any error message */

    return 0;
}

//This method was based on a method in KStars.
//It reads the information from the Solution file from Astrometry.net and puts it into the solution
bool ExternalSextractorSolver::getSolutionInformation()
{
    if(solutionFile == "")
        solutionFile = basePath + "/" + baseName + ".wcs";
    QFileInfo solutionInfo(solutionFile);
    if(!solutionInfo.exists())
    {
        emit logOutput("Solution file doesn't exist");
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
    wcsProcess.waitForFinished(30000); //Will timeout after 30 seconds
    QString wcsinfo_stdout = wcsProcess.readAllStandardOutput();

    //This is a quick way to find out what keys are available
    // emit logOutput(wcsinfo_stdout);

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

    solution = {fieldw, fieldh, ra, dec, orient, pixscale, parity, raErr, decErr};
    return true;
}

//This method was based on a method in KStars.
//It reads the information from the Solution file from Astrometry.net and puts it into the solution
bool ExternalSextractorSolver::getASTAPSolutionInformation()
{
    QFile results(basePath + "/" + baseName + ".ini");

    if (!results.open(QIODevice::ReadOnly))
    {
        emit logOutput("Failed to open solution file" + basePath + "/" + baseName + ".ini");
        return false;
    }

    QTextStream in(&results);
    QString line = in.readLine();

    QStringList ini = line.split("=");
    if(ini.count() <= 1)
    {
        emit logOutput("Results file is empty, try again.");
        return false;
    }
    if (ini[1] == "F")
    {
        emit logOutput("Solver failed. Try again.");
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
        fieldw = m_Statistics.width * pixscale / 60;
        fieldh = m_Statistics.height * pixscale / 60;

        double raErr = 0;
        double decErr = 0;
        if(use_position)
        {
            raErr = (search_ra - ra) * 3600;
            decErr = (search_dec - dec) * 3600;
        }

        solution = {fieldw, fieldh, ra, dec, orient, pixscale, parity, raErr, decErr};

        emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logOutput(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
        emit logOutput(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr).arg( decstr));
        if(use_position)
            emit logOutput(QString("Field is: (%1, %2) deg from search coords.").arg( raErr).arg( decErr));
        emit logOutput(QString("Field size: %1 x %2").arg( fieldw).arg( fieldh));
        emit logOutput(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
        emit logOutput(QString("Pixel Scale: %1\"").arg( pixscale ));

        return true;
    }
    else
    {
        emit logOutput("Solver failed. Try again.");
        return false;
    }
}

//This method writes the table to the file
//I had to create it from the examples on NASA's website
//When I first made this program, I needed it to generate an xyls file from the internal sextraction
//Now it is just used on Windows for the external solving because it needs to use the internal sextractor and the external solver.
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/quick/node10.html
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/cookbook/node16.html
int ExternalSextractorSolver::writeSextractorTable()
{
    int status = 0;
    fitsfile * new_fptr;

    if(m_ExtractorType == EXTRACTOR_INTERNAL && m_SolverType == SOLVER_ASTAP)
    {
        QFileInfo file(fileToProcess);
        if(!file.exists())
            return -1;

        if(file.suffix() != "fits" && file.suffix() != "fit")
        {
            int ret = saveAsFITS();
            if(ret != 0)
                return ret;
        }
        else
        {
            QString newFileURL = basePath + "/" + baseName + ".fits";
            QFile::copy(fileToProcess, newFileURL);
            fileToProcess = newFileURL;
            fileToProcessIsTempFile = true;
        }

        if (fits_open_diskfile(&new_fptr, fileToProcess.toLatin1(), READWRITE, &status))
        {
            fits_report_error(stderr, status);
            return status;
        }
    }
    else
    {
        if(sextractorFilePath == "")
        {
            sextractorFilePathIsTempFile = true;
            sextractorFilePath = basePath + "/" + baseName + ".xyls";
        }

        QFile sextractorFile(sextractorFilePath);
        if(sextractorFile.exists())
            sextractorFile.remove();

        if (fits_create_file(&new_fptr, sextractorFilePath.toLatin1(), &status))
        {
            fits_report_error(stderr, status);
            return status;
        }
    }

    int tfields = 3;
    int nrows = stars.size();
    QString extname = "Sextractor_File";

    //Columns: X_IMAGE, double, pixels, Y_IMAGE, double, pixels, MAG_AUTO, double, mag
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
        emit logOutput(QString("Could not create binary table."));
        return status;
    }

    int firstrow  = 1;  /* first row in table to write   */
    int firstelem = 1;

    int column = 1;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, xArray, &status))
    {
        emit logOutput(QString("Could not write x pixels in binary table."));
        return status;
    }

    column = 2;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, yArray, &status))
    {
        emit logOutput(QString("Could not write y pixels in binary table."));
        return status;
    }

    column = 3;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, magArray, &status))
    {
        emit logOutput(QString("Could not write magnitudes in binary table."));
        return status;
    }

    if(fits_close_file(new_fptr, &status))
    {
        emit logOutput(QString("Error closing file."));
        return status;
    }

    free(xArray);
    free(yArray);
    free(magArray);

    return 0;
}

//This is very necessary for solving non-fits images with external Sextractor
//This was copied and pasted and modified from ImageToFITS in fitsdata in KStars
int ExternalSextractorSolver::saveAsFITS()
{
    QFileInfo fileInfo(fileToProcess.toLatin1());
    QString newFilename = basePath + "/" + baseName + ".fits";

    int status = 0;
    fitsfile * new_fptr;

    //I am hoping that this is correct.
    //I"m trying to set these two variables based on the ndim variable since this class doesn't have access to these variables.
    long naxis;
    int m_Channels;
    if (m_Statistics.ndim < 3)
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
    long naxes[3] = { m_Statistics.width, m_Statistics.height, m_Channels };
    char error_status[512] = {0};

    QFileInfo newFileInfo(newFilename);
    if(newFileInfo.exists())
        QFile(newFilename).remove();

    nelements = m_Statistics.samples_per_channel * m_Channels;

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, newFilename.toLatin1(), &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    fitsfile *fptr = new_fptr;

    if (fits_create_img(fptr, BYTE_IMG, naxis, naxes, &status))
    {
        emit logOutput(QString("fits_create_img failed: %1").arg(error_status));
        status = 0;
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        return status;
    }

    /* Write Data */
    uint8_t *imageBuffer = new uint8_t[m_Statistics.samples_per_channel * m_Channels * m_Statistics.bytesPerPixel];
    memcpy(imageBuffer, m_ImageBuffer, m_Statistics.samples_per_channel * m_Channels * m_Statistics.bytesPerPixel);
    if (fits_write_img(fptr, m_Statistics.dataType, 1, nelements, imageBuffer, &status))
    {
        delete[] imageBuffer;
        fits_report_error(stderr, status);
        return status;
    }
    delete[] imageBuffer;

    /* Write keywords */

    exposure = 1;
    fits_update_key(fptr, TLONG, "EXPOSURE", &exposure, "Total Exposure Time", &status);

    // NAXIS1
    if (fits_update_key(fptr, TUSHORT, "NAXIS1", &(m_Statistics.width), "length of data axis 1", &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    // NAXIS2
    if (fits_update_key(fptr, TUSHORT, "NAXIS2", &(m_Statistics.height), "length of data axis 2", &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    // ISO Date
    if (fits_write_date(fptr, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    fileToProcess = newFilename;
    fileToProcessIsTempFile = true;

    fits_flush_file(fptr, &status);

    if(fits_close_file(fptr, &status))
    {
        emit logOutput(QString("Error closing file."));
        return status;
    }

    emit logOutput("Saved FITS file:" + fileToProcess);

    return 0;
}

//This was essentially copied from KStars' loadWCS method and split in half with some modifications.
int ExternalSextractorSolver::loadWCS()
{
    if(solutionFile == "")
        solutionFile = basePath + "/" + baseName + ".wcs";

    emit logOutput("Loading WCS from file...");

    QFile solution(solutionFile);
    if(!solution.exists())
    {
        emit logOutput("WCS File does not exist.");
        return -1;
    }

    int status = 0;
    char * header { nullptr };
    int nkeyrec, nreject, nwcs;

    fitsfile *fptr { nullptr };

    if (fits_open_diskfile(&fptr, solutionFile.toLatin1(), READONLY, &status))
    {
        char errmsg[512];
        fits_get_errstatus(status, errmsg);
        emit logOutput(QString("Error opening fits file %1, %2").arg(solutionFile).arg(errmsg));
        return status;
    }

    if (fits_hdr2str(fptr, 1, nullptr, 0, &header, &nkeyrec, &status))
    {
        char errmsg[512];
        fits_get_errstatus(status, errmsg);
        emit logOutput(QString("ERROR %1: %2.").arg(status).arg(wcshdr_errmsg[status]));
        return status;
    }

    if ((status = wcspih(header, nkeyrec, WCSHDR_all, -3, &nreject, &nwcs, &m_wcs)) != 0)
    {
        free(header);
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        hasWCS = false;
        emit logOutput(QString("wcspih ERROR %1: %2.").arg(status).arg(wcshdr_errmsg[status]));
        return status;
    }
    fits_close_file(fptr, &status);

#ifndef _WIN32 //For some very strange reason, this causes a crash on Windows??
    free(header);
#endif

    if (m_wcs == nullptr)
    {
        emit logOutput("No world coordinate systems found.");
        hasWCS = false;
        return status;
    }
    else
        hasWCS = true;

    // FIXME: Call above goes through EVEN if no WCS is present, so we're adding this to return for now.
    if (m_wcs->crpix[0] == 0)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        hasWCS = false;
        emit logOutput("No world coordinate systems found.");
        return status;
    }

    if ((status = wcsset(m_wcs)) != 0)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        hasWCS = false;
        emit logOutput(QString("wcsset error %1: %2.").arg(status).arg(wcs_errmsg[status]));
        return status;
    }

    emit logOutput("Finished Loading WCS...");

    return 0;
}

//This was essentially copied from KStars' loadWCS method and split in half with some modifications
void ExternalSextractorSolver::computeWCSCoord()
{
    if(!hasWCS)
    {
        emit logOutput("There is no WCS Data.  Did you solve the image first?");
        return;
    }
    int w  = m_Statistics.width;
    int h = m_Statistics.height;
    wcs_coord = new FITSImage::wcs_point[w * h];
    FITSImage::wcs_point * p = wcs_coord;
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
                emit logOutput(QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]));
            }
            else
            {
                p->ra  = world[0];
                p->dec = world[1];

                p++;
            }
        }
    }
}

bool ExternalSextractorSolver::pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint)
{
    if(!hasWCSData())
    {
        emit logOutput("There is no WCS Data.");
        return false;
    }
    double imgcrd[2], phi, pixcrd[2], theta, world[2];
    int stat[2];
    pixcrd[0] = pixelPoint.x();
    pixcrd[1] = pixelPoint.y();

    int status = wcsp2s(m_wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0]);
    if(status != 0)
    {
        emit logOutput(QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]));
        return false;
    }
    else
    {
        skyPoint.ra = world[0];
        skyPoint.dec = world[1];
    }
    return true;
}

bool ExternalSextractorSolver::wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint)
{
    if(!hasWCSData())
    {
        emit logOutput("There is no WCS Data.");
        return false;
    }
    double imgcrd[2], worldcrd[2], pixcrd[2], phi[2], theta[2];
    int stat[2];
    worldcrd[0] = skyPoint.ra;
    worldcrd[1] = skyPoint.dec;

    int status = wcss2p(m_wcs, 1, 2, &worldcrd[0], &phi[0], &theta[0], &imgcrd[0], &pixcrd[0], &stat[0]);
    if(status != 0)
    {
        emit logOutput(QString("wcss2p error %1: %2.").arg(status).arg(wcs_errmsg[status]));
        return false;
    }
    pixelPoint.setX(pixcrd[0]);
    pixelPoint.setY(pixcrd[1]);
    return true;
}

bool ExternalSextractorSolver::appendStarsRAandDEC()
{
    if(!hasWCS)
    {
        emit logOutput("There is no WCS Data.  Did you solve the image first?");
        return false;
    }

    double imgcrd[2], phi = 0, pixcrd[2], theta = 0, world[2];
    int stat[2];

    for(auto &oneStar : stars)
    {
        int status = 0;
        double ra = HUGE_VAL;
        double dec = HUGE_VAL;
        pixcrd[0] = oneStar.x;
        pixcrd[1] = oneStar.y;

        if ((status = wcsp2s(m_wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0])) != 0)
        {
            emit logOutput(QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]));
            return false;
        }
        else
        {
            ra  = world[0];
            dec = world[1];
        }

        oneStar.ra = ra;
        oneStar.dec = dec;
    }

    return true;
}
