/*  ExternalExtractorSolver, StellarSolver Internal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#include "externalextractorsolver.h"
#include <QTextStream>
#include <QMessageBox>
#include <qmath.h>
#include <wcshdr.h>
#include <wcsfix.h>

//CFitsio Includes
#include <fitsio.h>

// This needs to be static even if there are parallel StellarSolvers so that each solver and child solver gets a unique identifier
static int solverNum = 1;

ExternalExtractorSolver::ExternalExtractorSolver(ProcessType type, ExtractorType exType, SolverType solType,
        FITSImage::Statistic imagestats, uint8_t const *imageBuffer, QObject *parent) : InternalExtractorSolver(type, exType,
                    solType, imagestats, imageBuffer, parent)
{

    // This sets the base name used for the temp files.
    m_BaseName = "externalExtractorSolver_" + QString::number(solverNum++);

    // The code below sets default paths for these key external file settings to their operating system defaults.
    setExternalFilePaths(getDefaultExternalPaths());
}

ExternalExtractorSolver::~ExternalExtractorSolver()
{
    free(xcol);
    free(ycol);
    free(magcol);
    free(colFormat);
    free(colUnits);
    free(magUnits);
}

//The following methods are available to get the default paths for different operating systems and configurations.

ExternalProgramPaths ExternalExtractorSolver::getDefaultExternalPaths(ComputerSystemType system)
{
    ExternalProgramPaths paths;
    switch(system)
    {
    case LINUX_DEFAULT:
        paths.confPath = "/etc/astrometry.cfg";
        paths.sextractorBinaryPath =  "/usr/bin/sextractor";
        paths.solverPath = "/usr/bin/solve-field";
        (QFile("/bin/astap").exists()) ?
            paths.astapBinaryPath = "/bin/astap" :
            paths.astapBinaryPath = "/opt/astap/astap";
        if(QFile("/usr/bin/astap").exists())
            paths.astapBinaryPath = "/usr/bin/astap";
        paths.watneyBinaryPath = "/opt/watney/watney-solve";
        paths.wcsPath = "/usr/bin/wcsinfo";
        break;
    case LINUX_INTERNAL:
        paths.confPath = "$HOME/.local/share/kstars/astrometry/astrometry.cfg";
        paths.sextractorBinaryPath = "/usr/bin/sextractor";
        paths.solverPath = "/usr/bin/solve-field";
        (QFile("/bin/astap").exists()) ?
            paths.astapBinaryPath = "/bin/astap" :
            paths.astapBinaryPath = "/opt/astap/astap";
        paths.watneyBinaryPath = "/opt/watney/watney-solve";
        paths.wcsPath = "/usr/bin/wcsinfo";
        break;
    case MAC_HOMEBREW:
        paths.confPath = "/usr/local/etc/astrometry.cfg";
        paths.sextractorBinaryPath = "/usr/local/bin/sex";
        paths.solverPath = "/usr/local/bin/solve-field";
        paths.astapBinaryPath = "/Applications/ASTAP.app/Contents/MacOS/astap";
        paths.watneyBinaryPath = "/usr/local/bin/watney-solve";
        paths.wcsPath = "/usr/local/bin/wcsinfo";
        break;
    case WIN_ANSVR:
        paths.confPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/etc/astrometry/backend.cfg";
        paths.sextractorBinaryPath = ""; //Not on windows?
        paths.solverPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/lib/astrometry/bin/solve-field.exe";
        paths.astapBinaryPath = "C:/Program Files/astap/astap.exe";
        paths.watneyBinaryPath = "C:/watney/watney-solve.exe";
        paths.wcsPath = QDir::homePath() + "/AppData/Local/cygwin_ansvr/lib/astrometry/bin/wcsinfo.exe";
        break;
    case WIN_CYGWIN:
        paths.confPath = "C:/cygwin64/usr/etc/astrometry.cfg";
        paths.sextractorBinaryPath = ""; //Not on windows?
        paths.solverPath = "C:/cygwin64/bin/solve-field";
        paths.astapBinaryPath = "C:/Program Files/astap/astap.exe";
        paths.watneyBinaryPath = "C:/watney/watney-solve.exe";
        paths.wcsPath = "C:/cygwin64/bin/wcsinfo";
        break;
    }
    return paths;
}
ExternalProgramPaths ExternalExtractorSolver::getDefaultExternalPaths()
{
    #if defined(Q_OS_OSX)
        return getDefaultExternalPaths(MAC_HOMEBREW);
    #elif defined(Q_OS_LINUX)
        return getDefaultExternalPaths(LINUX_DEFAULT);
    #else //Windows
        return getDefaultExternalPaths(WIN_ANSVR);
    #endif
}

int ExternalExtractorSolver::extract()
{
    if(m_ExtractorType == EXTRACTOR_EXTERNAL)
    {
#ifdef _WIN32  //Note that this is just a warning, if the user has SExtractor installed somehow on Windows, they could use it.
        emit logOutput("SExtractor is not easily installed on windows. Please select the Internal SEP and External Solver.");
#endif

        if(!QFileInfo::exists(externalPaths.sextractorBinaryPath))
        {
            emit logOutput("There is no SExtractor binary at " + externalPaths.sextractorBinaryPath + ", Aborting");
            return -1;
        }
    }

    if(starXYLSFilePath == "")
    {
        starXYLSFilePathIsTempFile = true;
        starXYLSFilePath = m_BasePath + "/" + m_BaseName + ".xyls";
    }

    if(m_ProcessType == EXTRACT_WITH_HFR || m_ProcessType == EXTRACT)
        return runExternalExtractor();
    else
    {
        int fail = 0;
        if(m_ExtractorType == EXTRACTOR_INTERNAL)
        {
            fail = runSEPExtractor();
            if(fail != 0)
                return fail;
            return(writeStarExtractorTable());
        }
        else if(m_ExtractorType == EXTRACTOR_EXTERNAL)
            return(runExternalExtractor());
    }
    return -1;
}

void ExternalExtractorSolver::run()
{
    if(m_AstrometryLogLevel != LOG_NONE && m_LogToFile)
    {
        if(m_LogFileName == "")
            m_LogFileName = m_BasePath + "/" + m_BaseName + ".log.txt";
        if(QFile(m_LogFileName).exists())
            QFile(m_LogFileName).remove();
    }

    if(cancelfn == "")
        cancelfn = m_BasePath + "/" + m_BaseName + ".cancel";
    if(solvedfn == "")
        solvedfn = m_BasePath + "/" + m_BaseName + ".solved";
    if(solutionFile == "")
        solutionFile = m_BasePath + "/" + m_BaseName + ".wcs";

    QFile solvedFile(solvedfn);
    solvedFile.setPermissions(solvedFile.permissions() | QFileDevice::WriteOther);
    solvedFile.remove();

    QFile(cancelfn).remove();

    //These are the solvers that use External Astrometry.
    if(m_SolverType == SOLVER_LOCALASTROMETRY)
    {
        if(!QFileInfo::exists(externalPaths.solverPath))
        {
            emit logOutput("There is no astrometry solver at " + externalPaths.solverPath + ", Aborting");
            emit finished(-1);
            return;
        }
#ifdef _WIN32
        if(m_ActiveParameters.inParallel)
        {
            emit logOutput("The external ANSVR solver on windows does not handle the inparallel option well, disabling it for this run.");
            m_ActiveParameters.inParallel = false;
        }
#endif
    }
    else if(m_SolverType == SOLVER_ASTAP)
    {
        if(!QFileInfo::exists(externalPaths.astapBinaryPath))
        {
            emit logOutput("There is no ASTAP solver at " + externalPaths.astapBinaryPath + ", Aborting");
            emit finished(-1);
            return;
        }
    }

    if(starXYLSFilePath == "")
    {
        starXYLSFilePathIsTempFile = true;
        starXYLSFilePath = m_BasePath + "/" + m_BaseName + ".xyls";
    }

    switch(m_ProcessType)
    {
        case EXTRACT:
        case EXTRACT_WITH_HFR:
        {
            int result = extract();
            cleanupTempFiles();
            emit finished(result);
        }
        break;

        case SOLVE:
        {
            if(m_ExtractorType == EXTRACTOR_BUILTIN && m_SolverType == SOLVER_LOCALASTROMETRY)
            {
                int result = runExternalSolver();
                cleanupTempFiles();
                emit finished(result);
            }
            else if(m_ExtractorType == EXTRACTOR_BUILTIN && m_SolverType == SOLVER_ASTAP)
            {
                int result = runExternalASTAPSolver();
                cleanupTempFiles();
                emit finished(result);
            }
            else if(m_ExtractorType == EXTRACTOR_BUILTIN && m_SolverType == SOLVER_WATNEYASTROMETRY)
            {
                int result = runExternalWatneySolver();
                cleanupTempFiles();
                emit finished(result);
            }
            else
            {
                if(!m_HasExtracted)
                {
                    int fail = extract();
                    if(fail != 0)
                    {
                        cleanupTempFiles();
                        emit finished(fail);
                        return;
                    }
                    if(m_ExtractedStars.size() == 0)
                    {
                        cleanupTempFiles();
                        emit logOutput("No stars were found, so the image cannot be solved");
                        emit finished(-1);
                        return;
                    }
                }

                if(m_HasExtracted)
                {
                    if(m_SolverType == SOLVER_ASTAP)
                    {
                        int result = runExternalASTAPSolver();
                        cleanupTempFiles();
                        emit finished(result);
                    }
                    if(m_SolverType == SOLVER_WATNEYASTROMETRY)
                    {
                        int result = runExternalWatneySolver();
                        cleanupTempFiles();
                        emit finished(result);
                    }
                    else
                    {
                        int result = runExternalSolver();
                        cleanupTempFiles();
                        emit finished(result);
                    }
                }
                else
                {
                    cleanupTempFiles();
                    emit finished(-1);
                }
            }

        }
        break;

        default:
            break;
    }
}

//This method generates child solvers with the options of the current solver
ExtractorSolver* ExternalExtractorSolver::spawnChildSolver(int n)
{
    ExternalExtractorSolver *solver = new ExternalExtractorSolver(m_ProcessType, m_ExtractorType, m_SolverType, m_Statistics,
            m_ImageBuffer, nullptr);
    solver->m_ExtractedStars = m_ExtractedStars;
    solver->m_BasePath = m_BasePath;
    solver->m_BaseName = m_BaseName + "_" + QString::number(n);
    solver->m_HasExtracted = true;
    solver->starXYLSFilePath = starXYLSFilePath;
    solver->starXYLSFilePathIsTempFile = starXYLSFilePathIsTempFile;
    solver->fileToProcess = fileToProcess;
    solver->externalPaths = externalPaths;
    solver->cleanupTemporaryFiles = cleanupTemporaryFiles;
    solver->autoGenerateAstroConfig = autoGenerateAstroConfig;
    solver->onlySendFITSFiles = onlySendFITSFiles;

    solver->isChildSolver = true;
    solver->m_ActiveParameters = m_ActiveParameters;
    solver->indexFolderPaths = indexFolderPaths;
    solver->indexFiles = indexFiles;
    //Set the log level one less than the main solver
    if(m_SSLogLevel == LOG_VERBOSE )
        solver->m_SSLogLevel = LOG_NORMAL;
    if(m_SSLogLevel == LOG_NORMAL || m_SSLogLevel == LOG_OFF)
        solver->m_SSLogLevel = LOG_OFF;
    if(m_UseScale)
        solver->setSearchScale(scalelo, scalehi, scaleunit);
    if(m_UsePosition)
        solver->setSearchPositionInDegrees(search_ra, search_dec);
    if(m_AstrometryLogLevel != SSolver::LOG_NONE || m_SSLogLevel != SSolver::LOG_OFF)
        connect(solver, &ExtractorSolver::logOutput, this, &ExtractorSolver::logOutput);
    //This way they all share a solved and cancel fn
    solver->solutionFile = solutionFile;
    //solver->cancelfn = cancelfn;
    //solver->solvedfn = basePath + "/" + baseName + ".solved";
    return solver;
}

//This is the abort method.  For the external SExtractor and solver, it uses the kill method to abort the processes
void ExternalExtractorSolver::abort()
{
    if(solver){
        solver->kill();
        QFile file(cancelfn);
        if(QFileInfo(file).dir().exists())
        {
            file.open(QIODevice::WriteOnly);
            file.write("Cancel");
            file.close();
        }
    }
    if(extractorProcess)
        extractorProcess->kill();
    if(!isChildSolver)
        emit logOutput("Aborting ...");
    quit();
    m_WasAborted = true;
}

void ExternalExtractorSolver::cleanupTempFiles()
{
    if(cleanupTemporaryFiles)
    {
        QDir temp(m_BasePath);
        //SExtractor Files
        temp.remove(m_BaseName + ".param");
        temp.remove(m_BaseName + ".conv");
        temp.remove(m_BaseName + ".cfg");

        //ASTAP files
        temp.remove(m_BaseName + ".ini");

        //Astrometry Files
        temp.remove(m_BaseName + ".corr");
        temp.remove(m_BaseName + ".rdls");
        temp.remove(m_BaseName + ".axy");
        temp.remove(m_BaseName + ".corr");
        temp.remove(m_BaseName + ".new");
        temp.remove(m_BaseName + ".match");
        temp.remove(m_BaseName + "-indx.xyls");
        temp.remove(m_BaseName + ".solved");

        //Other Files
        QFile solvedFile(solvedfn);
        solvedFile.setPermissions(solvedFile.permissions() | QFileDevice::WriteOther);
        solvedFile.remove();
        QFile(solutionFile).remove();
        //QFile(cancelfn).remove();
        if(starXYLSFilePathIsTempFile)
            QFile(starXYLSFilePath).remove();
        if(fileToProcessIsTempFile)
            QFile(fileToProcess).remove();
    }
}

//This method is copied and pasted and modified from the code I wrote to use SExtractor in OfflineAstrometryParser in KStars
//It creates key files needed to run SExtractor from the desired options, then runs the SExtractor program using the options.
int ExternalExtractorSolver::runExternalExtractor()
{
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Configuring external SExtractor");
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
        QString newFileURL = m_BasePath + "/" + m_BaseName + "." + file.suffix();
        QFile::copy(fileToProcess, newFileURL);
        fileToProcess = newFileURL;
        fileToProcessIsTempFile = true;
    }

    //Configuration arguments for SExtractor
    QStringList sextractorArgs;
    //This one is not really that necessary, it will use the defaults if it can't find it
    //We will set all of the things we need in the parameters below
    //sextractorArgs << "-c" << "/usr/local/share/sextractor/default.sex";

    sextractorArgs << "-CATALOG_NAME" << starXYLSFilePath;
    sextractorArgs << "-CATALOG_TYPE" << "FITS_1.0";

    //SExtractor needs a default.param file in the working directory
    //This creates that file with the options we need for astrometry.net and SExtractor

    QString paramPath =  m_BasePath + "/" + m_BaseName + ".param";
    QFile paramFile(paramPath);
    if (paramFile.open(QIODevice::WriteOnly) == false)
    {
        QMessageBox::critical(nullptr, "Message", "SExtractor file write error.");
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


    //SExtractor needs a default.conv file in the working directory
    //This creates the default one

    QString convPath =  m_BasePath + "/" + m_BaseName + ".conv";
    QFile convFile(convPath);
    if (convFile.open(QIODevice::WriteOnly) == false)
    {
        QMessageBox::critical(nullptr, "Message", "SExtractor CONV filter write error.");
        return -1;
    }
    else
    {
        QTextStream out(&convFile);
        out << "CONV Filter Generated by StellarSolver Internal Library\n";
        int c = 0;
        for(int i = 0; i < convFilter.size(); i++)
        {
            out << convFilter.at(i);

            //We want the last one before the sqrt of the size to spark a new line
            if(c < sqrt(convFilter.size()) - 1)
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
    sextractorArgs << "-DETECT_MINAREA" << QString::number(m_ActiveParameters.minarea);

    //sextractorArgs << "-DETECT_THRESH" << QString::number();
    //sextractorArgs << "-ANALYSIS_THRESH" << QString::number(minarea);

    sextractorArgs << "-FILTER" << "Y";
    sextractorArgs << "-FILTER_NAME" << convPath;

    sextractorArgs << "-DEBLEND_NTHRESH" << QString::number(m_ActiveParameters.deblend_thresh);
    sextractorArgs << "-DEBLEND_MINCONT" << QString::number(m_ActiveParameters.deblend_contrast);

    sextractorArgs << "-CLEAN" << ((m_ActiveParameters.clean == 1) ? "Y" : "N");
    sextractorArgs << "-CLEAN_PARAM" << QString::number(m_ActiveParameters.clean_param);

    //------------------------------ Photometry -----------------------------------
    sextractorArgs << "-PHOT_AUTOPARAMS" << QString::number(m_ActiveParameters.kron_fact) + "," + QString::number(
                       m_ActiveParameters.r_min);
    sextractorArgs << "-MAG_ZEROPOINT" << QString::number(m_ActiveParameters.magzero);

    sextractorArgs <<  fileToProcess;

    extractorProcess.clear();
    extractorProcess = new QProcess();

    extractorProcess->setWorkingDirectory(m_BasePath);
    extractorProcess->setProcessChannelMode(QProcess::MergedChannels);
    if(m_SSLogLevel != LOG_OFF)
        connect(extractorProcess, &QProcess::readyReadStandardOutput, this, &ExternalExtractorSolver::logSextractor);

    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting external SExtractor with the " + m_ActiveParameters.listName + " profile...");
    emit logOutput(externalPaths.sextractorBinaryPath + " " + sextractorArgs.join(' '));

    extractorProcess->start(externalPaths.sextractorBinaryPath, sextractorArgs);
    extractorProcess->waitForFinished(30000); //Will timeout after 30 seconds
    emit logOutput(extractorProcess->readAllStandardError().trimmed());

    if(extractorProcess->exitCode() != 0 || extractorProcess->exitStatus() == QProcess::CrashExit)
        return extractorProcess->exitCode();

    int exitCode = getStarsFromXYLSFile();
    if(exitCode != 0)
        return exitCode;

    if(m_UseSubframe)
    {
        for(int i = 0; i < m_ExtractedStars.size(); i++)
        {
            FITSImage::Star star = m_ExtractedStars.at(i);
            if(!m_SubFrameRect.contains(star.x, star.y))
            {
                m_ExtractedStars.removeAt(i);
                i--;
            }
        }
    }

    applyStarFilters(m_ExtractedStars);

    m_HasExtracted = true;

    return 0;

}

//The code for this method is copied and pasted and modified from OfflineAstrometryParser in KStars
//It runs the astrometry.net external program using the options selected.
int ExternalExtractorSolver::runExternalSolver()
{
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    if(m_ExtractorType == EXTRACTOR_BUILTIN)
        emit logOutput("Configuring external Astrometry.net solver classically: using python and without SExtractor first");
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

        if( !isChildSolver && onlySendFITSFiles && file.suffix() != "fits" && file.suffix() != "fit")
        {
            int ret = saveAsFITS();
            if(ret != 0)
            {
                emit logOutput("Failed to Save the image as a FITS File.");
                return ret;
            }
        }
        else
        {
            //Making a copy of the file and putting it in the temp directory
            //So that we can find all the temporary files and delete them later
            //That way we don't pollute the directory the original image is located in
            QString newFileURL = m_BasePath + "/" + m_BaseName + "." + file.suffix();
            QFile::copy(fileToProcess, newFileURL);
            fileToProcess = newFileURL;
            fileToProcessIsTempFile = true;
        }
    }
    else
    {
        QFileInfo sextractorFile(starXYLSFilePath);
        if(!sextractorFile.exists())
        {
            emit logOutput("Please Star Extract the image first");
        }
        if(isChildSolver)
        {
            QString newFileURL = m_BasePath + "/" + m_BaseName + "." + sextractorFile.suffix();
            QFile::copy(starXYLSFilePath, newFileURL);
            starXYLSFilePath = newFileURL;
            starXYLSFilePathIsTempFile = true;
        }
    }

    QStringList solverArgs = getSolverArgsList();

    if(m_ExtractorType == EXTRACTOR_BUILTIN)
    {
        solverArgs << "--keep-xylist" << starXYLSFilePath;
        solverArgs << fileToProcess;
    }
    else
        solverArgs << starXYLSFilePath;

    solver.clear();
    solver = new QProcess();

    solver->setProcessChannelMode(QProcess::MergedChannels);
    if(m_AstrometryLogLevel != LOG_NONE)
        connect(solver, &QProcess::readyReadStandardOutput, this, &ExternalExtractorSolver::logSolver);

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

    solver->start(externalPaths.solverPath, solverArgs);
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting external Astrometry.net solver with the " + m_ActiveParameters.listName + " profile...");
    emit logOutput("Command: " + externalPaths.solverPath + " " + solverArgs.join(" "));

    solver->waitForFinished(m_ActiveParameters.solverTimeLimit * 1000 *
                            1.2); //Set to timeout in a little longer than the timeout
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
    if(m_WasAborted)
        return -1;
    if(!getSolutionInformation())
        return -1;
    loadWCS(); //Attempt to Load WCS, but don't totally fail if you don't find it.
    m_HasSolved = true;
    return 0;
}


int ExternalExtractorSolver::runExternalASTAPSolver()
{
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Configuring external ASTAP solver");

    if(m_ExtractorType != EXTRACTOR_BUILTIN)
    {
        QFileInfo file(fileToProcess);
        if(!file.exists())
            return -1;

        QString newFileURL = m_BasePath + "/" + m_BaseName + "." + file.suffix();
        QFile::copy(fileToProcess, newFileURL);
        fileToProcess = newFileURL;
        fileToProcessIsTempFile = true;
    }

    QStringList solverArgs;

    QString astapSolutionFile = m_BasePath + "/" + m_BaseName + ".ini";
    solverArgs << "-o" << astapSolutionFile;
    solverArgs << "-speed" << "auto";
    solverArgs << "-f" << fileToProcess;
    solverArgs << "-wcs";
    if(m_ActiveParameters.downsample > 1)
        solverArgs << "-z" << QString::number(m_ActiveParameters.downsample);
    else
        solverArgs << "-z" << "0";
    if(m_UseScale)
    {
        double scale = scalelo;
        if(scaleunit == ARCSEC_PER_PIX)
            scale = (scalehi + scalelo) / 2;
        double degreesFOV = convertToDegreeHeight(scale);
        solverArgs << "-fov" << QString::number(degreesFOV);
    }
    if(m_UsePosition)
    {
        solverArgs << "-ra" << QString::number(search_ra / 15.0); //Convert ra to hours
        solverArgs << "-spd" << QString::number(search_dec + 90); //Convert dec to spd
        solverArgs << "-r" << QString::number(m_ActiveParameters.search_radius);
    }
    if(m_AstrometryLogLevel != LOG_NONE)
        solverArgs << "-log";

    solver.clear();
    solver = new QProcess();

    solver->setProcessChannelMode(QProcess::MergedChannels);
    if(m_AstrometryLogLevel != LOG_NONE)
        connect(solver, &QProcess::readyReadStandardOutput, this, &ExternalExtractorSolver::logSolver);

    solver->start(externalPaths.astapBinaryPath, solverArgs);

    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting external ASTAP Solver with the " + m_ActiveParameters.listName + " profile...");
    emit logOutput("Command: " + externalPaths.astapBinaryPath + " " + solverArgs.join(" "));

    solver->waitForFinished(m_ActiveParameters.solverTimeLimit * 1000 *
                            1.2); //Set to timeout in a little longer than the timeout

    if(m_AstrometryLogLevel != LOG_NONE)
    {
        QFile logFile(m_BasePath + "/" + m_BaseName + ".log");
        if(logFile.exists())
        {
            if(m_LogToFile)
            {
                if(m_LogFileName != "")
                    logFile.copy(m_LogFileName);
            }
            else
            {
                if (logFile.open(QIODevice::ReadOnly))
                {
                    QTextStream in(&logFile);
                    emit logOutput(in.readAll());
                }
                else
                    emit logOutput("Failed to open ASTAP log file" + logFile.fileName());
            }
        }
        else
            emit logOutput("ASTAP log file " + logFile.fileName() + " does not exist.");
    }

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
    m_HasSolved = true;
    return 0;
}

int ExternalExtractorSolver::runExternalWatneySolver()
{
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Configuring external Watney Astrometry solver");

    if(m_ExtractorType == EXTRACTOR_BUILTIN)
    {
        QFileInfo file(fileToProcess);
        if(!file.exists())
            return -1;

        QString newFileURL = m_BasePath + "/" + m_BaseName + "." + file.suffix();
        QFile::copy(fileToProcess, newFileURL);
        fileToProcess = newFileURL;
        fileToProcessIsTempFile = true;
    }
    else
    {
        QFileInfo sextractorFile(starXYLSFilePath);
        if(!sextractorFile.exists())
        {
            emit logOutput("Please Star Extract the image first");
        }
        if(isChildSolver)
        {
            QString newFileURL = m_BasePath + "/" + m_BaseName + "." + sextractorFile.suffix();
            QFile::copy(starXYLSFilePath, newFileURL);
            starXYLSFilePath = newFileURL;
            starXYLSFilePathIsTempFile = true;
        }
    }

    QStringList solverArgs;

    QString watneySolutionFile = m_BasePath + "/" + m_BaseName + ".ini";
    if(m_UsePosition)
    {
        solverArgs << "nearby";
        solverArgs << "-r" << QString::number(search_ra);
        solverArgs << "-d" << QString::number(search_dec);
        solverArgs << "-s" << QString::number(m_ActiveParameters.search_radius);
        solverArgs << "-m"; //manual, don't look in fits headers for position info
        if(m_UseScale) // Note: Watney can't use scale in a blind solve, so it is here
        {
            double scale = scalelo;
            if(scaleunit == ARCSEC_PER_PIX)
                scale = (scalehi + scalelo) / 2;
            double degreesFOV = convertToDegreeHeight(scale);
            solverArgs << "--field-radius" << QString::number(degreesFOV);
        }
    }
    else
    {
        solverArgs << "blind";
        solverArgs << "--min-radius" << QString::number(0.5);
        solverArgs << "--max-radius" << QString::number(m_ActiveParameters.search_radius);
    }

    if(m_ExtractorType != EXTRACTOR_BUILTIN)
    {
        solverArgs << "--xyls" << starXYLSFilePath;
        solverArgs << "--xyls-imagesize" << QString::number(m_Statistics.width) + "x" + QString::number(m_Statistics.height);
    }
    else
    {
        solverArgs << "-i" << fileToProcess;
        solverArgs << "--max-stars" << QString::number(m_ActiveParameters.keepNum);
    }
    if(m_ActiveParameters.multiAlgorithm == SSolver::NOT_MULTI)
        solverArgs << "--use-parallelism false";
    else
        solverArgs << "--use-parallelism true";
    solverArgs << "-o" << watneySolutionFile;
    solverArgs << "-w" << solutionFile;
    /*
    if(m_ActiveParameters.downsample > 1)
        solverArgs << "-z" << QString::number(m_ActiveParameters.downsample);
    else
        solverArgs << "-z" << "0";
    */

    if(m_AstrometryLogLevel != LOG_NONE){
        if(m_LogFileName == "")
            m_LogFileName = m_BasePath + "/" + m_BaseName + ".log";
        if(QFile(m_LogFileName).exists())
            QFile(m_LogFileName).remove();
        solverArgs << "--log-file" << m_LogFileName;
    }

    solver.clear();
    solver = new QProcess();

    solver->setProcessChannelMode(QProcess::MergedChannels);
    if(m_AstrometryLogLevel != LOG_NONE)
        connect(solver, &QProcess::readyReadStandardOutput, this, &ExternalExtractorSolver::logSolver);

    solver->start(externalPaths.watneyBinaryPath, solverArgs);

    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting external Watney Solver with the " + m_ActiveParameters.listName + " profile...");
    emit logOutput("Command: " + externalPaths.watneyBinaryPath + " " + solverArgs.join(" "));

    solver->waitForFinished(m_ActiveParameters.solverTimeLimit * 1000 *
                            1.2); //Set to timeout in a little longer than the timeout

    if(m_AstrometryLogLevel != LOG_NONE)
    {
        QFile logFile(m_BasePath + "/" + m_BaseName + ".log");
        if(logFile.exists())
        {
            if(m_LogToFile)
            {
                if(m_LogFileName != "")
                    logFile.copy(m_LogFileName);
            }
            else
            {
                if (logFile.open(QIODevice::ReadOnly))
                {
                    QTextStream in(&logFile);
                    emit logOutput(in.readAll());
                }
                else
                    emit logOutput("Failed to open Watney log file" + logFile.fileName());
            }
        }
        else
            emit logOutput("Watney log file " + logFile.fileName() + " does not exist.");
    }

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
    if(!getWatneySolutionInformation())
        return -1;
    loadWCS(); //Attempt to Load WCS, but don't totally fail if you don't find it.
    m_HasSolved = true;
    return 0;
}

//This method is copied and pasted and modified from getSolverOptionsFromFITS in Align in KStars
//Then it was split in two parts (The other part is located in the MainWindow class)
//This part generates the argument list from the options for the external solver only
QStringList ExternalExtractorSolver::getSolverArgsList()
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
    if(m_ActiveParameters.resort)
        solverArgs << "--resort";

    if(depthhi != -1 && depthlo != -1)
        solverArgs << "--depth" << QString("%1-%2").arg(depthlo).arg(depthhi);

    if(m_ActiveParameters.keepNum != 0)
        solverArgs << "--objs" << QString("%1").arg(m_ActiveParameters.keepNum);

    //This will shrink the image so that it is easier to solve.  It is only useful if you are sending an image.
    //It is not used if you are solving an xylist as in the classic astrometry.net solver
    if(m_ActiveParameters.downsample > 1 && m_ExtractorType == EXTRACTOR_BUILTIN)
        solverArgs << "--downsample" << QString::number(m_ActiveParameters.downsample);

    //I am not sure if we want to provide these options or not.  They do make a huge difference in solving time
    //But changing them can be dangerous because it can cause false positive solves.
    solverArgs << "--odds-to-solve" << QString::number(exp(m_ActiveParameters.logratio_tosolve));
    solverArgs << "--odds-to-tune-up" << QString::number(exp(m_ActiveParameters.logratio_totune));
    //solverArgs << "--odds-to-keep" << QString::number(logratio_tokeep);  I'm not sure if this is one we need.

    if (m_UseScale)
        solverArgs << "-L" << QString::number(scalelo) << "-H" << QString::number(scalehi) << "-u" << getScaleUnitString();

    if (m_UsePosition)
        solverArgs << "-3" << QString::number(search_ra) << "-4" << QString::number(search_dec) << "-5" << QString::number(
                       m_ActiveParameters.search_radius);

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
        if(m_ActiveParameters.resort)
        {
            solverArgs << "--sort-column" << "MAG_AUTO";
            solverArgs << "--sort-ascending";
        }
    }

    //Note This set of items is NOT NEEDED for SExtractor or for astrometry.net to solve, it is needed to avoid python usage.
    //On many user's systems especially on Mac and sometimes on Windows, there is an issue in the Python setup that causes astrometry to fail.
    //This should avoid those problems as long as you send a FITS file or a xy list to astrometry.
    solverArgs << "--no-remove-lines";
    solverArgs << "--uniformize" << "0";
    if(onlySendFITSFiles && m_ExtractorType == EXTRACTOR_BUILTIN)
        solverArgs << "--fits-image";

    //Don't need any argument for default level
    if(m_AstrometryLogLevel == LOG_MSG || m_AstrometryLogLevel == LOG_ERROR)
        solverArgs << "-v";
    else if(m_AstrometryLogLevel == LOG_VERB || m_AstrometryLogLevel == LOG_ALL)
        solverArgs << "-vv";

    if(autoGenerateAstroConfig || !QFile(externalPaths.confPath).exists())
        generateAstrometryConfigFile();

    //This sends the path to the config file.  Note that backend-config seems to be more universally recognized across
    //the different solvers than config
    solverArgs << "--backend-config" << externalPaths.confPath;

    //This sets the cancel filename for astrometry.net.  Astrometry will monitor for the creation of this file
    //In order to shut down and stop processing
    solverArgs << "--cancel" << cancelfn;

    //This sets the wcs file for astrometry.net.  This file will be very important for reading in WCS info later on
    solverArgs << "-W" << solutionFile;

    return solverArgs;
}

//This will generate a temporary Astrometry.cfg file to use for solving so that we have more control over these options
//for the external solvers from inside the program.
bool ExternalExtractorSolver::generateAstrometryConfigFile()
{
    externalPaths.confPath =  m_BasePath + "/" + m_BaseName + ".cfg";
    QFile configFile(externalPaths.confPath);
    if (configFile.open(QIODevice::WriteOnly) == false)
    {
        QMessageBox::critical(nullptr, "Message", "Config file write error.");
        return false;
    }
    else
    {
        QTextStream out(&configFile);
        if(m_ActiveParameters.inParallel)
            out << "inparallel\n";
        out << "minwidth " << m_ActiveParameters.minwidth << "\n";
        out << "maxwidth " << m_ActiveParameters.maxwidth << "\n";
        out << "cpulimit " << m_ActiveParameters.solverTimeLimit << "\n";
        if(indexFolderPaths.count() > 0)
            out << "autoindex\n";
        foreach(QString folder, indexFolderPaths)
        {
            out << "add_path " << folder << "\n";
        }
        foreach(QString file, indexFiles)
        {
            out << "index " << file << "\n";
        }
        configFile.close();
    }
    return true;
}


//These methods are for the logging of information to the textfield at the bottom of the window.

void ExternalExtractorSolver::logSextractor()
{
    if(extractorProcess->canReadLine())
    {
        QString rawText(extractorProcess->readLine().trimmed());
        QString cleanedString = rawText.remove("[1M>").remove("[1A");
        if(!cleanedString.isEmpty())
        {
            emit logOutput(cleanedString);
            if(m_LogToFile)
            {
                QFile file(m_LogFileName);
                if (file.open(QIODevice::Append | QIODevice::Text))
                {
                    QTextStream outstream(&file);
                    #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        outstream << cleanedString << Qt::endl;
                    #else
                        outstream << cleanedString << endl;
                    #endif
                    file.close();
                }
                else
                    emit logOutput(("Log File Write Error"));
            }
        }
    }
}

void ExternalExtractorSolver::logSolver()
{
    if(solver->canReadLine())
    {
        QString solverLine(solver->readLine().trimmed());
        if(!solverLine.isEmpty())
        {
            emit logOutput(solverLine);
            if(m_LogToFile)
            {
                QFile file(m_LogFileName);
                if (file.open(QIODevice::Append | QIODevice::Text))
                {
                    QTextStream outstream(&file);
                    #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                        outstream << solverLine << Qt::endl;
                    #else
                        outstream << solverLine << endl;
                    #endif
                    file.close();
                }
                else
                    emit logOutput(("Log File Write Error"));
            }
        }
    }
}

//This method is copied and pasted and modified from tablist.c in astrometry.net
//This is needed to load in the stars sextracted by an external SExtractor to get them into the table
int ExternalExtractorSolver::getStarsFromXYLSFile()
{
    QFile sextractorFile(starXYLSFilePath);
    if(!sextractorFile.exists())
    {
        emit logOutput("Can't get SExtractor file since it doesn't exist.");
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

    if (fits_open_diskfile(&new_fptr, starXYLSFilePath.toLocal8Bit(), READONLY, &status))
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

    m_ExtractedStars.clear();

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

        FITSImage::Star star = {starx, stary, mag, flux, peak, HFR, a, b, theta, 0, 0, (int)(a*b*3.14)};

        m_ExtractedStars.append(star);
    }
    fits_close_file(new_fptr, &status);

    if (status) fits_report_error(stderr, status); /* print any error message */

    return 0;
}

//This method was based on a method in KStars.
//It reads the information from the Solution file from Astrometry.net and puts it into the solution
bool ExternalExtractorSolver::getSolutionInformation()
{
    if(solutionFile == "")
        solutionFile = m_BasePath + "/" + m_BaseName + ".wcs";
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

    wcsProcess.start(externalPaths.wcsPath, QStringList(solutionFile));
    wcsProcess.waitForFinished(30000); //Will timeout after 30 seconds
    QString wcsinfo_stdout = wcsProcess.readAllStandardOutput();

    //This is a quick way to find out what keys are available
    // emit logOutput(wcsinfo_stdout);

    QStringList wcskeys = wcsinfo_stdout.split(QRegExp("[\n]"));

    QStringList key_value;

    double ra = 0, dec = 0, orient = 0;
    double fieldw = 0, fieldh = 0, pixscale = 0;
    QString fieldunits;
    QString rastr, decstr;
    FITSImage::Parity parity = FITSImage::NEGATIVE;

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
            else if (key_value[0] == "fieldunits")
                fieldunits = key_value[1];
            else if (key_value[0] == "ra_center_hms")
                rastr = key_value[1];
            else if (key_value[0] == "dec_center_dms")
                decstr = key_value[1];
            else if (key_value[0] == "pixscale")
                pixscale = key_value[1].toDouble();
            else if (key_value[0] == "parity")
                parity = (key_value[1].toInt() == -1) ? FITSImage::POSITIVE : FITSImage::NEGATIVE;
        }
    }

    if(usingDownsampledImage)
        pixscale /= m_ActiveParameters.downsample;

    double raErr = 0;
    double decErr = 0;
    if(m_UsePosition)
    {
        raErr = (search_ra - ra) * 3600;
        decErr = (search_dec - dec) * 3600;
    }

    if(fieldunits == "degrees")
    {
        fieldw *= 60;
        fieldh *= 60;
    }
    if(fieldunits == "arcseconds")
    {
        fieldw /= 60;
        fieldh /= 60;
    }

    m_Solution = {fieldw, fieldh, ra, dec, orient, pixscale, parity, raErr, decErr};

    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
    emit logOutput(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr, decstr));
    if(m_UsePosition)
        emit logOutput(QString("Field is: (%1, %2) deg from search coords.").arg( raErr).arg(decErr));
    emit logOutput(QString("Field size: %1 x %2 arcminutes").arg( fieldw).arg(fieldh));
    emit logOutput(QString("Pixel Scale: %1\"").arg( pixscale ));
    emit logOutput(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
    emit logOutput(QString("Field parity: %1\n").arg(FITSImage::getParityText(parity).toUtf8().data()));

    return true;
}

//This method was based on a method in KStars.
//It reads the information from the Solution file from Astrometry.net and puts it into the solution
bool ExternalExtractorSolver::getASTAPSolutionInformation()
{
    QFile results(m_BasePath + "/" + m_BaseName + ".ini");

    if (!results.open(QIODevice::ReadOnly))
    {
        emit logOutput("Failed to open solution file" + m_BasePath + "/" + m_BaseName + ".ini");
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
        line = in.readLine();
        //If the plate solve failed, we still need to search for any error or warning messages and print them out.
        while (!line.isNull())
        {
            QStringList ini = line.split("=");
            if (ini[0] == "WARNING")
                emit logOutput(line.mid(8).trimmed());
            else if (ini[0] == "ERROR")
                emit logOutput(line.mid(6).trimmed());

            line = in.readLine();
        }
        emit logOutput("Solver failed. Try again.");
        return false;
    }
    double ra = 0, dec = 0, orient = 0;
    double fieldw = 0, fieldh = 0, pixscale = 0;
    char rastr[32], decstr[32];
    FITSImage::Parity parity = FITSImage::NEGATIVE;
    double cd11 = 0;
    double cd22 = 0;
    double cd12 = 0;
    double cd21 = 0;
    bool ok[8] = {false};

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
        else if (ini[0] == "CD1_1")
            cd11 = ini[1].trimmed().toDouble(&ok[4]);
        else if (ini[0] == "CD1_2")
            cd12 = ini[1].trimmed().toDouble(&ok[5]);
        else if (ini[0] == "CD2_1")
            cd21 = ini[1].trimmed().toDouble(&ok[6]);
        else if (ini[0] == "CD2_2")
            cd22 = ini[1].trimmed().toDouble(&ok[7]);
        else if (ini[0] == "WARNING")
            emit logOutput(line.mid(8).trimmed());
        else if (ini[0] == "ERROR")
            emit logOutput(line.mid(6).trimmed());

        line = in.readLine();
    }

    if ( ok[0] && ok[1] && ok[2] && ok[3] )
    {
        ra2hmsstring(ra, rastr);
        dec2dmsstring(dec, decstr);
        fieldw = m_Statistics.width * pixscale / 60;
        fieldh = m_Statistics.height * pixscale / 60;

        if ( ok[4] && ok[5] && ok[6] && ok[7] )
        {
            // Note, negative determinant = positive parity.
            double det = cd11 * cd22 - cd12 * cd21;
            if(det > 0)
                parity = FITSImage::NEGATIVE;
            else
                parity = FITSImage::POSITIVE;
        }

        double raErr = 0;
        double decErr = 0;
        if(m_UsePosition)
        {
            raErr = (search_ra - ra) * 3600;
            decErr = (search_dec - dec) * 3600;
        }

        if(orient < 0)
            orient = 360 + orient;

        m_Solution = {fieldw, fieldh, ra, dec, orient, pixscale, parity, raErr, decErr};
        emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logOutput(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg(dec));
        emit logOutput(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr, decstr));
        if(m_UsePosition)
            emit logOutput(QString("Field is: (%1, %2) deg from search coords.").arg( raErr).arg(decErr));
        emit logOutput(QString("Field size: %1 x %2 arcminutes").arg(fieldw).arg(fieldh));
        emit logOutput(QString("Pixel Scale: %1\"").arg( pixscale ));
        emit logOutput(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
        emit logOutput(QString("Field parity: %1\n").arg(FITSImage::getParityText(parity).toUtf8().data()));

        return true;
    }
    else
    {
        emit logOutput("Solver failed. Try again.");
        return false;
    }
}

//This method was based on a method in KStars.
//It reads the information from the Solution file from Astrometry.net and puts it into the solution
bool ExternalExtractorSolver::getWatneySolutionInformation()
{
    QFile results(m_BasePath + "/" + m_BaseName + ".ini");

    if (!results.open(QIODevice::ReadOnly))
    {
        emit logOutput("Failed to open solution file" + m_BasePath + "/" + m_BaseName + ".ini");
        return false;
    }

    QString doc = results.readAll();
    results.close();
    QJsonParseError qjsonError;
    QJsonDocument jdoc = QJsonDocument::fromJson(doc.toUtf8(), &qjsonError);
    if (qjsonError.error != QJsonParseError::NoError)
    {
        emit logOutput("QJson parsing error!");
        return false;
    }

    QJsonObject jsonObj = jdoc.object();

    if (!jsonObj.contains("success") || !jsonObj["success"].toBool())
    {
        emit logOutput("Solving Failure!");
        return false;
    }

    if(!jsonObj.contains("ra") || !jsonObj.contains("dec") || !jsonObj.contains("orientation")
            || !jsonObj.contains("pixScale") || !jsonObj.contains("parity"))
    {
        emit logOutput("Output file not in expected format!");
        return false;
    }

    double ra = 0, dec = 0, orient = 0;
    double fieldw = 0, fieldh = 0, pixscale = 0;
    char rastr[32], decstr[32];
    FITSImage::Parity parity = FITSImage::NEGATIVE;

    if (jsonObj.contains("ra"))
        ra = jsonObj["ra"].toDouble();
    if (jsonObj.contains("dec"))
        dec = jsonObj["dec"].toDouble();
    if (jsonObj.contains("orientation"))
        orient = jsonObj["orientation"].toDouble();
    if (jsonObj.contains("pixScale"))
        pixscale = jsonObj["pixScale"].toDouble();
    if (jsonObj.contains("parity"))
        parity = jsonObj["parity"].toString() == "flipped" ? FITSImage::NEGATIVE : FITSImage::POSITIVE;

    ra2hmsstring(ra, rastr);
    dec2dmsstring(dec, decstr);
    fieldw = m_Statistics.width * pixscale / 60;
    fieldh = m_Statistics.height * pixscale / 60;

    double raErr = 0;
    double decErr = 0;
    if(m_UsePosition)
    {
        raErr = (search_ra - ra) * 3600;
        decErr = (search_dec - dec) * 3600;
    }

    m_Solution = {fieldw, fieldh, ra, dec, orient, pixscale, parity, raErr, decErr};
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
    emit logOutput(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr, decstr));
    if(m_UsePosition)
        emit logOutput(QString("Field is: (%1, %2) deg from search coords.").arg( raErr).arg(decErr));
    emit logOutput(QString("Field size: %1 x %2 arcminutes").arg( fieldw).arg(fieldh));
    emit logOutput(QString("Pixel Scale: %1\"").arg( pixscale ));
    emit logOutput(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
    emit logOutput(QString("Field parity: %1\n").arg(FITSImage::getParityText(parity).toUtf8().data()));

    return true;

}


//This method writes the table to the file
//I had to create it from the examples on NASA's website
//When I first made this program, I needed it to generate an xyls file from the internal star extraction
//Now it is just used on Windows for the external solving because it needs to use the internal star extractor and the external solver.
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/quick/node10.html
//https://heasarc.gsfc.nasa.gov/docs/software/fitsio/cookbook/node16.html
int ExternalExtractorSolver::writeStarExtractorTable()
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
            QString newFileURL = m_BasePath + "/" + m_BaseName + ".fits";
            QFile::copy(fileToProcess, newFileURL);
            fileToProcess = newFileURL;
            fileToProcessIsTempFile = true;
        }

        if (fits_open_diskfile(&new_fptr, fileToProcess.toLocal8Bit(), READWRITE, &status))
        {
            fits_report_error(stderr, status);
            return status;
        }
    }
    else
    {
        if(starXYLSFilePath == "")
        {
            starXYLSFilePathIsTempFile = true;
            starXYLSFilePath = m_BasePath + "/" + m_BaseName + ".xyls";
        }

        QFile sextractorFile(starXYLSFilePath);
        if(sextractorFile.exists())
            sextractorFile.remove();

        if (fits_create_file(&new_fptr, starXYLSFilePath.toLocal8Bit(), &status))
        {
            fits_report_error(stderr, status);
            return status;
        }
    }

    int tfields = 3;
    int nrows = m_ExtractedStars.size();

    //Columns: X_IMAGE, double, pixels, Y_IMAGE, double, pixels, MAG_AUTO, double, mag
    char* ttype[] = { xcol, ycol, magcol };
    char* tform[] = { colFormat, colFormat, colFormat };
    char* tunit[] = { colUnits, colUnits, magUnits };
    const char* extfile = "SExtractor_File";

    float *xArray = new float[m_ExtractedStars.size()];
    float *yArray = new float[m_ExtractedStars.size()];
    float *magArray = new float[m_ExtractedStars.size()];

    for (int i = 0; i < m_ExtractedStars.size(); i++)
    {
        xArray[i] = m_ExtractedStars.at(i).x;
        yArray[i] = m_ExtractedStars.at(i).y;
        magArray[i] = m_ExtractedStars.at(i).mag;
    }
    
    int firstrow  = 1;  /* first row in table to write   */
    int firstelem = 1;
    int column = 1;

    if(fits_create_tbl(new_fptr, BINARY_TBL, nrows, tfields,
                       ttype, tform, tunit, extfile, &status))
    {
        emit logOutput(QString("Could not create binary table."));
        goto exit;
    }

    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, xArray, &status))
    {
        emit logOutput(QString("Could not write x pixels in binary table."));
        goto exit;
    }

    column = 2;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, yArray, &status))
    {
        emit logOutput(QString("Could not write y pixels in binary table."));
        goto exit;
    }

    column = 3;
    if(fits_write_col(new_fptr, TFLOAT, column, firstrow, firstelem, nrows, magArray, &status))
    {
        emit logOutput(QString("Could not write magnitudes in binary table."));
        goto exit;
    }

    if(fits_close_file(new_fptr, &status))
    {
        emit logOutput(QString("Error closing file."));
        goto exit;
    }
    status = 0;

    exit:
        delete[] xArray;
        delete[] yArray;
        delete[] magArray;

        return status;
}

//This is very necessary for solving non-fits images with the external Star Extractor
//This was copied and pasted and modified from ImageToFITS in fitsdata in KStars
int ExternalExtractorSolver::saveAsFITS()
{
    QString newFilename = m_BasePath + "/" + m_BaseName + ".fits";

    int status = 0;
    fitsfile * new_fptr;

    //I am hoping that this is correct.
    //I"m trying to set these two variables based on the ndim variable since this class doesn't have access to these variables.
    long naxis;
    int channels;
    if (m_Statistics.ndim < 3)
    {
        channels = 1;
        naxis = 2;
    }
    else
    {
        channels = 3;
        naxis = 3;
    }

    long nelements, exposure;
    long naxes[3] = { m_Statistics.width, m_Statistics.height, channels };
    char error_status[512] = {0};

    QFileInfo newFileInfo(newFilename);
    if(newFileInfo.exists())
        QFile(newFilename).remove();

    nelements = m_Statistics.samples_per_channel * channels;

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, newFilename.toLocal8Bit(), &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    int bitpix;
    switch(m_Statistics.dataType)
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
        return status;
    }

    /* Write Data */
    if (fits_write_img(fptr, m_Statistics.dataType, 1, nelements, const_cast<void *>(reinterpret_cast<const void *>(m_ImageBuffer)), &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

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
int ExternalExtractorSolver::loadWCS()
{
    if(solutionFile == "")
        solutionFile = m_BasePath + "/" + m_BaseName + ".wcs";

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

    if (fits_open_diskfile(&fptr, solutionFile.toLocal8Bit(), READONLY, &status))
    {
        char errmsg[512];
        fits_get_errstatus(status, errmsg);
        emit logOutput(QString("Error opening fits file %1, %2").arg(solutionFile, errmsg));
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
        m_HasWCS = false;
        emit logOutput(QString("wcspih ERROR %1: %2.").arg(status).arg(wcshdr_errmsg[status]));
        return status;
    }
    fits_close_file(fptr, &status);

    QFile solFile(solutionFile);
    if (!solFile.open(QIODevice::ReadOnly))
        emit logOutput(("File Read Error"));
    else
    {
        QString searchString("COMMENT index id: ");
        QTextStream in (&solFile);
        QString text = in.readAll();
        if(text.contains(searchString))
        {
            int before = text.indexOf(searchString);
            QString cut = text.mid(before).remove(searchString);
            int after = cut.indexOf(" ");
            solutionIndexNumber = cut.left(after).toShort();
        }
        searchString = "COMMENT index healpix: ";
        if(text.contains(searchString))
        {
            int before = text.indexOf(searchString);
            QString cut = text.mid(before).remove(searchString);
            int after = cut.indexOf(" ");
            solutionHealpix = cut.left(after).toShort();
        }
        solFile.close();
    }

#ifndef _WIN32 //For some very strange reason, this causes a crash on Windows??
    free(header);
#endif

    if (m_wcs == nullptr)
    {
        emit logOutput("No world coordinate systems found.");
        m_HasWCS = false;
        return status;
    }
    else
        m_HasWCS = true;

    // FIXME: Call above goes through EVEN if no WCS is present, so we're adding this to return for now.
    if (m_wcs->crpix[0] == 0)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        m_HasWCS = false;
        emit logOutput("No world coordinate systems found.");
        return status;
    }

    if ((status = wcsset(m_wcs)) != 0)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        m_HasWCS = false;
        emit logOutput(QString("wcsset error %1: %2.").arg(status).arg(wcs_errmsg[status]));
        return status;
    }

    emit logOutput("Finished Loading WCS...");

    return 0;
}

bool ExternalExtractorSolver::pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint)
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

bool ExternalExtractorSolver::wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint)
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

bool ExternalExtractorSolver::appendStarsRAandDEC(QList<FITSImage::Star> &stars)
{
    if(!m_HasWCS)
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
