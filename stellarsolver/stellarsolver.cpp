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
    qRegisterMetaType<ExtractorType>("ExtractorType");
    m_ProcessType = type;
    m_ImageBuffer = imageBuffer;
    m_Subframe = QRect(0, 0, m_Statistics.width, m_Statistics.height);
}

StellarSolver::StellarSolver(const FITSImage::Statistic &imagestats, uint8_t const *imageBuffer,
                             QObject *parent) : QObject(parent), m_Statistics(imagestats)
{
    qRegisterMetaType<SolverType>("SolverType");
    qRegisterMetaType<ProcessType>("ProcessType");
    qRegisterMetaType<ExtractorType>("ExtractorType");
    m_ImageBuffer = imageBuffer;
    m_Subframe = QRect(0, 0, m_Statistics.width, m_Statistics.height);
}
StellarSolver::~StellarSolver()
{

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
            && m_SextractorType != EXTRACTOR_EXTERNAL))
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
        extSolver->watneyBinaryPath = m_WatneyBinaryPath;
        extSolver->wcsPath = m_WCSPath;
        extSolver->cleanupTemporaryFiles = m_CleanupTemporaryFiles;
        extSolver->autoGenerateAstroConfig = m_AutoGenerateAstroConfig;
        extSolver->onlySendFITSFiles = m_OnlySendFITSFiles;
        solver = extSolver;
    }

    if(useSubframe)
        solver->setUseSubframe(m_Subframe);
    solver->m_LogToFile = m_LogToFile;
    solver->m_LogFileName = m_LogFileName;
    solver->m_AstrometryLogLevel = m_AstrometryLogLevel;
    solver->m_SSLogLevel = m_SSLogLevel;
    solver->m_BasePath = m_BasePath;
    solver->m_ActiveParameters = params;
    solver->convFilter = convFilter;
    solver->indexFolderPaths = indexFolderPaths;
    solver->indexFiles = indexFiles;
    if(m_UseScale)
        solver->setSearchScale(m_ScaleLow, m_ScaleHigh, m_ScaleUnit);
    if(m_UsePosition)
        solver->setSearchPositionInDegrees(m_SearchRA, m_SearchDE);
    if(m_SSLogLevel != LOG_OFF)
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
ExternalProgramPaths StellarSolver::getWinANSVRPaths()
{
    return ExternalSextractorSolver::getWinANSVRPaths();
};
ExternalProgramPaths StellarSolver::getWinCygwinPaths()
{
    return ExternalSextractorSolver::getLinuxDefaultPaths();
};

bool StellarSolver::extract(bool calculateHFR, QRect frame)
{
    m_ProcessType = calculateHFR ? EXTRACT_WITH_HFR : EXTRACT;
    useSubframe = !frame.isNull() && frame.isValid();
    if (useSubframe)
        m_Subframe = frame;
    QEventLoop loop;
    connect(this, &StellarSolver::finished, &loop, &QEventLoop::quit);
    start();
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    return m_HasExtracted;
}

bool StellarSolver::solve()
{
    m_ProcessType = SOLVE;
    QEventLoop loop;
    connect(this, &StellarSolver::finished, &loop, &QEventLoop::quit);
    start();
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    return m_HasSolved;
}

//This will allow the solver to gracefully disconnect, abort, finish, and get deleted
//Right now the internal solvers are all deleted when StellarSolver is deleted
//I might try experimenting with this.
void StellarSolver::releaseSextractorSolver(SextractorSolver *solver)
{
    if(solver != nullptr)
    {
        if(solver->isRunning())
        {
            connect(solver, &SextractorSolver::finished, solver, &SextractorSolver::deleteLater);
            solver->disconnect(this);
            solver->abort();
        }
        else
            solver->deleteLater();
    }
}

void StellarSolver::start()
{
    if(checkParameters() == false)
    {
        emit logOutput("There is an issue with your parameters. Terminating the process.");
        m_isRunning = false;
        m_HasFailed = true;
        emit ready();
        emit finished();
        return;
    }

    //This is necessary before starting up so that the correct convolution filter gets passed to the SextractorSolver
    updateConvolutionFilter();

    m_SextractorSolver = createSextractorSolver();

    m_isRunning = true;
    m_HasFailed = false;
    if(m_ProcessType == EXTRACT || m_ProcessType == EXTRACT_WITH_HFR)
    {
        m_ExtractorStars.clear();
        m_HasExtracted = false;
    }
    else
    {
        m_SolverStars.clear();
        m_HasSolved = false;
        hasWCS = false;
    }

    //These are the solvers that support parallelization, ASTAP and the online ones do not
    if(params.multiAlgorithm != NOT_MULTI && m_ProcessType == SOLVE && (m_SolverType == SOLVER_STELLARSOLVER
            || m_SolverType == SOLVER_LOCALASTROMETRY))
    {
        //Note that it is good to do the Star Extraction before parallelization because it doesn't make sense to repeat this step in all the threads, especially since SEP is now also parallelized in StellarSolver.
        if(m_SextractorType != EXTRACTOR_BUILTIN)
        {
            m_SextractorSolver->extract();
            if(m_SextractorSolver->getNumStarsFound() == 0)
            {
                emit logOutput("No stars were found, so the image cannot be solved");
                m_isRunning = false;
                m_HasFailed = true;
                emit ready();
                emit finished();
                return;
            }
        }
        //Note that converting the image to a FITS file if desired, doesn't need to be repeated in all the threads, but also CFITSIO fails when accessed by multiple parallel threads.
        if(m_SolverType == SOLVER_LOCALASTROMETRY && m_SextractorType == EXTRACTOR_BUILTIN && m_OnlySendFITSFiles)
        {
            QPointer<ExternalSextractorSolver> extSolver = (ExternalSextractorSolver*) (SextractorSolver*) m_SextractorSolver;
            QFileInfo file(extSolver->fileToProcess);
            if(file.suffix() != "fits" && file.suffix() != "fit")
            {
                int ret = extSolver->saveAsFITS();
                if(ret != 0)
                {
                    emit logOutput("Failed to save FITS File.");
                    return;
                }
            }
        }
        parallelSolve();
    }
    else if(m_SolverType == SOLVER_ONLINEASTROMETRY)
    {
        connect(m_SextractorSolver, &SextractorSolver::finished, this, &StellarSolver::processFinished);
        m_SextractorSolver->execute();
    }
    else
    {
        connect(m_SextractorSolver, &SextractorSolver::finished, this, &StellarSolver::processFinished);
        m_SextractorSolver->start();
    }

}

bool StellarSolver::checkParameters()
{
    if(m_SolverType == SOLVER_ASTAP && m_SextractorType != EXTRACTOR_BUILTIN)
    {
        if(m_SSLogLevel != LOG_OFF)
            emit logOutput("ASTAP no longer supports alternative star extraction methods.  Changing to built-in star extraction.");
        m_SextractorType = EXTRACTOR_BUILTIN;
    }

    if(params.multiAlgorithm != NOT_MULTI && m_SolverType == SOLVER_ASTAP && m_ProcessType == SOLVE)
    {
        if(m_SSLogLevel != LOG_OFF)
            emit logOutput("ASTAP does not support multi-threaded solves.  Disabling that option");
        params.multiAlgorithm = NOT_MULTI;
    }

    if(m_ProcessType == SOLVE  && params.autoDownsample)
    {
        //Take whichever one is bigger
        int imageSize = m_Statistics.width > m_Statistics.height ? m_Statistics.width : m_Statistics.height;
        params.downsample = imageSize / 2048 + 1;
        if(m_SSLogLevel != LOG_OFF)
            emit logOutput(QString("Automatically downsampling the image by %1").arg(params.downsample));
    }

    if(m_ProcessType == SOLVE && m_SolverType != SOLVER_ASTAP)
    {
        if(m_SolverType == SOLVER_STELLARSOLVER && m_SextractorType != EXTRACTOR_INTERNAL)
        {
            if(m_SSLogLevel != LOG_OFF)
                emit logOutput("StellarSolver only uses the Internal SEP Sextractor since it doesn't save files to disk. Changing to Internal Star Extractor.");
            m_SextractorType = EXTRACTOR_INTERNAL;
        }

        if(params.multiAlgorithm == MULTI_AUTO)
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

        if(params.inParallel)
        {
            if(enoughRAMisAvailableFor(indexFolderPaths))
            {
                if(m_SSLogLevel != LOG_OFF)
                    emit logOutput("There should be enough RAM to load the indexes in parallel.");
            }
            else
            {
                if(m_SSLogLevel != LOG_OFF)
                {
                    emit logOutput("Not enough RAM is available on this system for loading the index files you have in parallel");
                    emit logOutput("Disabling the inParallel option.");
                }
                params.inParallel = false;
            }
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
    m_ParallelSolversFinishedCount = 0;
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
        if(m_SSLogLevel != LOG_OFF)
            emit logOutput(QString("Starting %1 threads to solve on multiple scales").arg(threads));
        for(int thread = 0; thread < threads; thread++)
        {
            double low = minScale + scaleConst * pow(thread, 2);
            double high = minScale + scaleConst * pow(thread + 1, 2);
            SextractorSolver *solver = m_SextractorSolver->spawnChildSolver(thread);
            connect(solver, &SextractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            solver->setSearchScale(low, high, units);
            parallelSolvers.append(solver);
            if(m_SSLogLevel != LOG_OFF)
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
        if(m_SSLogLevel != LOG_OFF)
            emit logOutput(QString("Starting %1 threads to solve on multiple depths").arg(sourceNum / inc));
        for(int i = 1; i < sourceNum; i += inc)
        {
            SextractorSolver *solver = m_SextractorSolver->spawnChildSolver(i);
            connect(solver, &SextractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            solver->depthlo = i;
            solver->depthhi = i + inc;
            parallelSolvers.append(solver);
            if(m_SSLogLevel != LOG_OFF)
                emit logOutput(QString("Child Solver # %1, Depth Low %2, Depth High %3").arg(parallelSolvers.count()).arg(i).arg(i + inc));
        }
    }
    for(auto &solver : parallelSolvers)
        solver->start();
}

bool StellarSolver::parallelSolversAreRunning() const
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
            solutionIndexNumber = m_SextractorSolver->getSolutionIndexNumber();
            solutionHealpix = m_SextractorSolver->getSolutionHealpix();
            m_SolverStars = m_SextractorSolver->getStarList();
            if(m_SextractorSolver->hasWCSData())
            {
                hasWCS = true;
                solverWithWCS = m_SextractorSolver;
                if(m_ExtractorStars.count() > 0)
                    solverWithWCS->appendStarsRAandDEC(m_ExtractorStars);
            }
            m_HasSolved = true;
        }
        else if((m_ProcessType == EXTRACT || m_ProcessType == EXTRACT_WITH_HFR) && m_SextractorSolver->sextractionDone())
        {
            m_ExtractorStars = m_SextractorSolver->getStarList();
            background = m_SextractorSolver->getBackground();
            m_CalculateHFR = m_SextractorSolver->isCalculatingHFR();
            if(solverWithWCS)
                solverWithWCS->appendStarsRAandDEC(m_ExtractorStars);
            m_HasExtracted = true;
        }
    }
    else
        m_HasFailed = true;

    m_isRunning = false;

    emit ready();
    emit finished();
}

int StellarSolver::whichSolver(SextractorSolver *solver)
{
    for(int i = 0; i < parallelSolvers.count(); i++ )
    {
        if(parallelSolvers.at(i) == solver)
            return i + 1;
    }
    return 0;
}

//This slot listens for signals from the child solvers that they are in fact done with the solve
//If they
void StellarSolver::finishParallelSolve(int success)
{
    m_ParallelSolversFinishedCount++;
    SextractorSolver *reportingSolver = qobject_cast<SextractorSolver*>(sender());
    if(!reportingSolver)
        return;

    if(success == 0 && !m_HasSolved)
    {
        for(auto &solver : parallelSolvers)
        {
            disconnect(solver, &SextractorSolver::logOutput, this, &StellarSolver::logOutput);
            if(solver != reportingSolver && solver->isRunning())
                solver->abort();
        }
        if(m_SSLogLevel != LOG_OFF)
        {
            emit logOutput(QString("Successfully solved with child solver: %1").arg(whichSolver(reportingSolver)));
            emit logOutput("Shutting down other child solvers");
        }

        numStars = reportingSolver->getNumStarsFound();
        solution = reportingSolver->getSolution();
        solutionIndexNumber = reportingSolver->getSolutionIndexNumber();
        solutionHealpix = reportingSolver->getSolutionHealpix();
        m_SolverStars = reportingSolver->getStarList();

        if(reportingSolver->hasWCSData())
        {
            solverWithWCS = reportingSolver;
            hasWCS = true;
            disconnect(solverWithWCS, &SextractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            if(m_ExtractorStars.count() > 0)
                solverWithWCS->appendStarsRAandDEC(m_ExtractorStars);
            m_isRunning = false;
        }
        if(m_SextractorType !=
                EXTRACTOR_BUILTIN) //Note this is just cleaning up the files from the star extraction done prior to the parallel solve.  So for built in, it doesn't even do it, so no files to clean up
            m_SextractorSolver->cleanupTempFiles();
        m_HasSolved = true;
        emit ready();
    }
    else
    {
        if(m_SSLogLevel != LOG_OFF && !m_HasSolved)
            emit logOutput(QString("Child solver: %1 did not solve or was aborted").arg(whichSolver(reportingSolver)));
    }

    if(m_ParallelSolversFinishedCount == parallelSolvers.count())
    {
        m_isRunning = false;
        if(!m_HasSolved){
            m_HasFailed = true;
            emit ready(); //Since this was emitted earlier if it had solved
        }
        emit finished();
    }
}

bool StellarSolver::wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint)
{
    if(hasWCS && solverWithWCS)
    {
        solverWithWCS->wcsToPixel(skyPoint, pixelPoint);
        return true;
    }
    return false;
}

bool StellarSolver::pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint)
{
    if(hasWCS && solverWithWCS)
    {
        solverWithWCS->pixelToWCS(pixelPoint, skyPoint);
        return true;
    }
    return false;
}

//This is the abort method.  The way that it works is that it creates a file.  Astrometry.net is monitoring for this file's creation in order to abort.
void StellarSolver::abort()
{
    for(auto &solver : parallelSolvers)
        solver->abort();
    if(m_SextractorSolver)
        m_SextractorSolver->abort();
    wasAborted = true;
}

//This method checks all the solvers and the internal running boolean to determine if anything is running.
bool StellarSolver::isRunning() const
{
    if(parallelSolversAreRunning())
        return true;
    if(m_SextractorSolver && m_SextractorSolver->isRunning())
        return true;
    return m_isRunning;
}

//This method uses a fwhm value to generate the conv filter the sextractor will use.
QVector<float> StellarSolver::generateConvFilter(SSolver::ConvFilterType filter, double fwhm)
{
    QVector<float> convFilter;
    int size = abs(ceil(fwhm));
    double amplitude = 1.0;
    if(filter == SSolver::CONV_DEFAULT)
    {
        convFilter = {1, 2, 1,
                      2, 4, 2,
                      1, 2, 1
                     };
    }
    else if(filter == SSolver::CONV_CUSTOM)
    {
        //Error! Generate Conv Filter should not get called if using a custom conv filter
    }
    else if(filter == SSolver::CONV_GAUSSIAN)
    {
        for(int y = -size; y <= size; y++ )
        {
            for(int x = -size; x <= size; x++ )
            {
                double value = amplitude * exp( ( -4.0 * log(2.0) * pow(sqrt( pow(x, 2) + pow(y, 2) ), 2) ) / pow(size, 2));
                convFilter.append(value);
            }
        }
    }
    else if(filter == SSolver::CONV_MEXICAN_HAT)
    {

        for(int y = -size; y <= size; y++ )
        {
            for(int x = -size; x <= size; x++ )
            {
                //Formula inspired by astropy package
                double rr_ww = (pow(x, 2) + pow(y, 2)) / (2 * pow(size, 2));
                convFilter.append(amplitude * (1 - rr_ww) * exp(- rr_ww));
            }
        }
    }
    else if(filter == SSolver::CONV_TOP_HAT)
    {
        for(int y = -size; y <= size; y++ )
        {
            for(int x = -size; x <= size; x++ )
            {
                double a = 0;
                if(1.2*abs(x)/size + 1.2*abs(y)/size <= 1)
                    a = 1.0;
                else a = 0;

                convFilter.append(amplitude * a);
            }
        }
    }
    else if(filter == SSolver::CONV_RING)
    {
        for(int y = -size; y <= size; y++ )
        {
            for(int x = -size; x <= size; x++ )
            {
                double value1 = amplitude * exp( ( -4.0 * log(2.0) * pow(sqrt( pow(x, 2) + pow(y, 2) ), 2) ) / pow(size, 2));
                double value2 = amplitude * exp( ( -4.0 * log(2.0) * pow(sqrt( pow(x, 2) + pow(y, 2) ), 2) ) / pow(size/2.0, 2));
                convFilter.append(value1 - value2);
            }
        }
    }
    else
    {
        convFilter = {1, 2, 1,
                      2, 4, 2,
                      1, 2, 1
                     };
    }
    return convFilter;
}

void StellarSolver::updateConvolutionFilter()
{
    if(params.convFilterType == SSolver::CONV_CUSTOM)
        return;
    convFilter.clear();
    convFilter = generateConvFilter(params.convFilterType, params.fwhm);
}

QList<Parameters> StellarSolver::getBuiltInProfiles()
{
    QList<Parameters> profileList;

    Parameters defaultProfile;
    defaultProfile.listName = "1-Default";
    defaultProfile.description = "Default profile. Generic and not optimized for any specific purpose.";
    profileList.append(defaultProfile);

    Parameters singleThreadSolving;
    singleThreadSolving.listName = "2-SingleThreadSolving";
    singleThreadSolving.description = "Profile intended for Plate Solving telescopic sized images in a single CPU Thread";
    singleThreadSolving.multiAlgorithm = NOT_MULTI;
    singleThreadSolving.minwidth = 0.1;
    singleThreadSolving.maxwidth = 10;
    singleThreadSolving.keepNum = 50;
    singleThreadSolving.initialKeep = 500;
    singleThreadSolving.maxEllipse = 1.5;
    singleThreadSolving.convFilterType = SSolver::CONV_GAUSSIAN;
    singleThreadSolving.fwhm = 4;
    profileList.append(singleThreadSolving);

    Parameters parLargeSolving;
    parLargeSolving.listName = "3-LargeScaleSolving";
    parLargeSolving.description = "Profile intended for Plate Solving camera lens sized images";
    parLargeSolving.minwidth = 10;
    parLargeSolving.maxwidth = 180;
    parLargeSolving.keepNum = 50;
    parLargeSolving.initialKeep = 500;
    parLargeSolving.maxEllipse = 1.5;
    parLargeSolving.convFilterType = SSolver::CONV_GAUSSIAN;
    parLargeSolving.fwhm = 4;
    profileList.append(parLargeSolving);

    Parameters parSmallSolving;
    parSmallSolving.listName = "4-SmallScaleSolving";
    parSmallSolving.description = "Profile intended for Plate Solving telescopic sized images";
    parSmallSolving.minwidth = 0.1;
    parSmallSolving.maxwidth = 10;
    parSmallSolving.keepNum = 50;
    parSmallSolving.initialKeep = 500;
    parSmallSolving.maxEllipse = 1.5;
    parSmallSolving.convFilterType = SSolver::CONV_GAUSSIAN;
    parSmallSolving.fwhm = 4;
    profileList.append(parSmallSolving);

    Parameters allStars;
    allStars.listName = "5-AllStars";
    allStars.description = "Profile for the source extraction of all the stars in an image.";
    allStars.maxEllipse = 1.5;
    allStars.convFilterType = SSolver::CONV_GAUSSIAN;
    allStars.fwhm = 2;
    allStars.r_min = 2;
    profileList.append(allStars);

    Parameters smallStars;
    smallStars.listName = "6-SmallSizedStars";
    smallStars.description = "Profile optimized for source extraction of smaller stars.";
    smallStars.maxEllipse = 1.5;
    smallStars.convFilterType = SSolver::CONV_GAUSSIAN;
    smallStars.fwhm = 2;
    smallStars.r_min = 2;
    smallStars.maxSize = 5;
    smallStars.initialKeep = 500;
    smallStars.saturationLimit = 80;
    profileList.append(smallStars);

    Parameters mid;
    mid.listName = "7-MidSizedStars";
    mid.description = "Profile optimized for source extraction of medium sized stars.";
    mid.maxEllipse = 1.5;
    mid.minarea = 20;
    mid.convFilterType = SSolver::CONV_GAUSSIAN;
    mid.fwhm = 4;
    mid.r_min = 5;
    mid.removeDimmest = 20;
    mid.minSize = 2;
    mid.maxSize = 10;
    mid.initialKeep = 500;
    mid.saturationLimit = 80;
    profileList.append(mid);

    Parameters big;
    big.listName = "8-BigSizedStars";
    big.description = "Profile optimized for source extraction of larger stars.";
    big.maxEllipse = 1.5;
    big.minarea = 40;
    big.convFilterType = SSolver::CONV_GAUSSIAN;
    big.fwhm = 8;
    big.r_min = 20;
    big.minSize = 5;
    big.initialKeep = 500;
    big.removeDimmest = 50;
    profileList.append(big);

    return profileList;
}

QList<SSolver::Parameters> StellarSolver::loadSavedOptionsProfiles(QString savedOptionsProfiles)
{
    QList<SSolver::Parameters> optionsList;
    if(!QFileInfo::exists(savedOptionsProfiles))
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
        if(QFileInfo::exists(path))
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

bool StellarSolver::appendStarsRAandDEC(QList<FITSImage::Star> &stars)
{
    if(solverWithWCS)
        return solverWithWCS->appendStarsRAandDEC(stars);
    return false;
}

//This function should get the system RAM in bytes.  I may revise it later to get the currently available RAM
//But from what I read, getting the Available RAM is inconsistent and buggy on many systems.
bool StellarSolver::getAvailableRAM(double &availableRAM, double &totalRAM)
{
#if defined(Q_OS_OSX)
    int mib [] = { CTL_HW, HW_MEMSIZE };
    size_t length;
    length = sizeof(int64_t);
    int64_t RAMcheck;
    if(sysctl(mib, 2, &RAMcheck, &length, NULL, 0))
        return false; // On Error
    //Until I can figure out how to get free RAM on Mac
    availableRAM = RAMcheck;
    totalRAM = RAMcheck;
#elif defined(Q_OS_LINUX)
    QProcess p;
    p.start("awk", QStringList() << "/MemFree/ { print $2 }" << "/proc/meminfo");
    p.waitForFinished();
    QString memory = p.readAllStandardOutput();
    availableRAM = memory.toLong() * 1024.0; //It is in kB on this system

    p.start("awk", QStringList() << "/MemTotal/ { print $2 }" << "/proc/meminfo");
    p.waitForFinished();
    memory = p.readAllStandardOutput();
    totalRAM = memory.toLong() * 1024.0; //It is in kB on this system
    p.close();
#else
    MEMORYSTATUSEX memory_status;
    ZeroMemory(&memory_status, sizeof(MEMORYSTATUSEX));
    memory_status.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memory_status))
    {
        availableRAM = memory_status.ullAvailPhys;
        totalRAM = memory_status.ullTotalPhys;
    }
    else
    {
        return false;
    }
#endif
    return true;
}

//This should determine if enough RAM is available to load all the index files in parallel
bool StellarSolver::enoughRAMisAvailableFor(QStringList indexFolders)
{
    double totalSize = 0;

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
    double availableRAM = 0;
    double totalRAM = 0;
    getAvailableRAM(availableRAM, totalRAM);
    if(availableRAM == 0)
    {
        if(m_SSLogLevel != LOG_OFF)
            emit logOutput("Unable to determine system RAM for inParallel Option");
        return false;
    }
    double bytesInGB = 1024.0 * 1024.0 *
                       1024.0; // B -> KB -> MB -> GB , float to make sure it reports the answer with any decimals
    if(m_SSLogLevel != LOG_OFF)
    {
        emit logOutput(
            QString("Evaluating Installed RAM for inParallel Option.  Total Size of Index files: %1 GB, Installed RAM: %2 GB, Free RAM: %3 GB").arg(
                totalSize / bytesInGB).arg(totalRAM / bytesInGB).arg(availableRAM / bytesInGB));
#if defined(Q_OS_OSX)
        emit logOutput("Note: Free RAM for now is reported as Installed RAM on MacOS until I figure out how to get available RAM");
#endif
    }
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
