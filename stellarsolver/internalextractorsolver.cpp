/*  InternalExtractorSolver, StellarSolver Internal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#if defined(__APPLE__)
#include <sys/stat.h>
#elif defined(_WIN32)
#define NOMINMAX
#include "windows.h"
#else //Linux
#include <sys/stat.h>
#endif

#include <memory>

#include "internalextractorsolver.h"
#include "stellarsolver.h"
#include "sep/extract.h"
#include "qmath.h"
#include <QMutexLocker>

//CFitsio Includes
#include <fitsio.h>

//Astrometry.net includes
extern "C" {
#include "astrometry/log.h"
#include "astrometry/sip-utils.h"
}

using namespace SSolver;
using namespace SEP;

static int solverNum = 1;

InternalExtractorSolver::InternalExtractorSolver(ProcessType pType, ExtractorType eType, SolverType sType,
        const FITSImage::Statistic &imagestats, uint8_t const *imageBuffer, QObject *parent) : ExtractorSolver(pType, eType, sType,
                    imagestats, imageBuffer, parent)
{
    //This sets the base name used for the temp files.
    m_BaseName = "internalExtractorSolver_" + QString::number(solverNum++);
    m_PartitionThreads = QThread::idealThreadCount();
}

InternalExtractorSolver::~InternalExtractorSolver()
{
    waitSEP(); // Just in case it has not shut down
    if(downSampledBuffer)
    {
        delete [] downSampledBuffer;
        downSampledBuffer = 0;
    }
    if(mergedChannelBuffer)
    {
        delete [] mergedChannelBuffer;
        mergedChannelBuffer = 0;
    }
    if(isRunning())
    {
        quit();
        requestInterruption();
        wait();
    }
}

//This is the abort method.  For the internal solver it sets a cancel variable. It quits the thread.  And it cancels any SEP threads that are in progress.
void InternalExtractorSolver::abort()
{
    waitSEP();
    quit();

    thejob.bp.cancelled = TRUE;
    if(!isChildSolver)
        emit logOutput("Aborting...");
    m_WasAborted = true;
}

void InternalExtractorSolver::waitSEP()
{
    QMutexLocker locker(&futuresMutex);
    if (futures.empty())
        return;

    for (auto &oneFuture : futures)
    {
        if(oneFuture.isRunning())
            oneFuture.waitForFinished();
    }

    futures.clear();
}

//This method generates child solvers with the options of the current solver
ExtractorSolver* InternalExtractorSolver::spawnChildSolver(int n)
{
    Q_UNUSED(n);

    InternalExtractorSolver *solver = new InternalExtractorSolver(m_ProcessType, m_ExtractorType, m_SolverType, m_Statistics,
            m_ImageBuffer, nullptr);
    solver->setParent(this->parent());  //This makes the parent the StellarSolver
    solver->m_ExtractedStars = m_ExtractedStars;
    solver->m_BasePath = m_BasePath;
    //They will all share the same basename
    solver->m_HasExtracted = true;
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
        connect(solver, &ExtractorSolver::logOutput, this,  &ExtractorSolver::logOutput);
    solver->usingDownsampledImage = usingDownsampledImage;
    solver->m_ColorChannel = m_ColorChannel;
    return solver;
}

int InternalExtractorSolver::extract()
{
    return(runSEPExtractor());
}

//This is the method that runs the solver or star extractor.  Do not call it, use the methods above instead, so that it can start a new thread.
void InternalExtractorSolver::run()
{
    if(m_AstrometryLogLevel != SSolver::LOG_NONE && m_LogToFile)
    {
        if(m_LogFileName == "")
            m_LogFileName = m_BasePath + "/" + m_BaseName + ".log.txt";
        if(QFile(m_LogFileName).exists())
            QFile(m_LogFileName).remove();
    }

    switch(m_ProcessType)
    {
        case EXTRACT:
        case EXTRACT_WITH_HFR:
        {
            int result = extract();
            emit finished(result);
        }
        break;

        case SOLVE:
        {
            if(!m_HasExtracted)
            {
                extract();
                if(m_ExtractedStars.size() == 0)
                {
                    emit logOutput("No stars were found, so the image cannot be solved");
                    cleanupTempFiles();
                    emit finished(-1);
                    return;
                }
            }
            if(m_HasExtracted)
            {
                int result = runInternalSolver();
                cleanupTempFiles();
                emit finished(result);
            }
            else
            {
                cleanupTempFiles();
                emit finished(-1);
            }
        }
        break;

        default:
            break;
    }
}

void InternalExtractorSolver::cleanupTempFiles()
{
    //There are NO temp files anymore for the internal SEP or Astrometry builds!!!
}

bool InternalExtractorSolver::allocateDataBuffer(float *data, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    switch (m_Statistics.dataType)
    {
        case SEP_TBYTE:
            return getFloatBuffer<uint8_t>(data, x, y, w, h);
        case TSHORT:
            return getFloatBuffer<int16_t>(data, x, y, w, h);
        case TUSHORT:
            return getFloatBuffer<uint16_t>(data, x, y, w, h);
        case TLONG:
            return getFloatBuffer<int32_t>(data, x, y, w, h);
        case TULONG:
            return getFloatBuffer<uint32_t>(data, x, y, w, h);
        case TFLOAT:
            return getFloatBuffer<float>(data, x, y, w, h);
        case TDOUBLE:
            return getFloatBuffer<double>(data, x, y, w, h);
        default:
            delete [] data;
            data = 0;
            return false;
    }

    return false;
}

namespace
{

// This function adds a margin around the rectangle with corners x1,y1 x2,y2 for the image
// with the given width and height, making sure the margin doesn't go outside the image.
// It returns the expanded rectangle defined by corner startX,startY and width, height.
void computeMargin(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
                   uint32_t imageWidth, uint32_t imageHeight, uint32_t margin,
                   uint32_t *startX, uint32_t *startY, uint32_t *width, uint32_t *height)
{
    // Figure out the start and start margins.
    // Make sure these are signed operations.
    int tempStartX = ((int) x1) - ((int)margin);
    if (tempStartX < 0)
    {
        tempStartX = 0;
    }
    *startX = tempStartX;

    int tempStartY = ((int) y1) - ((int)margin);
    if (tempStartY < 0)
    {
        tempStartY = 0;
    }
    *startY = tempStartY;

    // Figure out the end and end margins.
    uint32_t endX = x2 + margin;
    if (endX >= imageWidth) endX = imageWidth - 1;
    uint32_t endY = y2 + margin;
    if (endY >= imageHeight) endY = imageHeight - 1;

    *width = endX - *startX + 1;
    *height = endY - *startY + 1;
}

}  // namespace

//The code in this section is my attempt at running an internal star extractor program based on SEP
//I used KStars and the SEP website as a guide for creating these functions
int InternalExtractorSolver::runSEPExtractor()
{
    QMutexLocker locker(&futuresMutex);
    if(convFilter.size() == 0)
    {
        emit logOutput("No convFilter included.");
        return -1;
    }

    // Double check nothing else is running.
    //waitSEP();

    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting Internal StellarSolver Star Extractor with the " + m_ActiveParameters.listName + " profile . . .");
    //Only merge image channels if it is an RGB image and we are either averaging or integrating the channels
    if(m_Statistics.channels == 3 && (m_ColorChannel == FITSImage::AVERAGE_RGB || m_ColorChannel == FITSImage::INTEGRATED_RGB))
    {
        if (mergeImageChannels() == false)
        {
            emit logOutput("Merging image channels failed.");
            return -1;
        }
    }
    //Only downsample images before SEP if the Sextraction is being used for plate solving
    if(m_ProcessType == SOLVE && m_SolverType == SOLVER_STELLARSOLVER && m_ActiveParameters.downsample != 1)
    {
        if (downsampleImage(m_ActiveParameters.downsample) == false)
        {
            emit logOutput("Downsampling failed.");
            return -1;
        }
    }
    uint32_t x = 0, y = 0;
    uint32_t w = m_Statistics.width, h = m_Statistics.height;
    uint32_t raw_w = m_Statistics.width, raw_h = m_Statistics.height;
    if(m_UseSubframe && m_SubFrameRect.isValid())
    {
        // JM 2021-08-21 Max sure frame is within acceptable parameters.
        x = std::max(0, m_SubFrameRect.x());
        w = std::min(static_cast<int>(raw_w), m_SubFrameRect.width());
        y = std::max(0, m_SubFrameRect.y());
        h = std::min(static_cast<int>(raw_h), m_SubFrameRect.height());

    }

    // This data structure defines partitions of the full image processed to parallelize computation.
    // startX and startY define the x,y coordinates in the full image where this partition starts.
    // innerStartX and Y, and innerEndX and Y are the corners of the image patch of interest,
    // i.e. not including the margins. The endpoints are inclusive. Later, we will not include
    // stars detected in the margins, e.g. points where x < innerStartX or x > innerEndX.
    // Similar for Y.
    class StartupOffset
    {
        public:
            int startX = 0, startY = 0, width = 0, height = 0;
            int innerStartX = 0, innerStartY = 0, innerEndX = 0, innerEndY = 0;
            StartupOffset(int x, int y, int w, int h, int inX1, int inY1, int inX2, int inY2)
                : startX(x), startY(y), width(w), height(h),
                  innerStartX(inX1), innerStartY(inY1), innerEndX(inX2), innerEndY(inY2) {}
    };

    QVector<float *> dataBuffers;
    QList<StartupOffset> startupOffsets;
    QList<FITSImage::Background> backgrounds;

    // The margin is extra image placed around partitions, so we can detect large stars near
    // the edges of the partitions. The margin size needs to be about half the size of a star to
    // be detected, since the other half of the star would be internal to the partition.
    // Below determines the margin size used.  If m_ActiveParameters.maxSize == 0, that means that the max
    // star size is unspecified.  In this case we use a margin of 10, so stars of size > 20 may be missed
    // on the edge of a partition. If the max-star size is given very large, we limit the size of the margin
    // used to 50 (e.g. corresponding to a 100-pixel-wide star).
    int DEFAULT_MARGIN = m_ActiveParameters.maxSize / 2;
    if (DEFAULT_MARGIN <= 20)
        DEFAULT_MARGIN = 20;
    else if (DEFAULT_MARGIN > 50)
        DEFAULT_MARGIN = 50;

    // Only partition if:
    // We have 2 or more threads.
    // The image width and height is larger than partition size.
    constexpr int PARTITION_SIZE = 200;
    if (m_ActiveParameters.partition &&  w > PARTITION_SIZE && h > PARTITION_SIZE && (m_PartitionThreads % 2) == 0)
    {
        // Partition the image to regions.
        // If there is extra at the end, we add an offset.
        // e.g. 500x400 image with patitions sized 200x200 would have 4 paritions
        // #1 0, 0, 200, 200 (200 x 200)
        // #2 200, 0, 200 + 100, 200 (300 x 200)
        // #3 0, 200, 200, 200 (200 x 200)
        // #4 200, 200, 200 + 100, 200 (300 x 200)

        int W_PARTITION_SIZE = PARTITION_SIZE;
        int H_PARTITION_SIZE = PARTITION_SIZE;

        // Limit it to PARTITION_THREADS
        if ( (w * h) / (W_PARTITION_SIZE * H_PARTITION_SIZE) > m_PartitionThreads)
        {
            W_PARTITION_SIZE = w / (m_PartitionThreads / 2);
            H_PARTITION_SIZE = h / 2;
        }

        int horizontalPartitions = w / W_PARTITION_SIZE;
        int verticalPartitions = h / H_PARTITION_SIZE;
        int horizontalOffset = w - (W_PARTITION_SIZE * horizontalPartitions);
        int verticalOffset = h - (H_PARTITION_SIZE * verticalPartitions);

        for (int i = 0; i < verticalPartitions; i++)
        {
            for (int j = 0; j < horizontalPartitions; j++)
            {
                int offsetW = (j == horizontalPartitions - 1) ? horizontalOffset : 0;
                int offsetH = (i == verticalPartitions - 1) ? verticalOffset : 0;

                const uint32_t rawStartX = x + j * W_PARTITION_SIZE;
                const uint32_t rawStartY = y + i * H_PARTITION_SIZE;
                const uint32_t rawEndX = rawStartX + W_PARTITION_SIZE + offsetW;
                const uint32_t rawEndY = rawStartY + H_PARTITION_SIZE + offsetH;

                uint32_t startX, startY, subWidth, subHeight;
                computeMargin(rawStartX, rawStartY, rawEndX - 1, rawEndY - 1,
                              m_Statistics.width, m_Statistics.height, DEFAULT_MARGIN,
                              &startX, &startY, &subWidth, &subHeight);

                startupOffsets.append(StartupOffset(startX, startY, subWidth, subHeight,
                                                    rawStartX, rawStartY, rawEndX - 1, rawEndY - 1));

                auto *data = new float[subWidth * subHeight];
                if (allocateDataBuffer(data, startX, startY, subWidth, subHeight) == false)
                {
                    for (auto *buffer : dataBuffers)
                        delete [] buffer;
                    emit logOutput("Failed to allocate memory.");
                    return -1;
                }
                dataBuffers.append(data);
                FITSImage::Background tempBackground;
                backgrounds.append(tempBackground);

                ImageParams parameters = {data,
                                          subWidth,
                                          subHeight,
                                          0,
                                          0,
                                          subWidth,
                                          subHeight,
                                          m_ActiveParameters.initialKeep / m_PartitionThreads,
                                          &backgrounds[backgrounds.size() - 1]
                                         };
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                futures.append(QtConcurrent::run(this, &InternalExtractorSolver::extractPartition, parameters));
#else
                futures.append(QtConcurrent::run(&InternalExtractorSolver::extractPartition, this, parameters));
#endif
            }
        }
    }
    else
    {
        // In this case, there is no partitioning, but it is still possible that margins apply.
        // E.g. a subframe rectangle with enough space around it to have margins.
        uint32_t startX, startY, subWidth, subHeight;
        computeMargin(x, y, x + w - 1, y + h - 1, m_Statistics.width, m_Statistics.height, DEFAULT_MARGIN,
                      &startX, &startY, &subWidth, &subHeight);

        auto *data = new float[subWidth * subHeight];
        if (allocateDataBuffer(data, startX, startY, subWidth, subHeight) == false)
        {
            for (auto *buffer : dataBuffers)
                delete [] buffer;
            emit logOutput("Failed to allocate memory.");
            return -1;
        }
        dataBuffers.append(data);
        startupOffsets.append(StartupOffset(startX, startY, subWidth, subHeight, x, y, x + w - 1, y + h - 1));
        FITSImage::Background tempBackground;
        backgrounds.append(tempBackground);

        ImageParams parameters = {data, subWidth, subHeight, 0, 0, subWidth, subHeight, static_cast<uint32_t>(m_ActiveParameters.initialKeep), &backgrounds[backgrounds.size() - 1]};
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        futures.append(QtConcurrent::run(this, &InternalExtractorSolver::extractPartition, parameters));
#else
        futures.append(QtConcurrent::run(&InternalExtractorSolver::extractPartition, this, parameters));
#endif
    }

    for (auto &oneFuture : futures)
    {
        oneFuture.waitForFinished();
        QList<FITSImage::Star> partitionStars = oneFuture.result();
        QList<FITSImage::Star> acceptedStars;
        if (!startupOffsets.empty())
        {
            const StartupOffset oneOffset = startupOffsets.takeFirst();
            const int startX = oneOffset.startX;
            const int startY = oneOffset.startY;
            for (auto &oneStar : partitionStars)
            {
                // Don't use stars from the margins (they're detected in other partitions).
                if (oneStar.x < (oneOffset.innerStartX - startX) ||
                        oneStar.y < (oneOffset.innerStartY - startY) ||
                        oneStar.x > (oneOffset.innerEndX   - startX) ||
                        oneStar.y > (oneOffset.innerEndY   - startY))
                    continue;
                oneStar.x += startX;
                oneStar.y += startY;
                acceptedStars.append(oneStar);
            }
        }
        m_ExtractedStars.append(acceptedStars);
    }

    double sumGlobal = 0, sumRmsSq = 0;
    for (const auto &bg : qAsConst(backgrounds))
    {
        sumGlobal += bg.global;
        sumRmsSq += bg.globalrms * bg.globalrms;
    }
    if (!backgrounds.empty())
    {
        m_Background.bw = backgrounds[0].bw;
        m_Background.bh = backgrounds[0].bh;
    }
    m_Background.num_stars_detected = m_ExtractedStars.size();
    m_Background.global = sumGlobal / backgrounds.size();
    m_Background.globalrms = sqrt( sumRmsSq / backgrounds.size() );

    applyStarFilters(m_ExtractedStars);

    for (auto * buffer : dataBuffers)
        delete [] buffer;
    dataBuffers.clear();
    futures.clear();

    m_HasExtracted = true;

    return 0;
}

QList<FITSImage::Star> InternalExtractorSolver::extractPartition(const ImageParams &parameters)
{
    float *imback = nullptr;
    double *fluxerr = nullptr, *area = nullptr;
    short *flag = nullptr;
    int status = 0;
    sep_bkg *bkg = nullptr;
    sep_catalog * catalog = nullptr;
    QList<FITSImage::Star> partitionStars;
    const uint32_t maxRadius = 50;

    auto cleanup = [ & ]()
    {
        sep_bkg_free(bkg);
        bkg = nullptr;
        Extract::sep_catalog_free(catalog);
        catalog = nullptr;
        free(imback);
        imback = nullptr;
        free(fluxerr);
        fluxerr = nullptr;
        free(area);
        area = nullptr;
        free(flag);
        flag = nullptr;

        if (status != 0)
        {
            char errorMessage[512];
            sep_get_errmsg(status, errorMessage);
            emit logOutput(errorMessage);
        }
    };

    //These are for the HFR
    double requested_frac[2] = { 0.5, 0.99 };
    double flux_fractions[2] = {0};
    short flux_flag = 0;
    std::vector<std::pair<int, double>> ovals;
    int numToProcess = 0;

    // #0 Create SEP Image structure
    sep_image im = {parameters.data,
                    nullptr,
                    nullptr,
                    nullptr,
                    SEP_TFLOAT,
                    0,
                    0,
                    0,
                    static_cast<int>(parameters.width),
                    static_cast<int>(parameters.height),
                    static_cast<int>(parameters.subW),
                    static_cast<int>(parameters.subH),
                    0,
                    SEP_NOISE_NONE,
                    1.0,
                    0
                   };

    // #1 Background estimate
    status = sep_background(&im, 64, 64, 3, 3, 0.0, &bkg);
    if (status != 0)
    {
        cleanup();
        return partitionStars;
    }

    // #2 Background evaluation
    imback = (float *)malloc((parameters.subW * parameters.subH) * sizeof(float));
    status = sep_bkg_array(bkg, imback, SEP_TFLOAT);
    if (status != 0)
    {
        cleanup();
        return partitionStars;
    }

    //Saving some background information
    parameters.background->bh = bkg->bh;
    parameters.background->bw = bkg->bw;
    parameters.background->global = bkg->global;
    parameters.background->globalrms = bkg->globalrms;

    // #3 Background subtraction
    status = sep_bkg_subarray(bkg, im.data, im.dtype);
    if (status != 0)
    {
        cleanup();
        return partitionStars;
    }

    std::unique_ptr<Extract> extractor;
    extractor.reset(new Extract());
    // #4 Source Extraction
    // Note that we set deblend_cont = 1.0 to turn off deblending.
    const double extractionThreshold = m_ActiveParameters.threshold_bg_multiple * bkg->globalrms +
                                       m_ActiveParameters.threshold_offset;
    //fprintf(stderr, "Using %.1f =  %.1f * %.1f + %.1f\n", extractionThreshold, m_ActiveParameters.threshold_bg_multiple, bkg->globalrms,  m_ActiveParameters.threshold_offset);
    status = extractor->sep_extract(&im, extractionThreshold, SEP_THRESH_ABS, m_ActiveParameters.minarea,
                                    convFilter.data(),
                                    sqrt(convFilter.size()), sqrt(convFilter.size()), SEP_FILTER_CONV,
                                    m_ActiveParameters.deblend_thresh,
                                    m_ActiveParameters.deblend_contrast, m_ActiveParameters.clean, m_ActiveParameters.clean_param, &catalog);
    if (status != 0)
    {
        cleanup();
        return partitionStars;
    }

    // Record the number of stars detected.
    parameters.background->num_stars_detected = catalog->nobj;

    // Find the oval sizes for each detection in the detected star catalog, and sort by that. Oval size
    // correlates very well with HFR and likely magnitude.
    for (int i = 0; i < catalog->nobj; i++)
    {
        const double ovalSizeSq = catalog->a[i] * catalog->a[i] + catalog->b[i] * catalog->b[i];
        ovals.push_back(std::pair<int, double>(i, ovalSizeSq));
    }
    if(catalog->nobj > 0)
        std::sort(ovals.begin(), ovals.end(), [](const std::pair<int, double> &o1, const std::pair<int, double> &o2) -> bool { return o1.second > o2.second;});

    numToProcess = std::min(static_cast<uint32_t>(catalog->nobj), parameters.keep);
    for (int index = 0; index < numToProcess; index++)
    {
        // Processing detections in the order of the sort above.
        int i = ovals[index].first;

        if (catalog->flag[i] & SEP_OBJ_TRUNC)
        {
            // Don't accept detections that go over the boundary.
            continue;
        }

        //Variables that are obtained from the catalog
        //FOR SOME REASON, I FOUND THAT THE POSITIONS WERE OFF BY 1 PIXEL??
        //This might be because of this: https://sextractor.readthedocs.io/en/latest/Param.html
        //" Following the FITS convention, in SExtractor the center of the first image pixel has coordinates (1.0,1.0). "
        float xPos = catalog->x[i] + 1;
        float yPos = catalog->y[i] + 1;
        float a = catalog->a[i];
        float b = catalog->b[i];
        float theta = catalog->theta[i];
        float cxx = catalog->cxx[i];
        float cyy = catalog->cxx[i];
        float cxy = catalog->cxy[i];
        double flux = catalog->flux[i];
        double peak = catalog->peak[i];
        int numPixels = catalog->npix[i];

        //Variables that will be obtained through methods
        double kronrad;
        short kron_flag;
        double sum = 0;
        double sumerr;
        double kron_area;

        //This will need to be done for both auto and ellipse
        if(m_ActiveParameters.apertureShape != SHAPE_CIRCLE)
        {
            //Constant values
            //The instructions say to use a fixed value of 6: https://sep.readthedocs.io/en/v1.0.x/api/sep.kron_radius.html
            //Finding the kron radius for the sextraction

            sep_kron_radius(&im, xPos, yPos, cxx, cyy, cxy, 6, 0, &kronrad, &kron_flag);
        }

        bool use_circle;

        switch(m_ActiveParameters.apertureShape)
        {
            case SHAPE_AUTO:
                use_circle = kronrad * sqrt(a * b) < m_ActiveParameters.r_min;
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
            sep_sum_circle(&im, xPos, yPos, m_ActiveParameters.r_min, 0, m_ActiveParameters.subpix, m_ActiveParameters.inflags, &sum,
                           &sumerr, &kron_area, &kron_flag);
        }
        else
        {
            sep_sum_ellipse(&im, xPos, yPos, a, b, theta, m_ActiveParameters.kron_fact * kronrad, 0, m_ActiveParameters.subpix,
                            m_ActiveParameters.inflags, &sum, &sumerr,
                            &kron_area, &kron_flag);
        }

        float mag = m_ActiveParameters.magzero - 2.5 * log10(sum);
        float HFR = 0;

        if(m_ProcessType == EXTRACT_WITH_HFR)
        {
            //Get HFR
            sep_flux_radius(&im, catalog->x[i], catalog->y[i], maxRadius, 0, m_ActiveParameters.subpix, 0, &flux, requested_frac, 2,
                            flux_fractions,
                            &flux_flag);
            HFR = flux_fractions[0];
        }

        FITSImage::Star oneStar = {xPos,
                                   yPos,
                                   mag,
                                   static_cast<float>(sum),
                                   static_cast<float>(peak),
                                   HFR,
                                   a,
                                   b,
                                   qRadiansToDegrees(theta),
                                   0,
                                   0,
                                   numPixels
                                  };
        // Make a copy and add it to QList
        partitionStars.append(oneStar);
    }

    cleanup();

    return partitionStars;
}

void InternalExtractorSolver::applyStarFilters(QList<FITSImage::Star> &starList)
{
    if(starList.size() > 1)
    {
        emit logOutput(QString("Stars Found before Filtering: %1").arg(starList.size()));
        if(m_ActiveParameters.resort)
        {
            //Note that a star is dimmer when the mag is greater!
            //We want to sort in decreasing order though!
            std::sort(starList.begin(), starList.end(), [](const FITSImage::Star & s1, const FITSImage::Star & s2)
            {
                return s1.mag < s2.mag;
            });
        }

        if(m_ActiveParameters.maxSize > 0.0)
        {
            emit logOutput(QString("Removing stars wider than %1 pixels").arg(m_ActiveParameters.maxSize));
            starList.erase(std::remove_if(starList.begin(), starList.end(), [&](FITSImage::Star & oneStar)
            {
                return (oneStar.a > m_ActiveParameters.maxSize || oneStar.b > m_ActiveParameters.maxSize);
            }), starList.end());
        }

        if(m_ActiveParameters.minSize > 0.0)
        {
            emit logOutput(QString("Removing stars smaller than %1 pixels").arg(m_ActiveParameters.minSize));
            starList.erase(std::remove_if(starList.begin(), starList.end(), [&](FITSImage::Star & oneStar)
            {
                return ((oneStar.a < m_ActiveParameters.minSize || oneStar.b < m_ActiveParameters.minSize));
            }), starList.end());
        }

        if(m_ActiveParameters.resort && m_ActiveParameters.removeBrightest > 0.0 && m_ActiveParameters.removeBrightest < 100.0)
        {
            int numToRemove = starList.count() * (m_ActiveParameters.removeBrightest / 100.0);
            emit logOutput(QString("Removing the %1 brightest stars").arg(numToRemove));
            if(numToRemove > 1)
            {
                for(int i = 0; i < numToRemove; i++)
                    starList.removeFirst();
            }
        }

        if(m_ActiveParameters.resort && m_ActiveParameters.removeDimmest > 0.0 && m_ActiveParameters.removeDimmest < 100.0)
        {
            int numToRemove = starList.count() * (m_ActiveParameters.removeDimmest / 100.0);
            emit logOutput(QString("Removing the %1 dimmest stars").arg(numToRemove));
            if(numToRemove > 1)
            {
                for(int i = 0; i < numToRemove; i++)
                    starList.removeLast();
            }
        }

        if(m_ActiveParameters.maxEllipse > 1)
        {
            emit logOutput(QString("Removing the stars with a/b ratios greater than %1").arg(m_ActiveParameters.maxEllipse));
            starList.erase(std::remove_if(starList.begin(), starList.end(), [&](FITSImage::Star & oneStar)
            {
                return (oneStar.b != 0 && oneStar.a / oneStar.b > m_ActiveParameters.maxEllipse);
            }), starList.end());
        }

        if(m_ActiveParameters.saturationLimit > 0.0 && m_ActiveParameters.saturationLimit < 100.0)
        {
            double maxSizeofDataType;
            if(m_Statistics.dataType == TSHORT || m_Statistics.dataType == TLONG || m_Statistics.dataType == TLONGLONG)
                maxSizeofDataType = pow(2, m_Statistics.bytesPerPixel * 8) / 2 - 1;
            else if(m_Statistics.dataType == TUSHORT || m_Statistics.dataType == TULONG)
                maxSizeofDataType = pow(2, m_Statistics.bytesPerPixel * 8) - 1;
            else // Float and Double Images saturation level is not so easy to determine, especially since they were probably processed by another program and the saturation level is now changed.
                maxSizeofDataType = -1;

            if(maxSizeofDataType == -1)
            {
                emit logOutput("Skipping Saturation filter");
            }
            else
            {
                emit logOutput(QString("Removing the saturated stars with peak values greater than %1 Percent of %2").arg(
                                   m_ActiveParameters.saturationLimit).arg(maxSizeofDataType));
                starList.erase(std::remove_if(starList.begin(), starList.end(), [&](FITSImage::Star & oneStar)
                {
                    return (oneStar.peak > (m_ActiveParameters.saturationLimit / 100.0) * maxSizeofDataType);
                }), starList.end());
            }
        }

        if(m_ActiveParameters.resort && m_ActiveParameters.keepNum > 0)
        {
            emit logOutput(QString("Keeping just the %1 brightest stars").arg(m_ActiveParameters.keepNum));
            int numToRemove = starList.size() - m_ActiveParameters.keepNum;
            if(numToRemove > 1)
            {
                for(int i = 0; i < numToRemove; i++)
                    starList.removeLast();
            }
        }
        emit logOutput(QString("Stars Found after Filtering: %1").arg(starList.size()));
    }
}

template <typename T>
bool InternalExtractorSolver::getFloatBuffer(float * buffer, int x, int y, int w, int h)
{
    int channelShift = (m_Statistics.channels < 3 || usingDownsampledImage
                        || usingMergedChannelImage) ? 0 : ( m_Statistics.samples_per_channel * m_Statistics.bytesPerPixel * m_ColorChannel );
    auto * rawBuffer = reinterpret_cast<T const *>(m_ImageBuffer + channelShift);
    float * floatPtr = buffer;

    int x2 = x + w;
    int y2 = y + h;

    for (int y1 = y; y1 < y2; y1++)
    {
        int offset = y1 * m_Statistics.width;
        for (int x1 = x; x1 < x2; x1++)
        {
            *floatPtr++ = rawBuffer[offset + x1];
        }
    }

    return true;
}

bool InternalExtractorSolver::downsampleImage(int d)
{
    switch (m_Statistics.dataType)
    {
        case SEP_TBYTE:
            return downSampleImageType<uint8_t>(d);
        case TSHORT:
            return downSampleImageType<int16_t>(d);
        case TUSHORT:
            return downSampleImageType<uint16_t>(d);
        case TLONG:
            return downSampleImageType<int32_t>(d);
        case TULONG:
            return downSampleImageType<uint32_t>(d);
        case TFLOAT:
            return downSampleImageType<float>(d);
        case TDOUBLE:
            return downSampleImageType<double>(d);
        default:
            return false;
    }
}

template <typename T>
bool InternalExtractorSolver::downSampleImageType(int d)
{
    int w = m_Statistics.width;
    int h = m_Statistics.height;
    int oldBufferSize = m_Statistics.samples_per_channel * m_Statistics.bytesPerPixel;
    //It is d times smaller in width and height
    int newBufferSize = oldBufferSize / (d * d);
    if(downSampledBuffer)
        delete [] downSampledBuffer;
    downSampledBuffer = new uint8_t[newBufferSize];
    int channelShift = ( m_Statistics.channels < 3
                         || usingMergedChannelImage) ? 0 : ( m_Statistics.samples_per_channel * m_Statistics.bytesPerPixel * m_ColorChannel );
    auto * sourceBuffer = reinterpret_cast<T const *>(m_ImageBuffer + channelShift);
    auto * destinationBuffer = reinterpret_cast<T *>(downSampledBuffer);

    for(int y = 0; y < h - d; y += d)
    {
        for (int x = 0; x < w - d; x += d)
        {
            //The sum of all the pixels in the sample
            double total = 0;

            for(int y2 = 0; y2 < d; y2++)
            {
                //The offset for the current line of the sample to take, since we have to sample different rows
                int currentLine = w * y2;

                auto *sample = sourceBuffer + currentLine + x;
                for(int x2 = 0; x2 < d; x2++)
                {
                    //This iterates the sample x2 spots to the right,
                    total += *sample++;
                }
            }
            //This calculates the average pixel value and puts it in the new downsampled image.
            int pixel = (x / d) + (y / d) * (w / d);
            destinationBuffer[pixel] = total / (d * d);
        }
        //Shifts the pointer by a whole line, d times
        sourceBuffer += w * d;
    }

    m_ImageBuffer = downSampledBuffer;
    m_Statistics.samples_per_channel /= (d * d);
    m_Statistics.width /= d;
    m_Statistics.height /= d;
    if(scaleunit == ARCSEC_PER_PIX)
    {
        scalelo *= d;
        scalehi *= d;
    }
    usingDownsampledImage = true;
    return true;
}

bool InternalExtractorSolver::mergeImageChannels()
{
    switch (m_Statistics.dataType)
    {
        case SEP_TBYTE:
            return mergeImageChannelsType<uint8_t>();
        case TSHORT:
            return mergeImageChannelsType<int16_t>();
        case TUSHORT:
            return mergeImageChannelsType<uint16_t>();
        case TLONG:
            return mergeImageChannelsType<int32_t>();
        case TULONG:
            return mergeImageChannelsType<uint32_t>();
        case TFLOAT:
            return mergeImageChannelsType<float>();
        case TDOUBLE:
            return mergeImageChannelsType<double>();
        default:
            return false;
    }

}

template <typename T>
bool InternalExtractorSolver::mergeImageChannelsType()
{
    if(m_Statistics.channels != 3)
        return false;
    if(m_ColorChannel != FITSImage::INTEGRATED_RGB && m_ColorChannel != FITSImage::AVERAGE_RGB)
        return false;

    auto w = m_Statistics.width;
    auto h = m_Statistics.height;
    auto nextChannel = m_Statistics.samples_per_channel;
    auto channelSize = m_Statistics.samples_per_channel * m_Statistics.bytesPerPixel;

    if(mergedChannelBuffer)
        delete [] mergedChannelBuffer;

    mergedChannelBuffer = new uint8_t[channelSize];
    auto * source = reinterpret_cast<T const *>(m_ImageBuffer);
    auto * dest = reinterpret_cast<T *>(mergedChannelBuffer);

    for(int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            double total  = 0;
            int r = x + y * w;
            int g = x + y * w + nextChannel;
            int b = x + y * w + nextChannel * 2;
            if(m_ColorChannel == FITSImage::INTEGRATED_RGB)
                total = source[r] + source[g] + source[b];
            if(m_ColorChannel == FITSImage::AVERAGE_RGB)
                total = (source[r] + source[g] + source[b]) / 3.0;
            dest[ x + y * w ] = static_cast<T>(total);
        }
    }

    m_ImageBuffer = mergedChannelBuffer;
    usingMergedChannelImage = true;
    return true;
}

//This method prepares the job file.  It is based upon the methods parse_job_from_qfits_header and engine_read_job_file in engine.c of astrometry.net
//as well as the part of the method augment_xylist in augment_xylist.c where it handles xyls files
bool InternalExtractorSolver::prepare_job()
{
    blind_t* bp = &(job->bp);
    solver_t* sp = &(bp->solver);

    job->scales = dl_new(8);
    job->depths = il_new(8);

    job->use_radec_center = m_UsePosition ? TRUE : FALSE;
    if(m_UsePosition)
    {
        job->ra_center = search_ra;
        job->dec_center = search_dec;
        job->search_radius = m_ActiveParameters.search_radius;
    }

    //These initialize the blind and solver objects, and they MUST be in this order according to astrometry.net
    blind_init(bp);
    solver_set_default_values(sp);

    //These set the width and the height of the image in the solver
    sp->field_maxx = m_Statistics.width;
    sp->field_maxy = m_Statistics.height;

    //We would like the Coordinates found to be the center of the image
    sp->set_crpix = TRUE;
    sp->set_crpix_center = TRUE;

    //Logratios for Solving
    bp->logratio_tosolve = m_ActiveParameters.logratio_tosolve;
    sp->logratio_tokeep = m_ActiveParameters.logratio_tokeep;
    sp->logratio_totune = m_ActiveParameters.logratio_totune;
    sp->logratio_bail_threshold = log(DEFAULT_BAIL_THRESHOLD);

    bp->best_hit_only = TRUE;

    // gotta keep it to solve it!
    sp->logratio_tokeep = MIN(sp->logratio_tokeep, bp->logratio_tosolve);

    job->include_default_scales = 0;
    sp->parity = m_ActiveParameters.search_parity;

    //These set the default tweak settings
    sp->do_tweak = TRUE;
    sp->tweak_aborder = 2;
    sp->tweak_abporder = 2;

    if (m_UseScale)
    {
        double appu, appl;
        switch (scaleunit)
        {
            case DEG_WIDTH:
                emit logOutput(QString("Scale range: %1 to %2 degrees wide").arg(scalelo).arg(scalehi));
                appl = deg2arcsec(scalelo) / (double)m_Statistics.width;
                appu = deg2arcsec(scalehi) / (double)m_Statistics.width;
                break;
            case ARCMIN_WIDTH:
                emit logOutput(QString("Scale range: %1 to %2 arcmin wide").arg (scalelo).arg(scalehi));
                appl = arcmin2arcsec(scalelo) / (double)m_Statistics.width;
                appu = arcmin2arcsec(scalehi) / (double)m_Statistics.width;
                break;
            case ARCSEC_PER_PIX:
                emit logOutput(QString("Scale range: %1 to %2 arcsec/pixel").arg (scalelo).arg (scalehi));
                appl = scalelo;
                appu = scalehi;
                break;
            case FOCAL_MM:
                emit logOutput(QString("Scale range: %1 to %2 mm focal length").arg (scalelo).arg (scalehi));
                // "35 mm" film is 36 mm wide.
                appu = rad2arcsec(atan(36. / (2. * scalelo))) / (double)m_Statistics.width;
                appl = rad2arcsec(atan(36. / (2. * scalehi))) / (double)m_Statistics.width;
                break;
            default:
                emit logOutput(QString("Unknown scale unit code %1").arg (scaleunit));
                return false;
        }

        dl_append(job->scales, appl);
        dl_append(job->scales, appu);
        blind_add_field_range(bp, appl, appu);

        if(scaleunit == ARCMIN_WIDTH || scaleunit == DEG_WIDTH || scaleunit == FOCAL_MM)
        {
            if(m_ActiveParameters.downsample == 1)
                emit logOutput(QString("Image width %1 pixels; arcsec per pixel range: %2 to %3").arg (m_Statistics.width).arg (appl).arg (
                                   appu));
            else
                emit logOutput(QString("Image width: %1 pixels, Downsampled Image width: %2 pixels; arcsec per pixel range: %3 to %4").arg(
                                   m_Statistics.width * m_ActiveParameters.downsample).arg (m_Statistics.width).arg (appl).arg (appu));
        }
        if(m_ActiveParameters.downsample != 1 && scaleunit == ARCSEC_PER_PIX)
            emit logOutput(QString("Downsampling is multiplying the pixel scale by: %1").arg(m_ActiveParameters.downsample));
    }

    blind_add_field(bp, 1);


    return true;
}

//This method was adapted from the main method in engine-main.c in astrometry.net
int InternalExtractorSolver::runInternalSolver()
{
    if(!isChildSolver)
    {
        emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logOutput("Configuring StellarSolver");
    }

    //This creates and sets up the engine
    engine_t* engine = engine_new();

    //This sets some basic engine settings
    engine->inparallel = m_ActiveParameters.inParallel ? TRUE : FALSE;
    engine->minwidth = m_ActiveParameters.minwidth;
    engine->maxwidth = m_ActiveParameters.maxwidth;

    log_init((log_level)m_AstrometryLogLevel);

    if(m_AstrometryLogLevel != SSolver::LOG_NONE)
    {
        if(m_LogToFile)
            logFile = fopen(m_LogFileName.toLatin1().constData(), "w");
        else
        {
            if(!this->isChildSolver)
            {
                connect(&astroLogger, &AstrometryLogger::logOutput, this, &ExtractorSolver::logOutput);
                setAstroLogger(&astroLogger);
            }
        }
        if(logFile)
            log_to(logFile);
    }
    for(const auto &onePath : indexFiles)
    {
        engine_add_index(engine, onePath.toUtf8().data());
    }
    //These set the folders in which Astrometry.net will look for index files, based on the folers set before the solver was started.
    for(const auto &onePath : indexFolderPaths)
    {
        engine_add_search_path(engine, onePath.toLatin1().constData());
    }

    //This actually adds the index files in the directories above.
    if(indexFolderPaths.count() > 0)
        engine_autoindex_search_paths(engine);

    //This checks to see that index files were found in the paths above, if not, it prints this warning and aborts.
    if (!pl_size(engine->indexes))
    {
        emit logOutput(QString("\n\n"
                               "---------------------------------------------------------------------\n"
                               "You must include at least one index file in the index file directories\n\n"
                               "See http://astrometry.net/use.html about how to get some index files.\n"
                               "---------------------------------------------------------------------\n"
                               "\n"));
        engine_free(engine);
        engine = nullptr;
        return -1;
    }

    prepare_job();

    blind_t* bp = &(job->bp);

    //This will set up the field file to solve as an xylist
    double *xArray = new double[m_ExtractedStars.size()];
    double *yArray = new double[m_ExtractedStars.size()];

    int i = 0;
    for(const auto &oneStar : m_ExtractedStars)
    {
        xArray[i] = oneStar.x;
        yArray[i] = oneStar.y;
        i++;
    }

    starxy_t* fieldToSolve = (starxy_t*)calloc(1, sizeof(starxy_t));
    fieldToSolve->x = xArray;
    fieldToSolve->y = yArray;
    fieldToSolve->N = m_ExtractedStars.size();
    fieldToSolve->flux = nullptr;
    fieldToSolve->background = nullptr;
    bp->solver.fieldxy = fieldToSolve;

    if(depthlo != -1 && depthhi != -1)
    {
        il_append(job->depths, depthlo);
        il_append(job->depths, depthhi);
    }
    else
    {
        //This sets the depths for the job.
        if (!il_size(engine->default_depths))
        {
            for(int i = 10; i < 210; i += 10)
                il_append(engine->default_depths, i);
        }
        if (il_size(job->depths) == 0)
        {
            if (engine->inparallel)
            {
                // no limit.
                il_append(job->depths, 0);
                il_append(job->depths, 0);
            }
            else
                il_append_list(job->depths, engine->default_depths);
        }
    }

    //This makes sure the min and max widths for the engine make sense, aborting if not.
    if (engine->minwidth <= 0.0 || engine->maxwidth <= 0.0 || engine->minwidth > engine->maxwidth)
    {
        emit logOutput(QString("\"minwidth\" and \"maxwidth\" must be positive and the maxwidth must be greater!\n"));
        return -1;
    }
    ///This sets the scales based on the minwidth and maxwidth if the image scale isn't known
    if (!dl_size(job->scales))
    {
        double arcsecperpix;
        arcsecperpix = deg2arcsec(engine->minwidth) / m_Statistics.width;
        dl_append(job->scales, arcsecperpix);
        arcsecperpix = deg2arcsec(engine->maxwidth) / m_Statistics.height;
        dl_append(job->scales, arcsecperpix);
    }

    // These set the time limits for the solver
    bp->timelimit = m_ActiveParameters.solverTimeLimit;
#ifndef _WIN32
    bp->cpulimit = m_ActiveParameters.solverTimeLimit;
#endif

    // If not running inparallel, set total limits = limits.
    if (!engine->inparallel)
    {
        bp->total_timelimit = bp->timelimit;
        bp->total_cpulimit  = bp->cpulimit ;
    }
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting Internal StellarSolver Astrometry.net based Engine with the " + m_ActiveParameters.listName +
                   " profile. . .");

    //This runs the job in the engine in the file engine.c
    if (engine_run_job(engine, job))
        emit logOutput("Failed to run job");

    //Needs to close the file after the logging is done
    if(m_AstrometryLogLevel != SSolver::LOG_NONE && logFile)
        fclose(logFile);
    if(m_AstrometryLogLevel != SSolver::LOG_NONE && !this->isChildSolver)
        disconnect(&astroLogger, &AstrometryLogger::logOutput, this, &ExtractorSolver::logOutput);

    //This deletes or frees the items that are no longer needed.
    engine_free(engine);
    engine = nullptr;
    bl_free(job->scales);
    job->scales = nullptr;
    dl_free(job->depths);
    job->depths = nullptr;
    free(fieldToSolve);
    fieldToSolve = nullptr;
    delete[] xArray;
    xArray = 0;
    delete[] yArray;
    yArray = 0;

    //Note: I can only get these items after the solve because I made a couple of small changes to the Astrometry.net Code.
    //I made it return in solve_fields in blind.c before it ran "cleanup".  I also had it wait to clean up solutions, blind and solver in engine.c.  We will do that after we get the solution information.

    match = bp->solver.best_match;
    int returnCode = 0;
    if(match.sip)
    {
        wcs = *match.sip;
        m_HasWCS = true;
        double ra, dec, fieldw, fieldh, pixscale;
        char rastr[32], decstr[32];
        FITSImage::Parity parity;
        char* fieldunits;

        // print info about the field.

        double orient;
        sip_get_radec_center(&wcs, &ra, &dec);
        sip_get_radec_center_hms_string(&wcs, rastr, decstr);
        sip_get_field_size(&wcs, &fieldw, &fieldh, &fieldunits);
        orient = sip_get_orientation(&wcs);

        // Note, negative determinant = positive parity.
        double det = sip_det_cd(&wcs);
        parity = (det < 0 ? FITSImage::POSITIVE : FITSImage::NEGATIVE);
        if(usingDownsampledImage)
            pixscale = sip_pixel_scale(&wcs) / m_ActiveParameters.downsample;
        else
            pixscale = sip_pixel_scale(&wcs);

        double raErr = 0;
        double decErr = 0;
        if(m_UsePosition)
        {
            raErr = (search_ra - ra) * 3600;
            decErr = (search_dec - dec) * 3600;
        }

        if(strcmp(fieldunits, "degrees") == 0)
        {
            fieldw *= 60;
            fieldh *= 60;
        }
        if(strcmp(fieldunits, "arcseconds") == 0)
        {
            fieldw /= 60;
            fieldh /= 60;
        }

        emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logOutput(QString("Solve Log Odds:  %1").arg(bp->solver.best_logodds));
        emit logOutput(QString("Number of Matches:  %1").arg(match.nmatch));
        emit logOutput(QString("Solved with index:  %1").arg(match.indexid));
        emit logOutput(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
        emit logOutput(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr, decstr));
        if(m_UsePosition)
            emit logOutput(QString("Field is: (%1, %2) deg from search coords.").arg( raErr).arg( decErr));
        emit logOutput(QString("Field size: %1 x %2 arcminutes").arg( fieldw).arg( fieldh));
        emit logOutput(QString("Pixel Scale: %1\"").arg( pixscale));
        emit logOutput(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
        emit logOutput(QString("Field parity: %1\n").arg(FITSImage::getParityText(parity).toUtf8().data()));

        m_Solution = {fieldw, fieldh, ra, dec, orient, pixscale, parity, raErr, decErr};
        solutionIndexNumber = match.indexid;
        solutionHealpix = match.healpix;
        m_HasSolved = true;
        returnCode = 0;
    }
    else
    {
        if(!isChildSolver)
            emit logOutput("Solver was aborted, timed out, or failed, so no solution was found");
        returnCode = -1;
    }

    //This code was taken from engine.c and blind.c so that we can clean up all of the allocated memory after we get the solution information out of it so that we can prevent memory leaks.

    for (size_t i = 0; i < bl_size(bp->solutions); i++)
    {
        MatchObj* mo = (MatchObj*)bl_access(bp->solutions, i);
        //verify_free_matchobj(mo); // redundent with below
        blind_free_matchobj(mo);
    }
    //bl_remove_all(bp->solutions); // redundent with below
    //blind_clear_solutions(bp); // calls bl_remove_all(bp->solutions), redundent
    solver_cleanup(&bp->solver);
    blind_cleanup(bp);// calls bl_free(bp->solutions) which calls bl_remove_all(solutions)

    return returnCode;
}

WCSData InternalExtractorSolver::getWCSData()
{
    return WCSData(wcs, m_ActiveParameters.downsample);
}
