/*  StellarSolver, StellarSolver Internal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#include <QApplication>
#include <QSettings>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include "windows.h"
#else //Linux
#include <QProcess>
#endif
#include "externalextractorsolver.h"

#include "stellarsolver.h"
#include "extractorsolver.h"

#include "onlinesolver.h"


using namespace SSolver;

StellarSolver::StellarSolver(QObject *parent) : QObject(parent)
{
    registerMetaTypes();
}

StellarSolver::StellarSolver(const FITSImage::Statistic &imagestats, uint8_t const *imageBuffer,
                             QObject *parent) : QObject(parent)
{
    registerMetaTypes();
    loadNewImageBuffer(imagestats, imageBuffer);
}

StellarSolver::StellarSolver(ProcessType type, const FITSImage::Statistic &imagestats, const uint8_t *imageBuffer,
                             QObject *parent) : QObject(parent)
{
    registerMetaTypes();
    m_ProcessType = type;
    loadNewImageBuffer(imagestats, imageBuffer);
}

StellarSolver::~StellarSolver()
{
    // These lines make sure that before the StellarSolver is deleted, all parallel threads (if any) are shut down
    for(auto &solver : parallelSolvers)
      disconnect(solver, &ExtractorSolver::finished, this, &StellarSolver::finishParallelSolve);

    abortAndWait();
}

void StellarSolver::registerMetaTypes()
{
    qRegisterMetaType<SolverType>("SolverType");
    qRegisterMetaType<ProcessType>("ProcessType");
    qRegisterMetaType<ExtractorType>("ExtractorType");
}

bool StellarSolver::loadNewImageBuffer(const FITSImage::Statistic &imagestats, uint8_t const *imageBuffer)
{
    if(imageBuffer == nullptr)
        return false;
    if(isRunning())
        return false;
    m_ImageBuffer = imageBuffer;
    m_Statistics = imagestats;
    m_Subframe = QRect(0, 0, m_Statistics.width, m_Statistics.height);

    //information that should be reset since it was about the last image
    m_HasExtracted = false;
    m_HasSolved = false;
    m_HasFailed = false;
    hasWCS = false;
    m_isRunning = false;
    m_UsePosition = false;
    m_SearchRA = HUGE_VAL;
    m_SearchDE = HUGE_VAL;
    m_UseScale = false;
    m_ScaleHigh = 0;
    m_ScaleLow = 0;
    qDeleteAll(parallelSolvers);
    parallelSolvers.clear();
    m_ExtractorSolver.reset();
    m_ParallelSolversFinishedCount = 0;
    background = {};
    m_ExtractorStars.clear();
    m_SolverStars.clear();
    numStars = 0;
    solution = {};
    solutionIndexNumber = -1;
    solutionHealpix = -1;

    return true;
}

ExtractorSolver* StellarSolver::createExtractorSolver()
{
    ExtractorSolver *solver;

    if(m_ProcessType == SOLVE && m_SolverType == SOLVER_ONLINEASTROMETRY)
    {
        OnlineSolver *onlineSolver = new OnlineSolver(m_ProcessType, m_ExtractorType, m_SolverType, m_Statistics, m_ImageBuffer,
                this);
        onlineSolver->fileToProcess = m_FileToProcess;
        onlineSolver->astrometryAPIKey = m_AstrometryAPIKey;
        onlineSolver->astrometryAPIURL = m_AstrometryAPIURL;
        onlineSolver->externalPaths = m_ExternalPaths;
        solver = onlineSolver;
    }
    else if((m_ProcessType == SOLVE && m_SolverType == SOLVER_STELLARSOLVER) || (m_ProcessType != SOLVE
            && m_ExtractorType != EXTRACTOR_EXTERNAL))
        solver = new InternalExtractorSolver(m_ProcessType, m_ExtractorType, m_SolverType, m_Statistics, m_ImageBuffer, this);
    else
    {
        ExternalExtractorSolver *extSolver = new ExternalExtractorSolver(m_ProcessType, m_ExtractorType, m_SolverType,
                m_Statistics, m_ImageBuffer, this);
        extSolver->fileToProcess = m_FileToProcess;
        extSolver->externalPaths = m_ExternalPaths;
        extSolver->cleanupTemporaryFiles = m_CleanupTemporaryFiles;
        extSolver->autoGenerateAstroConfig = m_AutoGenerateAstroConfig;
        solver = extSolver;
    }

    if(useSubframe)
        solver->setUseSubframe(m_Subframe);
    solver->m_ColorChannel = m_ColorChannel;
    solver->m_LogToFile = m_LogToFile;
    solver->m_LogFileName = m_LogFileName;
    solver->m_AstrometryLogLevel = m_AstrometryLogLevel;
    solver->m_SSLogLevel = m_SSLogLevel;
    solver->m_BasePath = m_BasePath;
    solver->m_ActiveParameters = params;
    solver->convFilter = convFilter;
    solver->indexFolderPaths = indexFolderPaths;
    solver->indexFiles = m_IndexFilePaths;
    if(m_UseScale)
        solver->setSearchScale(m_ScaleLow, m_ScaleHigh, m_ScaleUnit);
    if(m_UsePosition)
        solver->setSearchPositionInDegrees(m_SearchRA, m_SearchDE);
    if(m_AstrometryLogLevel != SSolver::LOG_NONE || m_SSLogLevel != SSolver::LOG_OFF)
        connect(solver, &ExtractorSolver::logOutput, this, &StellarSolver::logOutput);

    return solver;
}

ExternalProgramPaths StellarSolver::getDefaultExternalPaths(ComputerSystemType system)
{
    return ExternalExtractorSolver::getDefaultExternalPaths(system);
};

ExternalProgramPaths StellarSolver::getDefaultExternalPaths()
{
#if defined(Q_OS_MACOS)
    return getDefaultExternalPaths(MAC_HOMEBREW);
#elif defined(Q_OS_LINUX)
    return getDefaultExternalPaths(LINUX_DEFAULT);
#else //Windows
    return getDefaultExternalPaths(WIN_ANSVR);
#endif
}

QStringList StellarSolver::getIndexFiles(const QStringList &directoryList, int indexToUse, int healpixToUse)
{
    QStringList indexFileList;
    for(int i = 0; i < directoryList.count(); i++)
    {
        const QString &currentPath = directoryList[i];
        QDir dir(currentPath);
        QStringList dirIndexFiles;
        if(dir.exists())
        {
            if(indexToUse < 0)
            {
                // Find all fits files in the folder.
                dir.setNameFilters(QStringList() << "*.fits" << "*.fit");
                dirIndexFiles << dir.entryList();
            }
            else
            {
                QString name1, name2;
                if (healpixToUse >= 0)
                {
                    // Find all the fits files in the folder associated with the index and healpix.
                    QString hStr = QString("%1").arg(healpixToUse, 2, 10, (QChar) '0');
                    name1 = "index-" + QString::number(indexToUse) + "-" + hStr + ".fits";
                    name2 = "index-" + QString::number(indexToUse) + "-" + hStr + ".fit";
                }
                else
                {
                    // Find all the fits files in the folder associated with the index number.
                    name1 = "index-" + QString::number(indexToUse) + "*.fits";
                    name2 = "index-" + QString::number(indexToUse) + "*.fit";
                }
                dir.setNameFilters(QStringList() << name1 << name2);
                dirIndexFiles << dir.entryList();
            }
            for(int i = 0; i < dirIndexFiles.count(); i++)
            {
               indexFileList.append(dir.absolutePath() + QDir::separator() + dirIndexFiles.at(i));
            }
        }
    }
    return indexFileList;
}

bool StellarSolver::extract(bool calculateHFR, QRect frame)
{
    m_ProcessType = calculateHFR ? EXTRACT_WITH_HFR : EXTRACT;
    useSubframe = !frame.isNull() && frame.isValid();
    if (useSubframe)
        m_Subframe = frame;

    // This loop will wait syncrounously for the finished signal that the star extraction is done
    QEventLoop loop;
    connect(this, &StellarSolver::finished, &loop, &QEventLoop::quit);
    start();
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    return m_HasExtracted;
}

bool StellarSolver::solve()
{
    m_ProcessType = SOLVE;

    // This loop will wait syncrounously for the finished signal that the solving is done
    QEventLoop loop;
    connect(this, &StellarSolver::finished, &loop, &QEventLoop::quit);
    start();
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    return m_HasSolved;
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

    //This is necessary before starting up so that the correct convolution filter gets passed to the ExtractorSolver
    updateConvolutionFilter();

    m_ExtractorSolver.reset(createExtractorSolver());

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
        if(m_ExtractorType != EXTRACTOR_BUILTIN)
        {
            m_ExtractorSolver->extract();
            if(m_ExtractorSolver->getNumStarsFound() == 0)
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
        if(m_SolverType == SOLVER_LOCALASTROMETRY && m_ExtractorType == EXTRACTOR_BUILTIN)
        {
            ExternalExtractorSolver *extSolver = static_cast<ExternalExtractorSolver*> (m_ExtractorSolver.data());
            QFileInfo file(extSolver->fileToProcess);
            int ret = extSolver->saveAsFITS();
            if(ret != 0)
            {
                emit logOutput("Failed to save FITS File.");
                return;
            }
        }
        // There is no reason to generate a bunch of copies of the config file, just one will do for all the parallel threads.
        if(m_SolverType == SOLVER_LOCALASTROMETRY)
        {
            ExternalExtractorSolver *extSolver = static_cast<ExternalExtractorSolver*> (m_ExtractorSolver.data());
            extSolver->generateAstrometryConfigFile();
        }
        parallelSolve();
    }
    else if(m_SolverType == SOLVER_ONLINEASTROMETRY)
    {
        OnlineSolver *onSolver = static_cast<OnlineSolver*> (m_ExtractorSolver.data());
        int ret = onSolver->saveAsFITS();
        if(ret != 0)
        {
            emit logOutput("Failed to save FITS File.");
            return;
        }
        connect(m_ExtractorSolver.data(), &ExtractorSolver::finished, this, &StellarSolver::processFinished);
        m_ExtractorSolver->execute();
    }
    else
    {
        connect(m_ExtractorSolver.data(), &ExtractorSolver::finished, this, &StellarSolver::processFinished);
        m_ExtractorSolver->start();
    }

}

bool StellarSolver::checkParameters()
{
    if(m_ImageBuffer == nullptr)
    {
        emit logOutput("The image buffer is not loaded, please load an image before processing it.");
        return false;
    }
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if(params.partition == true)
        {
            emit logOutput("There is a crash sometimes on QT6 when using partitioning. You are using QT6, so I am disabling partitioning to prevent a crash.");
            params.partition = false;
        }
    #endif

    if(m_ProcessType == SOLVE && m_SolverType == SOLVER_WATNEYASTROMETRY && (m_Statistics.dataType == SEP_TFLOAT || m_Statistics.dataType == SEP_TDOUBLE))
    {
        emit logOutput("The Watney Solver cannot solve floating point images.");
        return false;
    }

    if(m_SolverType == SOLVER_ASTAP && m_ExtractorType != EXTRACTOR_BUILTIN)
    {
        if(m_SSLogLevel != LOG_OFF)
            emit logOutput("ASTAP no longer supports alternative star extraction methods.  Changing to built-in star extraction.");
        m_ExtractorType = EXTRACTOR_BUILTIN;
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
        if(m_SolverType == SOLVER_STELLARSOLVER && m_ExtractorType != EXTRACTOR_INTERNAL)
        {
            if(m_SSLogLevel != LOG_OFF)
                emit logOutput("StellarSolver only uses the Internal SEP Star Extractor since it doesn't save files to disk. Changing to Internal Star Extractor.");
            m_ExtractorType = EXTRACTOR_INTERNAL;
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

        if(m_ProcessType == SOLVE && m_SolverType == SOLVER_WATNEYASTROMETRY && params.keepNum < 300)
        {
            emit logOutput("The Watney Solver needs at least 300 stars. Adjusting keepNum to 300");
            params.keepNum = 300;
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

    return true;
}

//This allows us to start multiple threads to search simulaneously in separate threads/cores
//to attempt to efficiently use modern multi core computers to speed up the solve
void StellarSolver::parallelSolve()
{
    if(params.multiAlgorithm == NOT_MULTI || !(m_SolverType == SOLVER_STELLARSOLVER || m_SolverType == SOLVER_LOCALASTROMETRY))
        return;
    qDeleteAll(parallelSolvers);
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
            ExtractorSolver *solver = m_ExtractorSolver->spawnChildSolver(thread);
            connect(solver, &ExtractorSolver::finished, this, &StellarSolver::finishParallelSolve);
            if (m_ProcessType == SOLVE && m_SolverType == SOLVER_STELLARSOLVER &&
                m_ExtractorSolver->m_ActiveParameters.downsample != 1 &&
                m_ExtractorSolver->scaleunit == ARCSEC_PER_PIX)
            {
                // The main solver downsamples and accouts for this in the search scale
                // but the spawned solvers also need that accounting.
                double d = m_ExtractorSolver->m_ActiveParameters.downsample;
                solver->setSearchScale(low*d, high*d, units);
            }
            else
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
            ExtractorSolver *solver = m_ExtractorSolver->spawnChildSolver(i);
            connect(solver, &ExtractorSolver::finished, this, &StellarSolver::finishParallelSolve);
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
    for(const auto &solver : parallelSolvers)
        if(solver->isRunning())
            return true;
    return false;
}
void StellarSolver::processFinished(int code)
{
    numStars  = m_ExtractorSolver->getNumStarsFound();
    if(code == 0)
    {
        if(m_ProcessType == SOLVE && m_ExtractorSolver->solvingDone())
        {
            solution = m_ExtractorSolver->getSolution();
            solutionIndexNumber = m_ExtractorSolver->getSolutionIndexNumber();
            solutionHealpix = m_ExtractorSolver->getSolutionHealpix();
            m_SolverStars = m_ExtractorSolver->getStarList();
            if(m_ExtractorSolver->hasWCSData())
            {
                hasWCS = true;
                wcsData = m_ExtractorSolver->getWCSData();
                wcsData = m_ExtractorSolver->getWCSData();
                if(m_ExtractorStars.count() > 0)
                    wcsData.appendStarsRAandDEC(m_ExtractorStars);
            }
            m_HasSolved = true;
        }
        else if((m_ProcessType == EXTRACT || m_ProcessType == EXTRACT_WITH_HFR) && m_ExtractorSolver->extractionDone())
        {
            m_ExtractorStars = m_ExtractorSolver->getStarList();
            background = m_ExtractorSolver->getBackground();
            m_CalculateHFR = m_ExtractorSolver->isCalculatingHFR();
            if(hasWCS)
                wcsData.appendStarsRAandDEC(m_ExtractorStars);
            m_HasExtracted = true;
        }
    }
    else
        m_HasFailed = true;

    m_isRunning = false;

    emit ready();
    emit finished();
}

int StellarSolver::whichSolver(ExtractorSolver *solver)
{
    for(int i = 0; i < parallelSolvers.count(); i++ )
    {
        if(parallelSolvers.at(i) == solver)
            return i + 1;
    }
    return 0;
}

//This slot listens for signals from the child solvers that they are in fact done with the solve
void StellarSolver::finishParallelSolve(int success)
{
    bool emitReady = false;
    bool emitFinished = false;

    m_ParallelSolversFinishedCount++;
    ExtractorSolver *reportingSolver = qobject_cast<ExtractorSolver*>(sender());
    if(!reportingSolver)
        return;

    if(success == 0 && !m_HasSolved)
    {
        for(auto &solver : parallelSolvers)
        {
            disconnect(solver, &ExtractorSolver::logOutput, this, &StellarSolver::logOutput);
            if(solver != reportingSolver && solver->isRunning())
                solver->abort();
        }
        if(m_AstrometryLogLevel != SSolver::LOG_NONE || m_SSLogLevel != SSolver::LOG_OFF)
        {
            for(auto solver : parallelSolvers)
                disconnect(solver, &ExtractorSolver::logOutput, m_ExtractorSolver.data(), &ExtractorSolver::logOutput);
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
            wcsData = reportingSolver->getWCSData();
            hasWCS = true;
            if(m_ExtractorStars.count() > 0)
                wcsData.appendStarsRAandDEC(m_ExtractorStars);
            m_isRunning = false;
        }
        m_HasSolved = true;
        m_ExtractorSolver->cleanupTempFiles();
        emitReady = true;
    }
    else
    {
        disconnect(reportingSolver, &ExtractorSolver::finished, this, &StellarSolver::finishParallelSolve);
        if(m_AstrometryLogLevel != SSolver::LOG_NONE || m_SSLogLevel != SSolver::LOG_OFF)
            disconnect(reportingSolver, &ExtractorSolver::logOutput, m_ExtractorSolver.data(), &ExtractorSolver::logOutput);
        if(m_SSLogLevel != LOG_OFF && !m_HasSolved)
            emit logOutput(QString("Child solver: %1 did not solve or was aborted").arg(whichSolver(reportingSolver)));
    }

    if(m_ParallelSolversFinishedCount == parallelSolvers.count())
    {
        m_isRunning = false;
        if(!m_HasSolved){
            m_HasFailed = true;
            emitReady = true; //Since this was emitted earlier if it had been solved
        }
        qDeleteAll(parallelSolvers);
        parallelSolvers.clear();
        m_ExtractorSolver->cleanupTempFiles();
        emitFinished=true;
    }

    if (emitReady) emit ready();
    if (emitFinished) emit finished();
}

QString StellarSolver::raString(double ra)
{
    char rastr[32];
    ra2hmsstring(ra, rastr);
    return rastr;
}

QString StellarSolver::decString(double dec)
{
    char decstr[32];
    dec2dmsstring(dec, decstr);
    return decstr;
}

bool StellarSolver::wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint)
{
    if(hasWCS)
    {
        wcsData.wcsToPixel(skyPoint, pixelPoint);
        return true;
    }
    return false;
}

bool StellarSolver::pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint)
{
    if(hasWCS)
    {
        wcsData.pixelToWCS(pixelPoint, skyPoint);
        return true;
    }
    return false;
}

//This is the abort method.  It works in different ways for the different solvers.
void StellarSolver::abort()
{
  for(auto &solver : parallelSolvers)
      solver->abort();
  if(m_ExtractorSolver)
      m_ExtractorSolver->abort();
}

//This is the abort and wait method, it is useful if you want the solver to be all shut down before moving on
void StellarSolver::abortAndWait()
{
  abort();
  for(auto &solver : parallelSolvers)
      solver->wait();
  if(m_ExtractorSolver)
      m_ExtractorSolver->wait();
}

//This method checks all the solvers and the internal running boolean to determine if anything is running.
bool StellarSolver::isRunning() const
{
    if(parallelSolversAreRunning())
        return true;
    if(m_ExtractorSolver && m_ExtractorSolver->isRunning())
        return true;
    return m_isRunning;
}

//This method uses a fwhm value to generate the conv filter the star extractor will use.
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
                double value = amplitude * exp( ( -4.0 * log(2.0) * pow(sqrt( pow(x, 2) + pow(y, 2) ), 2) ) / pow(size * 1.5, 2));
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

QList<SSolver::Parameters> StellarSolver::loadSavedOptionsProfiles(const QString &savedOptionsProfiles)
{
    QList<SSolver::Parameters> optionsList;
    if(!QFileInfo::exists(savedOptionsProfiles))
    {
        return StellarSolver::getBuiltInProfiles();
    }
    QSettings settings(savedOptionsProfiles, QSettings::IniFormat);
    QStringList groups = settings.childGroups();
    foreach(const QString &group, groups)
    {
        settings.beginGroup(group);
        QStringList keys = settings.childKeys();
        QMap<QString, QVariant> map;
        foreach(const QString &key, keys)
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
#if defined(Q_OS_MACOS)
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
    if(hasWCS)
        return wcsData.appendStarsRAandDEC(stars);
    return false;
}

//This function should get the system RAM in bytes.  I may revise it later to get the currently available RAM
//But from what I read, getting the Available RAM is inconsistent and buggy on many systems.
bool StellarSolver::getAvailableRAM(double &availableRAM, double &totalRAM)
{
#if defined(Q_OS_MACOS)
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
bool StellarSolver::enoughRAMisAvailableFor(const QStringList &indexFolders)
{
    double totalSize = 0;

    foreach(const QString &folder, indexFolders)
    {
        QDir dir(folder);
        if(dir.exists())
        {
            dir.setNameFilters(QStringList() << "*.fits" << "*.fit");
            QFileInfoList indexInfoList = dir.entryInfoList();
            foreach(const QFileInfo &indexInfo, indexInfoList)
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
#if defined(Q_OS_MACOS)
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
