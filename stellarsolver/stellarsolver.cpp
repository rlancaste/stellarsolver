/*  StellarSolver, StellarSolver Internal Library developed by Robert Lancaster, 2020

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

#include "stellarsolver.h"
#include "sextractorsolver.h"
#include "externalsextractorsolver.h"
#include "onlinesolver.h"
#include <QApplication>
#include <QSettings>

using namespace SSolver;

StellarSolver::StellarSolver(ProcessType type, const FITSImage::Statistic &imagestats, const uint8_t *imageBuffer,
                             QObject *parent) : QObject(parent), m_Statistics(imagestats)
{
    qRegisterMetaType<SolverType>("SolverType");
    qRegisterMetaType<ProcessType>("ProcessType");
    qRegisterMetaType<SextractorType>("SextractorType");
    m_ProcessType = type;
    m_ImageBuffer = imageBuffer;
    m_Subframe = QRect(0, 0, m_Statistics.width, m_Statistics.height);
}

StellarSolver::StellarSolver(const FITSImage::Statistic &imagestats, uint8_t const *imageBuffer,
                             QObject *parent) : QObject(parent), m_Statistics(imagestats)
{
    qRegisterMetaType<SolverType>("SolverType");
    qRegisterMetaType<ProcessType>("ProcessType");
    qRegisterMetaType<SextractorType>("SextractorType");
    m_ImageBuffer = imageBuffer;
    m_Subframe = QRect(0, 0, m_Statistics.width, m_Statistics.height);
}

SextractorSolver* StellarSolver::createSextractorSolver()
{
    SextractorSolver *solver;

    if(m_ProcessType == SOLVE && m_SolverType == SOLVER_ONLINEASTROMETRY)
    {
        OnlineSolver *onlineSolver = new OnlineSolver(m_ProcessType, m_SextractorType, m_SolverType, m_Statistics, m_ImageBuffer,
                this);
        onlineSolver->fileToProcess = m_FileToProcess;
        onlineSolver->astrometryAPIKey = m_AstrometryAPIKey;
        onlineSolver->astrometryAPIURL = m_AstrometryAPIURL;
        onlineSolver->sextractorBinaryPath = m_SextractorBinaryPath;
        solver = onlineSolver;
    }
    else if((m_ProcessType == SOLVE && m_SolverType == SOLVER_STELLARSOLVER) || (m_ProcessType != SOLVE
            && m_SextractorType != SEXTRACTOR_EXTERNAL))
        solver = new InternalSextractorSolver(m_ProcessType, m_SextractorType, m_SolverType, m_Statistics, m_ImageBuffer, this);
    else
    {
        ExternalSextractorSolver *extSolver = new ExternalSextractorSolver(m_ProcessType, m_SextractorType, m_SolverType,
                m_Statistics, m_ImageBuffer, this);
        extSolver->fileToProcess = m_FileToProcess;
        extSolver->sextractorBinaryPath = m_SextractorBinaryPath;
        extSolver->confPath = m_ConfPath;
        extSolver->solverPath = m_SolverPath;
        extSolver->astapBinaryPath = m_ASTAPBinaryPath;
        extSolver->wcsPath = m_WCSPath;
        extSolver->cleanupTemporaryFiles = m_CleanupTemporaryFiles;
        extSolver->autoGenerateAstroConfig = m_AutoGenerateAstroConfig;
        solver = extSolver;
    }

    if(useSubframe)
        solver->setUseSubframe(m_Subframe);
    solver->logToFile = m_LogToFile;
    solver->logFileName = m_LogFileName;
    solver->logLevel = m_LogLevel;
    solver->basePath = m_BasePath;
    solver->params = params;
    solver->indexFolderPaths = indexFolderPaths;
    if(m_UseScale)
        solver->setSearchScale(m_ScaleLow, m_ScaleHigh, m_ScaleUnit);
    if(m_UsePosition)
        solver->setSearchPositionInDegrees(m_SearchRA, m_SearchDE);
    if(m_LogLevel != LOG_NONE)
        connect(solver, &SextractorSolver::logOutput, this, &StellarSolver::logOutput);

    return solver;
}

//Methods to get default file paths
ExternalProgramPaths StellarSolver::getLinuxDefaultPaths()
{
    return ExternalSextractorSolver::getLinuxDefaultPaths();
};
ExternalProgramPaths StellarSolver::getLinuxInternalPaths()
{
    return ExternalSextractorSolver::getLinuxInternalPaths();
};
ExternalProgramPaths StellarSolver::getMacHomebrewPaths()
{
    return ExternalSextractorSolver::getMacHomebrewPaths();
};
ExternalProgramPaths StellarSolver::getMacInternalPaths()
{
    return ExternalSextractorSolver::getMacInternalPaths();
};
ExternalProgramPaths StellarSolver::getWinANSVRPaths()
{
    return ExternalSextractorSolver::getWinANSVRPaths();
};
ExternalProgramPaths StellarSolver::getWinCygwinPaths()
{
    return ExternalSextractorSolver::getLinuxDefaultPaths();
};

void StellarSolver::extract(bool calculateHFR, QRect frame)
{
    m_ProcessType = calculateHFR ? SEXTRACT_WITH_HFR : SEXTRACT;
    useSubframe = frame.isNull() ? false : true;
    m_Subframe = frame;
    m_isBlocking = true;
    start();
    m_SextractorSolver->executeProcess();
    processFinished(m_SextractorSolver->sextractionDone() ? 0 : 1);
    m_isBlocking = false;
}

void StellarSolver::start()
{
    m_SextractorSolver = createSextractorSolver();
    if(checkParameters() == false)
    {
        emit logOutput("There is an issue with your parameters. Terminating the process.");
        emit ready();
        return;
    }

    m_isRunning = true;
    hasFailed = false;
    if(m_ProcessType == SEXTRACT || m_ProcessType == SEXTRACT_WITH_HFR)
        hasSextracted = false;
    else
    {
        hasSolved = false;
        hasWCS = false;
        hasWCSCoord = false;
        wcs_coord = nullptr;
    }

    if(m_isBlocking)
        return;

    //These are the solvers that support parallelization, ASTAP and the online ones do not
    if(params.multiAlgorithm != NOT_MULTI && m_ProcessType == SOLVE && (m_SolverType == SOLVER_STELLARSOLVER
            || m_SolverType == SOLVER_LOCALASTROMETRY))
    {
        m_SextractorSolver->sextract();
        parallelSolve();
    }
    else if(m_SolverType == SOLVER_ONLINEASTROMETRY)
    {
        connect(m_SextractorSolver, &SextractorSolver::finished, this, &StellarSolver::processFinished);
        m_SextractorSolver->startProcess();
    }
    else
    {
        connect(m_SextractorSolver, &SextractorSolver::finished, this, &StellarSolver::processFinished);
        m_SextractorSolver->startProcess();
    }

}

bool StellarSolver::checkParameters()
{
    if(params.multiAlgorithm != NOT_MULTI && m_SolverType == SOLVER_ASTAP && m_ProcessType == SOLVE)
    {
        emit logOutput("ASTAP does not support Parallel solves.  Disabling that option");
        params.multiAlgorithm = NOT_MULTI;
    }

    if(m_ProcessType == SOLVE && m_SolverType == SOLVER_STELLARSOLVER && m_SextractorType != SEXTRACTOR_INTERNAL)
    {
        emit logOutput("StellarSolver only uses the Internal SEP Sextractor since it doesn't save files to disk. Changing to Internal Sextractor.");
        m_SextractorType = SEXTRACTOR_INTERNAL;

    }

    if(m_ProcessType == SOLVE && params.multiAlgorithm == MULTI_AUTO)
    {
        if(m_UseScale && m_UsePosition)
            params.multiAlgorithm = NOT_MULTI;
        else if(m_UsePosition)
            params.multiAlgorithm = MULTI_SCALES;
        else if(m_UseScale)
            params.multiAlgorithm = MULTI_DEPTHS;
        else
            params.multiAlgorithm = MULTI_SCALES;
    }

    if(m_ProcessType == SOLVE && params.inParallel)
    {
        if(enoughRAMisAvailableFor(indexFolderPaths))
        {
            if(m_LogLevel != LOG_NONE)
                emit logOutput("There should be enough RAM to load the indexes in parallel.");
        }
        else
        {
            if(m_LogLevel != LOG_NONE)
            {
                emit logOutput("Not enough RAM is available on this system for loading the index files you have in parallel");
                emit logOutput("Disabling the inParallel option.");
            }
            params.inParallel = false;
        }
    }

    return true; //For now
}

//This allows us to start multiple threads to search simulaneously in separate threads/cores
//to attempt to efficiently use modern multi core computers to speed up the solve
void StellarSolver::parallelSolve()
{
    if(params.multiAlgorithm == NOT_MULTI || !(m_SolverType == SOLVER_STELLARSOLVER || m_SolverType == SOLVER_LOCALASTROMETRY))
        return;
    parallelSolvers.clear();
    m_ParallelFailsCount = 0;
    int threads = QThread::idealThreadCount();

    if(params.multiAlgorithm == MULTI_SCALES)
    {
        //Attempt to search on multiple scales
        //Note, originally I had each parallel solver getting equal ranges, but solves are faster on bigger scales
        //So now I'm giving the bigger scale solvers more of a range to work with.
        double minScale;
        double maxScale;
        ScaleUnits units;
        if(m_UseScale)
        {
            minScale = m_ScaleLow;
            maxScale = m_ScaleHigh;
            units = m_ScaleUnit;
        }
        else
        {
            minScale = params.minwidth;
            maxScale = params.maxwidth;
            units = DEG_WIDTH;
        }
        double scaleConst = (maxScale - minScale) / pow(threads, 2);
        if(m_LogLevel != LOG_NONE)
            emit logOutput(QString("Starting %1 threads to solve on multiple scales").arg(threads));
        for(double thread = 0; thread < threads; thread++)
        {
            double low = minScale + scaleConst * pow(thread, 2);
            double high = minScale + scaleConst * pow(thread + 1, 2);
            SextractorSolver *solver = m_SextractorSolver->spawnChildSolver(thread);
            connect(solver, &SextractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            solver->setSearchScale(low, high, units);
            parallelSolvers.append(solver);
            if(m_LogLevel != LOG_NONE)
                emit logOutput(QString("Solver # %1, Low %2, High %3 %4").arg(parallelSolvers.count()).arg(low).arg(high).arg(
                                   getScaleUnitString()));
        }
    }
    //Note: it might be useful to do a parallel solve on multiple positions, but I am afraid
    //that since it searches in a circle around the search position, it might be difficult to make it
    //search a square grid without either missing sections of sky or overlapping search regions.
    else if(params.multiAlgorithm == MULTI_DEPTHS)
    {
        //Attempt to search on multiple depths
        int sourceNum = 200;
        if(params.keepNum != 0)
            sourceNum = params.keepNum;
        int inc = sourceNum / threads;
        //We don't need an unnecessary number of threads
        if(inc < 10)
            inc = 10;
        if(m_LogLevel != LOG_NONE)
            emit logOutput(QString("Starting %1 threads to solve on multiple depths").arg(sourceNum / inc));
        for(int i = 1; i < sourceNum; i += inc)
        {
            SextractorSolver *solver = m_SextractorSolver->spawnChildSolver(i);
            connect(solver, &SextractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            solver->depthlo = i;
            solver->depthhi = i + inc;
            parallelSolvers.append(solver);
            if(m_LogLevel != LOG_NONE)
                emit logOutput(QString("Child Solver # %1, Depth Low %2, Depth High %3").arg(parallelSolvers.count()).arg(i).arg(i + inc));
        }
    }
    for(auto solver : parallelSolvers)
        solver->startProcess();
}

bool StellarSolver::parallelSolversAreRunning()
{
    for(auto solver : parallelSolvers)
        if(solver->isRunning())
            return true;
    return false;
}
void StellarSolver::processFinished(int code)
{
    numStars  = m_SextractorSolver->getNumStarsFound();
    if(code == 0)
    {
        if(m_ProcessType == SOLVE && m_SextractorSolver->solvingDone())
        {
            solution = m_SextractorSolver->getSolution();
            if(m_SextractorSolver->hasWCSData())
            {
                hasWCS = true;
                solverWithWCS = m_SextractorSolver;
                if(loadWCS)
                {
                    solverWithWCS->computeWCSForStars = stars.count() > 0;
                    solverWithWCS->computingWCS = true;
                    disconnect(solverWithWCS, &SextractorSolver::finished, this, &StellarSolver::processFinished);
                    connect(solverWithWCS, &SextractorSolver::finished, this, &StellarSolver::finishWCS);
                    solverWithWCS->start();
                }
            }
            hasSolved = true;
        }
        else if((m_ProcessType == SEXTRACT || m_ProcessType == SEXTRACT_WITH_HFR) && m_SextractorSolver->sextractionDone())
        {
            stars = m_SextractorSolver->getStarList();
            background = m_SextractorSolver->getBackground();
            calculateHFR = m_SextractorSolver->isCalculatingHFR();
            if(solverWithWCS)
                stars = solverWithWCS->appendStarsRAandDEC(stars);
            hasSextracted = true;
        }
    }
    else
    {
        hasFailed = true;
        m_isRunning = false;
    }

    if(m_ProcessType != SOLVE || m_SextractorSolver->hasWCSData() || !loadWCS)
        m_isRunning = false;
    emit ready();


}

//This slot listens for signals from the child solvers that they are in fact done with the solve
//If they
void StellarSolver::finishParallelSolve(int success)
{
    SextractorSolver *reportingSolver = qobject_cast<SextractorSolver*>(sender());
    int whichSolver = 0;
    for(int i = 0; i < parallelSolvers.count(); i++ )
    {
        SextractorSolver *solver = parallelSolvers.at(i);
        if(solver == reportingSolver)
            whichSolver = i + 1;
    }

    if(success == 0)
    {
        numStars  = reportingSolver->getNumStarsFound();
        if(m_LogLevel != LOG_NONE)
        {
            emit logOutput(QString("Successfully solved with child solver: %1").arg(whichSolver));
            emit logOutput("Shutting down other child solvers");
        }
        for(auto solver : parallelSolvers)
        {
            disconnect(solver, &SextractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            disconnect(solver, &SextractorSolver::logOutput, this, &StellarSolver::logOutput);
            if(solver != reportingSolver && solver->isRunning())
                solver->abort();
        }
        solution = reportingSolver->getSolution();
        if(reportingSolver->hasWCSData() && loadWCS)
        {
            solverWithWCS = reportingSolver;
            hasWCS = true;
            solverWithWCS->computeWCSForStars = stars.count() > 0;
            solverWithWCS->computingWCS = true;
            disconnect(solverWithWCS, &SextractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            connect(solverWithWCS, &SextractorSolver::finished, this, &StellarSolver::finishWCS);
            connect(solverWithWCS, &SextractorSolver::logOutput, this, &StellarSolver::logOutput);
            solverWithWCS->start();
        }
        hasSolved = true;
        emit ready();

        if(!reportingSolver->hasWCSData() || !loadWCS)
            m_isRunning = false;
    }
    else
    {
        m_ParallelFailsCount++;
        if(m_LogLevel != LOG_NONE)
            emit logOutput(QString("Child solver: %1 did not solve or was aborted").arg(whichSolver));
        if(m_ParallelFailsCount == parallelSolvers.count())
        {
            if(!hasSolved)
                hasFailed = true;
            m_isRunning = false;
            emit ready();
        }
    }
}

void StellarSolver::finishWCS()
{
    if(solverWithWCS)
    {
        wcs_coord = solverWithWCS->getWCSCoord();
        if(solverWithWCS->computeWCSForStars)
            stars = solverWithWCS->getStarList();
        if(wcs_coord){
            hasWCSCoord = true;
            emit wcsReady();
        }
    }
    m_isRunning = false;
}

//This is the abort method.  The way that it works is that it creates a file.  Astrometry.net is monitoring for this file's creation in order to abort.
void StellarSolver::abort()
{
    for(auto solver : parallelSolvers)
        solver->abort();
    if(m_SextractorSolver)
        m_SextractorSolver->abort();
    wasAborted = true;
}

//This method uses a fwhm value to generate the conv filter the sextractor will use.
void StellarSolver::createConvFilterFromFWHM(Parameters *params, double fwhm)
{
    params->fwhm = fwhm;
    params->convFilter.clear();
    double a = 1;
    int size = abs(ceil(fwhm * 0.6));
    for(int y = -size; y <= size; y++ )
    {
        for(int x = -size; x <= size; x++ )
        {
            double value = a * exp( ( -4.0 * log(2.0) * pow(sqrt( pow(x, 2) + pow(y, 2) ), 2) ) / pow(fwhm, 2));
            params->convFilter.append(value);
        }
    }
}

QList<Parameters> StellarSolver::getBuiltInProfiles()
{
    QList<Parameters> profileList;

    Parameters fastSolving;
    fastSolving.listName = "1-FastSolving";
    fastSolving.downsample = 2;
    fastSolving.minwidth = 1;
    fastSolving.maxwidth = 10;
    fastSolving.keepNum = 50;
    fastSolving.maxEllipse = 1.5;
    createConvFilterFromFWHM(&fastSolving, 4);
    profileList.append(fastSolving);

    Parameters parSolving;
    parSolving.listName = "2-ParallelSolving";
    parSolving.multiAlgorithm = MULTI_AUTO;
    parSolving.downsample = 2;
    parSolving.minwidth = 1;
    parSolving.maxwidth = 10;
    parSolving.keepNum = 50;
    parSolving.maxEllipse = 1.5;
    createConvFilterFromFWHM(&parSolving, 2);
    profileList.append(parSolving);

    Parameters parLargeSolving;
    parLargeSolving.listName = "3-ParallelLargeScale";
    parLargeSolving.multiAlgorithm = MULTI_AUTO;
    parLargeSolving.downsample = 2;
    parLargeSolving.minwidth = 1;
    parLargeSolving.maxwidth = 10;
    parLargeSolving.keepNum = 50;
    parLargeSolving.maxEllipse = 1.5;
    createConvFilterFromFWHM(&parLargeSolving, 2);
    profileList.append(parLargeSolving);

    Parameters fastSmallSolving;
    fastSmallSolving.listName = "4-ParallelSmallScale";
    fastSmallSolving.multiAlgorithm = MULTI_AUTO;
    fastSmallSolving.downsample = 2;
    parLargeSolving.minwidth = 1;
    fastSmallSolving.maxwidth = 10;
    fastSmallSolving.keepNum = 50;
    fastSmallSolving.maxEllipse = 1.5;
    createConvFilterFromFWHM(&fastSmallSolving, 2);
    profileList.append(fastSmallSolving);

    Parameters stars;
    stars.listName = "5-AllStars";
    stars.maxEllipse = 1.5;
    createConvFilterFromFWHM(&stars, 1);
    stars.r_min = 2;
    profileList.append(stars);

    Parameters smallStars;
    smallStars.listName = "6-SmallSizedStars";
    smallStars.maxEllipse = 1.5;
    createConvFilterFromFWHM(&smallStars, 1);
    smallStars.r_min = 2;
    smallStars.maxSize = 5;
    smallStars.saturationLimit = 80;
    profileList.append(smallStars);

    Parameters mid;
    mid.listName = "7-MidSizedStars";
    mid.maxEllipse = 1.5;
    mid.minarea = 20;
    createConvFilterFromFWHM(&mid, 4);
    mid.r_min = 5;
    mid.removeDimmest = 20;
    mid.minSize = 2;
    mid.maxSize = 10;
    mid.saturationLimit = 80;
    profileList.append(mid);

    Parameters big;
    big.listName = "8-BigSizedStars";
    big.maxEllipse = 1.5;
    big.minarea = 40;
    createConvFilterFromFWHM(&big, 8);
    big.r_min = 20;
    big.minSize = 5;
    big.removeDimmest = 50;
    profileList.append(big);

    return profileList;
}

QList<SSolver::Parameters> StellarSolver::loadSavedOptionsProfiles(QString savedOptionsProfiles)
{
    QList<SSolver::Parameters> optionsList;
    if(!QFileInfo(savedOptionsProfiles).exists())
    {
        return StellarSolver::getBuiltInProfiles();
    }
    QSettings settings(savedOptionsProfiles, QSettings::IniFormat);
    QStringList groups = settings.childGroups();
    foreach(QString group, groups)
    {
        settings.beginGroup(group);
        QStringList keys = settings.childKeys();
        QMap<QString, QVariant> map;
        foreach(QString key, keys)
            map.insert(key, settings.value(key));
        SSolver::Parameters newParams = SSolver::Parameters::convertFromMap(map);
        settings.endGroup();
        optionsList.append(newParams);
    }
    return optionsList;
}

void StellarSolver::setParameterProfile(SSolver::Parameters::ParametersProfile profile)
{
    QList<Parameters> profileList = getBuiltInProfiles();
    setParameters(profileList.at(profile));
}

void StellarSolver::setUseSubframe(QRect frame)
{
    int x = frame.x();
    int y = frame.y();
    int w = frame.width();
    int h = frame.height();
    if(w < 0)
    {
        x = x + w; //It's negative
        w = -w;
    }
    if(h < 0)
    {
        y = y + h; //It's negative
        h = -h;
    }
    if(x < 0)
        x = 0;
    if(y < 0)
        y = 0;
    if(x > m_Statistics.width)
        x = m_Statistics.width;
    if(y > m_Statistics.height)
        y = m_Statistics.height;

    useSubframe = true;
    m_Subframe = QRect(x, y, w, h);
}

//This is a convenience function used to set all the scale parameters based on the FOV high and low values wit their units.
void StellarSolver::setSearchScale(double fov_low, double fov_high, const QString &scaleUnits)
{
    if(scaleUnits == "dw" || scaleUnits == "degw" || scaleUnits == "degwidth")
        setSearchScale(fov_low, fov_high, DEG_WIDTH);
    if(scaleUnits == "app" || scaleUnits == "arcsecperpix")
        setSearchScale(fov_low, fov_high, ARCSEC_PER_PIX);
    if(scaleUnits == "aw" || scaleUnits == "amw" || scaleUnits == "arcminwidth")
        setSearchScale(fov_low, fov_high, ARCMIN_WIDTH);
    if(scaleUnits == "focalmm")
        setSearchScale(fov_low, fov_high, FOCAL_MM);
}

//This is a convenience function used to set all the scale parameters based on the FOV high and low values wit their units.
void StellarSolver::setSearchScale(double fov_low, double fov_high, ScaleUnits units)
{
    m_UseScale = true;
    //L
    m_ScaleLow = fov_low;
    //H
    m_ScaleHigh = fov_high;
    //u
    m_ScaleUnit = units;
}

//This is a convenience function used to set all the search position parameters based on the ra, dec, and radius
//Warning!!  This method accepts the RA in decimal form and then will convert it to degrees for Astrometry.net
void StellarSolver::setSearchPositionRaDec(double ra, double dec)
{
    setSearchPositionInDegrees(ra * 15.0, dec);
}

//This is a convenience function used to set all the search position parameters based on the ra, dec, and radius
//Warning!!  This method accepts the RA in degrees just like the DEC
void StellarSolver::setSearchPositionInDegrees(double ra, double dec)
{
    m_UsePosition = true;
    //3
    m_SearchRA = ra;
    //4
    m_SearchDE = dec;
}

void addPathToListIfExists(QStringList *list, QString path)
{
    if(list)
    {
        if(QFileInfo(path).exists())
            list->append(path);
    }
}

QStringList StellarSolver::getDefaultIndexFolderPaths()
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



FITSImage::wcs_point * StellarSolver::getWCSCoord()
{
    if(hasWCS && hasWCSCoord)
        return wcs_coord;
    else
        return nullptr;
}

QList<FITSImage::Star> StellarSolver::appendStarsRAandDEC(QList<FITSImage::Star> stars)
{

    return m_SextractorSolver->appendStarsRAandDEC(stars);
}

//This function should get the system RAM in bytes.  I may revise it later to get the currently available RAM
//But from what I read, getting the Available RAM is inconsistent and buggy on many systems.
uint64_t StellarSolver::getAvailableRAM()
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
    if (GlobalMemoryStatusEx(&memory_status))
    {
        RAM = memory_status.ullTotalPhys;
    }
    else
    {
        RAM = 0;
    }
#endif
    return RAM;
}

//This should determine if enough RAM is available to load all the index files in parallel
bool StellarSolver::enoughRAMisAvailableFor(QStringList indexFolders)
{
    uint64_t totalSize = 0;

    foreach(QString folder, indexFolders)
    {
        QDir dir(folder);
        if(dir.exists())
        {
            dir.setNameFilters(QStringList() << "*.fits" << "*.fit");
            QFileInfoList indexInfoList = dir.entryInfoList();
            foreach(QFileInfo indexInfo, indexInfoList)
                totalSize += indexInfo.size();
        }

    }
    uint64_t availableRAM = getAvailableRAM();
    if(availableRAM == 0)
    {
        if(m_LogLevel != LOG_NONE)
            emit logOutput("Unable to determine system RAM for inParallel Option");
        return false;
    }
    float bytesInGB = 1024 * 1024 * 1024; // B -> KB -> MB -> GB , float to make sure it reports the answer with any decimals
    if(m_LogLevel != LOG_NONE)
        emit logOutput(
            QString("Evaluating Installed RAM for inParallel Option.  Total Size of Index files: %1 GB, Installed RAM: %2 GB").arg(
                totalSize / bytesInGB).arg(availableRAM / bytesInGB));
    return availableRAM > totalSize;
}

// Taken from: http://www1.phys.vt.edu/~jhs/phys3154/snr20040108.pdf
double StellarSolver::snr(const FITSImage::Background &background,
                          const FITSImage::Star &star,
                          double gain)
{
    const double numPixelsInSkyEstimate = background.bw * background.bh;
    const double varSky = background.globalrms * background.globalrms;

    if (numPixelsInSkyEstimate <= 0 || gain <= 0)
        return 0;

    // It seems SEP flux subtracts out the background, so no need
    // for numer = flux - star.numPixels * mean;
    const double numer = star.flux;
    const double denom = sqrt( numer / gain + star.numPixels * varSky  * (1 + 1 / numPixelsInSkyEstimate));
    if (denom <= 0)
        return 0;
    return numer / denom;

}
