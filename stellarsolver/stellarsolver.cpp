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

StellarSolver::StellarSolver(ProcessType type, FITSImage::Statistic imagestats, const uint8_t *imageBuffer, QObject *parent) : QThread(parent)
{
     processType = type;
     stats=imagestats;
     m_ImageBuffer=imageBuffer;
     subframe = QRect(0,0,stats.width,stats.height);
}

StellarSolver::StellarSolver(FITSImage::Statistic imagestats, uint8_t const *imageBuffer, QObject *parent) : QThread(parent)
{
     stats=imagestats;
     m_ImageBuffer=imageBuffer;
     subframe = QRect(0,0,stats.width,stats.height);
}

StellarSolver::~StellarSolver()
{

}

SextractorSolver* StellarSolver::createSextractorSolver()
{
    SextractorSolver *solver;

    if(processType == SOLVE && solverType == SOLVER_ONLINEASTROMETRY)
    {
        OnlineSolver *onlineSolver = new OnlineSolver(processType, sextractorType, solverType, stats, m_ImageBuffer, this);
        onlineSolver->fileToProcess = fileToProcess;
        onlineSolver->astrometryAPIKey = astrometryAPIKey;
        onlineSolver->astrometryAPIURL = astrometryAPIURL;
        onlineSolver->sextractorBinaryPath = sextractorBinaryPath;
        solver = onlineSolver;
    }
    else if((processType == SOLVE && solverType == SOLVER_STELLARSOLVER) || (processType != SOLVE && sextractorType != SEXTRACTOR_EXTERNAL))
        solver = new InternalSextractorSolver(processType, sextractorType, solverType, stats, m_ImageBuffer, this);
    else
    {
        ExternalSextractorSolver *extSolver = new ExternalSextractorSolver(processType, sextractorType, solverType, stats, m_ImageBuffer, this);
        extSolver->fileToProcess = fileToProcess;
        extSolver->sextractorBinaryPath = sextractorBinaryPath;
        extSolver->confPath = confPath;
        extSolver->solverPath = solverPath;
        extSolver->astapBinaryPath = astapBinaryPath;
        extSolver->wcsPath = wcsPath;
        extSolver->cleanupTemporaryFiles = cleanupTemporaryFiles;
        extSolver->autoGenerateAstroConfig = autoGenerateAstroConfig;
        solver = extSolver;
    }

    if(useSubframe)
        solver->setUseSubframe(subframe);
    solver->logToFile = logToFile;
    solver->logFileName = logFileName;
    solver->logLevel = logLevel;
    solver->basePath = basePath;
    solver->params = params;
    solver->indexFolderPaths = indexFolderPaths;
    if(use_scale)
        solver->setSearchScale(scalelo, scalehi, scaleunit);
    if(use_position)
        solver->setSearchPositionInDegrees(search_ra, search_dec);
    if(logLevel != LOG_NONE)
        connect(solver, &SextractorSolver::logOutput, this, &StellarSolver::logOutput);

    return solver;
}

//Methods to get default file paths
ExternalProgramPaths StellarSolver::getLinuxDefaultPaths(){return ExternalSextractorSolver::getLinuxDefaultPaths();};
ExternalProgramPaths StellarSolver::getLinuxInternalPaths(){return ExternalSextractorSolver::getLinuxInternalPaths();};
ExternalProgramPaths StellarSolver::getMacHomebrewPaths(){return ExternalSextractorSolver::getMacHomebrewPaths();};
ExternalProgramPaths StellarSolver::getMacInternalPaths(){return ExternalSextractorSolver::getMacInternalPaths();};
ExternalProgramPaths StellarSolver::getWinANSVRPaths(){return ExternalSextractorSolver::getWinANSVRPaths();};
ExternalProgramPaths StellarSolver::getWinCygwinPaths(){return ExternalSextractorSolver::getLinuxDefaultPaths();};


void StellarSolver::sextract()
{
    processType = SEXTRACT;
    useSubframe = false;
    executeProcess();
}

void StellarSolver::sextractWithHFR()
{
    processType = SEXTRACT_WITH_HFR;
    useSubframe = false;
    executeProcess();
}

void StellarSolver::sextract(QRect frame)
{
    processType = SEXTRACT;
    subframe = frame;
    useSubframe = true;
    executeProcess();
}

void StellarSolver::sextractWithHFR(QRect frame)
{
    processType = SEXTRACT_WITH_HFR;
    subframe = frame;
    useSubframe = true;
    executeProcess();
}

void StellarSolver::startsextraction()
{
    processType = SEXTRACT;
    startProcess();
}

void StellarSolver::startSextractionWithHFR()
{
    processType = SEXTRACT_WITH_HFR;
    startProcess();
}

void StellarSolver::startProcess()
{
    sextractorSolver = createSextractorSolver();
    start();
}

void StellarSolver::executeProcess()
{
    sextractorSolver = createSextractorSolver();
    start();
    while(!hasSextracted && !hasSolved && !hasFailed && !wasAborted)
        QApplication::processEvents();
}

bool StellarSolver::checkParameters()
{
    if(params.multiAlgorithm != NOT_MULTI && solverType == SOLVER_ASTAP && processType == SOLVE)
    {
        emit logOutput("ASTAP does not support Parallel solves.  Disabling that option");
        params.multiAlgorithm = NOT_MULTI;
    }

    if(processType == SOLVE && solverType == SOLVER_STELLARSOLVER && sextractorType != SEXTRACTOR_INTERNAL)
    {
        emit logOutput("StellarSolver only uses the Internal SEP Sextractor since it doesn't save files to disk. Changing to Internal Sextractor.");
        sextractorType = SEXTRACTOR_INTERNAL;

    }

    if(params.multiAlgorithm == MULTI_AUTO)
    {
        if(use_scale && use_position)
            params.multiAlgorithm = NOT_MULTI;
        else if(use_position)
            params.multiAlgorithm = MULTI_SCALES;
        else if(use_scale)
            params.multiAlgorithm = MULTI_DEPTHS;
        else
            params.multiAlgorithm = MULTI_SCALES;
    }

    if(params.inParallel)
    {
        if(enoughRAMisAvailableFor(indexFolderPaths))
        {
            if(logLevel != LOG_NONE)
                emit logOutput("There should be enough RAM to load the indexes in parallel.");
        }
        else
        {
            if(logLevel != LOG_NONE)
                emit logOutput("Not enough RAM is available on this system for loading the index files you have in parallel");
            if(logLevel != LOG_NONE)
                emit logOutput("Disabling the inParallel option.");
            params.inParallel = false;
        }
    }

    return true; //For now
}

void StellarSolver::run()
{
    if(checkParameters() == false)
    {
        emit logOutput("There is an issue with your parameters.  Terminating the process.");
        return;
    }

    hasFailed = false;
    if(processType == SEXTRACT || processType == SEXTRACT_WITH_HFR)
        hasSextracted = false;
    else
        hasSolved = false;

    //These are the solvers that support parallelization, ASTAP and the online ones do not
    if(params.multiAlgorithm != NOT_MULTI && processType == SOLVE && (solverType == SOLVER_STELLARSOLVER || solverType == SOLVER_LOCALASTROMETRY))
    {
        sextractorSolver->sextract();
        parallelSolve();

        while(!hasSolved && !wasAborted && parallelSolversAreRunning())
            msleep(100);

        if(loadWCS && hasWCS && solverWithWCS)
        {
            wcs_coord = solverWithWCS->getWCSCoord();
            if(wcs_coord)
            {
                if(stars.count() > 0)
                    stars = solverWithWCS->appendStarsRAandDEC(stars);
                emit wcsDataisReady();
            }
        }
        while(parallelSolversAreRunning())
            msleep(100);
    }
    else if(solverType == SOLVER_ONLINEASTROMETRY)
    {
        connect(sextractorSolver, &SextractorSolver::finished, this, &StellarSolver::processFinished);
        sextractorSolver->startProcess();
        while(sextractorSolver->isRunning())
            msleep(100);
    }
    else
    {
        connect(sextractorSolver, &SextractorSolver::finished, this, &StellarSolver::processFinished);
        sextractorSolver->executeProcess();
    }
    if(logLevel != LOG_NONE)
        emit logOutput("All Processes Complete");
}

//This allows us to start multiple threads to search simulaneously in separate threads/cores
//to attempt to efficiently use modern multi core computers to speed up the solve
void StellarSolver::parallelSolve()
{
    if(params.multiAlgorithm == NOT_MULTI || !(solverType == SOLVER_STELLARSOLVER || solverType == SOLVER_LOCALASTROMETRY))
        return;
    parallelSolvers.clear();
    parallelFails = 0;
    int threads = idealThreadCount();

    if(params.multiAlgorithm == MULTI_SCALES)
    {
        //Attempt to search on multiple scales
        //Note, originally I had each parallel solver getting equal ranges, but solves are faster on bigger scales
        //So now I'm giving the bigger scale solvers more of a range to work with.
        double minScale;
        double maxScale;
        ScaleUnits units;
        if(use_scale)
        {
            minScale = scalelo;
            maxScale = scalehi;
            units = scaleunit;
        }
        else
        {
            minScale = params.minwidth;
            maxScale = params.maxwidth;
            units = DEG_WIDTH;
        }
        double scaleConst = (maxScale - minScale) / pow(threads,2);
        if(logLevel != LOG_NONE)
            emit logOutput(QString("Starting %1 threads to solve on multiple scales").arg(threads));
        for(double thread = 0; thread < threads; thread++)
        {
            double low = minScale + scaleConst * pow(thread,2);
            double high = minScale + scaleConst * pow(thread + 1, 2);
            SextractorSolver *solver = sextractorSolver->spawnChildSolver(thread);
            connect(solver, &SextractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            solver->setSearchScale(low, high, units);
            parallelSolvers.append(solver);
            if(logLevel != LOG_NONE)
                emit logOutput(QString("Solver # %1, Low %2, High %3 %4").arg(parallelSolvers.count()).arg(low).arg(high).arg(getScaleUnitString()));
        }
    }
    //Note: it might be useful to do a parallel solve on multiple positions, but I am afraid
    //that since it searches in a circle around the search position, it might be difficult to make it
    //search a square grid without either missing sections of sky or overlapping search regions.
    else if(params.multiAlgorithm == MULTI_DEPTHS)
    {
        //Attempt to search on multiple depths
        int sourceNum = 200;
        if(params.keepNum !=0)
            sourceNum = params.keepNum;
        int inc = sourceNum / threads;
        //We don't need an unnecessary number of threads
        if(inc < 10)
            inc = 10;
        if(logLevel != LOG_NONE)
            emit logOutput(QString("Starting %1 threads to solve on multiple depths").arg(sourceNum / inc));
        for(int i = 1; i < sourceNum; i += inc)
        {
            SextractorSolver *solver = sextractorSolver->spawnChildSolver(i);
            connect(solver, &SextractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            solver->depthlo = i;
            solver->depthhi = i + inc;
            parallelSolvers.append(solver);
            if(logLevel != LOG_NONE)
                emit logOutput(QString("Child Solver # %1, Depth Low %2, Depth High %3").arg(parallelSolvers.count()).arg(i).arg(i + inc));
        }
    }
    foreach(SextractorSolver *solver, parallelSolvers)
        solver->startProcess();
}

bool StellarSolver::parallelSolversAreRunning()
{
    foreach(SextractorSolver *solver, parallelSolvers)
        if(solver->isRunning())
            return true;
    return false;
}
void StellarSolver::processFinished(int code)
{
    numStars  = sextractorSolver->getNumStarsFound();
    if(code == 0)
    {
        //This means it was a Solving Command
        if(sextractorSolver->solvingDone())
        {
            solution = sextractorSolver->getSolution();
            if(sextractorSolver->hasWCSData())
            {
                hasWCS = true;
                solverWithWCS = sextractorSolver;
            }
            hasSolved = true;
        }
        //This means it was a Sextraction Command
        else
        {
            stars = sextractorSolver->getStarList();
            background = sextractorSolver->getBackground();
            calculateHFR = sextractorSolver->isCalculatingHFR();
            if(solverWithWCS)
                stars = solverWithWCS->appendStarsRAandDEC(stars);
            hasSextracted = true;
        }
    }
    else
        hasFailed = true;
    emit finished(code);
    if(sextractorSolver->solvingDone())
    {
        if(loadWCS && hasWCS && solverWithWCS)
        {
            wcs_coord = solverWithWCS->getWCSCoord();
            stars = solverWithWCS->appendStarsRAandDEC(stars);
            if(wcs_coord)
                emit wcsDataisReady();
        }
    }
}


//This slot listens for signals from the child solvers that they are in fact done with the solve
//If they
void StellarSolver::finishParallelSolve(int success)
{
    SextractorSolver *reportingSolver = (SextractorSolver*)sender();
    int whichSolver = 0;
    for(int i =0; i<parallelSolvers.count(); i++ )
    {
        SextractorSolver *solver = parallelSolvers.at(i);
        if(solver == reportingSolver)
            whichSolver = i + 1;
    }

    if(success == 0)
    {
        numStars  = reportingSolver->getNumStarsFound();
        if(logLevel != LOG_NONE)
            emit logOutput(QString("Successfully solved with child solver: %1").arg(whichSolver));
        if(logLevel != LOG_NONE)
            emit logOutput("Shutting down other child solvers");
        foreach(SextractorSolver *solver, parallelSolvers)
        {
            disconnect(solver, &SextractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            disconnect(solver, &SextractorSolver::logOutput, this, &StellarSolver::logOutput);
            if(solver != reportingSolver && solver->isRunning())
                solver->abort();
        }
        solution = reportingSolver->getSolution();
        if(reportingSolver->hasWCSData())
        {
            solverWithWCS = reportingSolver;
            hasWCS = true;
        }
        hasSolved = true;
        emit finished(0);
    }
    else
    {
        parallelFails++;
        if(logLevel != LOG_NONE)
            emit logOutput(QString("Child solver: %1 did not solve or was aborted").arg(whichSolver));
        if(parallelFails == parallelSolvers.count())
            emit finished(-1);
    }
}

//This is the abort method.  The way that it works is that it creates a file.  Astrometry.net is monitoring for this file's creation in order to abort.
void StellarSolver::abort()
{
    foreach(SextractorSolver *solver, parallelSolvers)
        solver->abort();
    if(sextractorSolver)
        sextractorSolver->abort();
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
            double value = a * exp( ( -4.0 * log(2.0) * pow(sqrt( pow(x,2) + pow(y,2) ),2) ) / pow(fwhm,2));
            params->convFilter.append(value);
        }
    }
}

QList<Parameters> StellarSolver::getBuiltInProfiles()
{
    QList<Parameters> profileList;

    Parameters fastSolving;
    fastSolving.listName = "FastSolving";
    fastSolving.downsample = 2;
    fastSolving.minwidth = 1;
    fastSolving.maxwidth = 10;
    fastSolving.keepNum = 50;
    fastSolving.maxEllipse = 1.5;
    createConvFilterFromFWHM(&fastSolving, 4);
    profileList.append(fastSolving);

    Parameters parSolving;
    parSolving.listName = "ParallelSolving";
    parSolving.multiAlgorithm = MULTI_AUTO;
    parSolving.downsample = 2;
    parSolving.minwidth = 1;
    parSolving.maxwidth = 10;
    parSolving.keepNum = 50;
    parSolving.maxEllipse = 1.5;
    createConvFilterFromFWHM(&parSolving, 2);
    profileList.append(parSolving);

    Parameters parLargeSolving;
    parLargeSolving.listName = "ParallelLargeScale";
    parLargeSolving.multiAlgorithm = MULTI_AUTO;
    parLargeSolving.downsample = 2;
    parLargeSolving.minwidth = 1;
    parLargeSolving.maxwidth = 10;
    parLargeSolving.keepNum = 50;
    parLargeSolving.maxEllipse = 1.5;
    createConvFilterFromFWHM(&parLargeSolving, 2);
    profileList.append(parLargeSolving);

    Parameters fastSmallSolving;
    fastSmallSolving.listName = "ParallelSmallScale";
    fastSmallSolving.multiAlgorithm = MULTI_AUTO;
    fastSmallSolving.downsample = 2;
    parLargeSolving.minwidth = 1;
    fastSmallSolving.maxwidth = 10;
    fastSmallSolving.keepNum = 50;
    fastSmallSolving.maxEllipse = 1.5;
    createConvFilterFromFWHM(&fastSmallSolving, 2);
    profileList.append(fastSmallSolving);

    Parameters stars;
    stars.listName = "AllStars";
    stars.maxEllipse = 1.5;
    createConvFilterFromFWHM(&stars, 1);
    stars.r_min = 2;
    profileList.append(stars);

    Parameters smallStars;
    smallStars.listName = "SmallSizedStars";
    smallStars.maxEllipse = 1.5;
    createConvFilterFromFWHM(&smallStars, 1);
    smallStars.r_min = 2;
    smallStars.maxSize = 5;
    smallStars.saturationLimit = 80;
    profileList.append(smallStars);

    Parameters mid;
    mid.listName = "MidSizedStars";
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
    big.listName = "BigSizedStars";
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
        foreach(SSolver::Parameters params, optionsList)
        {
            optionsList.append(newParams);
        }
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
    if(x > stats.width)
        x = stats.width;
    if(y > stats.height)
        y = stats.height;

    useSubframe = true;
    subframe = QRect(x, y, w, h);
}

//This is a convenience function used to set all the scale parameters based on the FOV high and low values wit their units.
void StellarSolver::setSearchScale(double fov_low, double fov_high, QString scaleUnits)
{
    if(scaleUnits =="dw" || scaleUnits =="degw" || scaleUnits =="degwidth")
        setSearchScale(fov_low, fov_high, DEG_WIDTH);
    if(scaleUnits == "app" || scaleUnits == "arcsecperpix")
        setSearchScale(fov_low, fov_high, ARCSEC_PER_PIX);
    if(scaleUnits =="aw" || scaleUnits =="amw" || scaleUnits =="arcminwidth")
        setSearchScale(fov_low, fov_high, ARCMIN_WIDTH);
    if(scaleUnits =="focalmm")
        setSearchScale(fov_low, fov_high, FOCAL_MM);
}

//This is a convenience function used to set all the scale parameters based on the FOV high and low values wit their units.
void StellarSolver::setSearchScale(double fov_low, double fov_high, ScaleUnits units)
{
    use_scale = true;
    //L
    scalelo = fov_low;
    //H
    scalehi = fov_high;
    //u
    scaleunit = units;
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
    use_position = true;
    //3
    search_ra = ra;
    //4
    search_dec = dec;
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
    if(hasWCS)
        return wcs_coord;
    else
        return nullptr;
}

QList<FITSImage::Star> StellarSolver::appendStarsRAandDEC(QList<FITSImage::Star> stars)
{

    return sextractorSolver->appendStarsRAandDEC(stars);
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
    if (GlobalMemoryStatusEx(&memory_status)) {
      RAM = memory_status.ullTotalPhys;
    } else {
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
            dir.setNameFilters(QStringList()<<"*.fits"<<"*.fit");
            QFileInfoList indexInfoList = dir.entryInfoList();
            foreach(QFileInfo indexInfo, indexInfoList)
                totalSize += indexInfo.size();
        }

    }
    uint64_t availableRAM = getAvailableRAM();
    if(availableRAM == 0)
    {
        if(logLevel != LOG_NONE)
            emit logOutput("Unable to determine system RAM for inParallel Option");
        return false;
    }
    float bytesInGB = 1024 * 1024 * 1024; // B -> KB -> MB -> GB , float to make sure it reports the answer with any decimals
    if(logLevel != LOG_NONE)
        emit logOutput(QString("Evaluating Installed RAM for inParallel Option.  Total Size of Index files: %1 GB, Installed RAM: %2 GB").arg(totalSize / bytesInGB).arg(availableRAM / bytesInGB));
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
