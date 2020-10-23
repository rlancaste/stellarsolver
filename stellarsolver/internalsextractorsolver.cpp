/*  InternalSextractorSolver, StellarSolver Internal Library developed by Robert Lancaster, 2020

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

#include <QtConcurrent>
#include <memory>
#include <algorithm>

#include "internalsextractorsolver.h"
#include "sep/extract.h"
#include "qmath.h"

extern "C" {
#include "astrometry/log.h"
}

using namespace SSolver;
using namespace SEP;

static int solverNum = 1;

InternalSextractorSolver::InternalSextractorSolver(ProcessType pType, ExtractorType eType, SolverType sType,
        FITSImage::Statistic imagestats, uint8_t const *imageBuffer, QObject *parent) : SextractorSolver(pType, eType, sType,
                    imagestats, imageBuffer, parent)
{
    //This sets the base name used for the temp files.
    m_BaseName = "internalSextractorSolver_" + QString::number(solverNum++);

    m_PartitionThreads = QThread::idealThreadCount();

}

InternalSextractorSolver::~InternalSextractorSolver()
{
    if(usingDownsampledImage)
        delete downSampledBuffer;
}

//This is the abort method.  The way that it works is that it creates a file.  Astrometry.net is monitoring for this file's creation in order to abort.
void InternalSextractorSolver::abort()
{
    QFile file(cancelfn);
    if(QFileInfo(file).dir().exists())
    {
        file.open(QIODevice::WriteOnly);
        file.write("Cancel");
        file.close();
    }
    if(!isChildSolver)
        emit logOutput("Aborting...");
    m_WasAborted = true;
}

//This method generates child solvers with the options of the current solver
SextractorSolver* InternalSextractorSolver::spawnChildSolver(int n)
{
    Q_UNUSED(n);

    InternalSextractorSolver *solver = new InternalSextractorSolver(m_ProcessType, m_ExtractorType, m_SolverType, m_Statistics,
            m_ImageBuffer, nullptr);
    solver->m_ExtractedStars = m_ExtractedStars;
    solver->m_BasePath = m_BasePath;
    //They will all share the same basename
    solver->m_HasExtracted = true;
    solver->isChildSolver = true;
    solver->m_ActiveParameters = m_ActiveParameters;
    solver->indexFolderPaths = indexFolderPaths;
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
        connect(solver, &SextractorSolver::logOutput, this,  &SextractorSolver::logOutput);
    //This way they all share a solved and cancel fn
    solver->cancelfn = cancelfn;
    solver->solvedfn = solvedfn;
    solver->usingDownsampledImage = usingDownsampledImage;
    return solver;
}

int InternalSextractorSolver::extract()
{
    return(runSEPSextractor());
}

//This is the method that runs the solver or sextractor.  Do not call it, use the methods above instead, so that it can start a new thread.
void InternalSextractorSolver::run()
{
    if(computingWCS)
    {
        if(m_HasSolved)
        {
            computeWCSCoord();
            emit finished(0);
        }
        else
            emit finished(-1);
        computingWCS = false;
        return;
    }

    if(m_AstrometryLogLevel != SSolver::LOG_NONE && m_LogToFile)
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

    if(QFile(cancelfn).exists())
        QFile(cancelfn).remove();
    if(QFile(solvedfn).exists())
        QFile(solvedfn).remove();

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
                extract();
            if(m_HasExtracted)
            {
                int result = runInternalSolver();
                emit finished(result);
            }
            else
                emit finished(-1);
        }
        break;

        default:
            break;
    }

    if(!isChildSolver)
    {
        QFile(solvedfn).remove();
        QFile(cancelfn).remove();
    }
}

void InternalSextractorSolver::allocateDataBuffer(float *data, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    switch (m_Statistics.dataType)
    {
        case SEP_TBYTE:
            getFloatBuffer<uint8_t>(data, x, y, w, h);
            break;
        case TSHORT:
            getFloatBuffer<int16_t>(data, x, y, w, h);
            break;
        case TUSHORT:
            getFloatBuffer<uint16_t>(data, x, y, w, h);
            break;
        case TLONG:
            getFloatBuffer<int32_t>(data, x, y, w, h);
            break;
        case TULONG:
            getFloatBuffer<uint32_t>(data, x, y, w, h);
            break;
        case TFLOAT:
            getFloatBuffer<float>(data, x, y, w, h);
            break;
        case TDOUBLE:
            getFloatBuffer<double>(data, x, y, w, h);
            break;
        default:
            delete [] data;
    }
}

//The code in this section is my attempt at running an internal sextractor program based on SEP
//I used KStars and the SEP website as a guide for creating these functions
int InternalSextractorSolver::runSEPSextractor()
{
    if(m_ActiveParameters.convFilter.size() == 0)
    {
        emit logOutput("No convFilter included.");
        return -1;
    }
    emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    emit logOutput("Starting Internal StellarSolver Sextractor with the " + m_ActiveParameters.listName + " profile . . .");

    //Only downsample images before SEP if the Sextraction is being used for plate solving
    if(m_ProcessType == SOLVE && m_SolverType == SOLVER_STELLARSOLVER && m_ActiveParameters.downsample != 1)
        downsampleImage(m_ActiveParameters.downsample);

    uint32_t x = 0, y = 0;
    uint32_t w = m_Statistics.width, h = m_Statistics.height;
    uint32_t raw_w = m_Statistics.width, raw_h = m_Statistics.height;
    if(m_UseSubframe)
    {
        x = m_SubFrameRect.x();
        w = m_SubFrameRect.width();
        y = m_SubFrameRect.y();
        h = m_SubFrameRect.height();

        raw_w = w;
        raw_h = h;
    }

    QList<float *> dataBuffers;
    QList<QFuture<QList<FITSImage::Star>>> futures;
    QList<QPair<uint32_t, uint32_t>> startupOffsets;

    if (w > PARTITION_SIZE && h > PARTITION_SIZE)
    {
        // Partition the image to regions, each region is 200x200
        // If there is extra at the end, we add an offset.
        // e.g. 500x400 image would have 4 paritions
        // #1 0, 0, 200, 200 (200 x 200)
        // #2 200, 0, 200 + 100, 200 (300 x 200)
        // #3 0, 200, 200, 200 (200 x 200)
        // #4 200, 200, 200 + 100, 200 (300 x 200)

        int W_PARTITION_SIZE = PARTITION_SIZE;
        int H_PARTITION_SIZE = PARTITION_SIZE;

        // Limit it to PARTITION_THREADS
        if ( (w * h) / (W_PARTITION_SIZE * H_PARTITION_SIZE) > m_PartitionThreads)
        {
            W_PARTITION_SIZE = w / std::min(m_PartitionThreads / 2, (uint32_t)1);
            H_PARTITION_SIZE = h / std::min(m_PartitionThreads / 4, (uint32_t)1);
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
                uint32_t subX = x + j * W_PARTITION_SIZE + PARTITION_MARGIN;
                uint32_t subY = y + i * H_PARTITION_SIZE + PARTITION_MARGIN;
                uint32_t subW = W_PARTITION_SIZE + offsetW - PARTITION_MARGIN;
                uint32_t subH = H_PARTITION_SIZE + offsetH - PARTITION_MARGIN;
                //uint32_t offset = subX + (subY * raw_w);

                auto * data = new float[subW * subH];
                allocateDataBuffer(data, subX, subY, subW, subH);
                dataBuffers.append(data);
                startupOffsets.append(qMakePair(subX, subY));

                ImageParams parameters = {data,
                                          subW,
                                          subH,
                                          0,
                                          0,
                                          subW,
                                          subH
                                         };
                futures.append(QtConcurrent::run(this, &InternalSextractorSolver::extractPartition, parameters));
            }
        }
    }
    else
    {
        auto * data = new float[w * h];
        allocateDataBuffer(data, x, y, w, h);
        dataBuffers.append(data);
        startupOffsets.append(qMakePair(x, y));
        ImageParams parameters = {data, raw_w, raw_h, x, y, w, h};
        futures.append(QtConcurrent::run(this, &InternalSextractorSolver::extractPartition, parameters));
    }

    for (auto oneFuture : futures)
    {
        oneFuture.waitForFinished();
        QList<FITSImage::Star> partitionStars = oneFuture.result();
        if (!startupOffsets.empty())
        {
            QPair<uint32_t, uint32_t> oneOffset = startupOffsets.takeFirst();
            for (auto &oneStar : partitionStars)
            {
                oneStar.x += oneOffset.first;
                oneStar.y += oneOffset.second;
            }
        }
        m_ExtractedStars.append(partitionStars);
    }

    applyStarFilters(m_ExtractedStars);

    for (auto * buffer : dataBuffers)
        delete [] buffer;
    dataBuffers.clear();

    m_HasExtracted = true;

    return 0;
}

QList<FITSImage::Star> InternalSextractorSolver::extractPartition(const ImageParams &parameters)
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
        Extract::sep_catalog_free(catalog);
        free(imback);
        free(fluxerr);
        free(area);
        free(flag);

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
    m_Background.bh = bkg->bh;
    m_Background.bw = bkg->bw;
    m_Background.global = bkg->global;
    m_Background.globalrms = bkg->globalrms;

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
    status = extractor->sep_extract(&im, 2 * bkg->globalrms, SEP_THRESH_ABS, m_ActiveParameters.minarea,
                                    m_ActiveParameters.convFilter.data(),
                                    sqrt(m_ActiveParameters.convFilter.size()), sqrt(m_ActiveParameters.convFilter.size()), SEP_FILTER_CONV,
                                    m_ActiveParameters.deblend_thresh,
                                    m_ActiveParameters.deblend_contrast, m_ActiveParameters.clean, m_ActiveParameters.clean_param, &catalog);
    if (status != 0)
    {
        cleanup();
        return partitionStars;
    }

    // Record the number of stars detected.
    m_Background.num_stars_detected = catalog->nobj;

    // Find the oval sizes for each detection in the detected star catalog, and sort by that. Oval size
    // correlates very well with HFR and likely magnitude.
    for (int i = 0; i < catalog->nobj; i++)
    {
        const double ovalSizeSq = catalog->a[i] * catalog->a[i] + catalog->b[i] * catalog->b[i];
        ovals.push_back(std::pair<int, double>(i, ovalSizeSq));
    }
    std::sort(ovals.begin(), ovals.end(), [](const std::pair<int, double> &o1, const std::pair<int, double> &o2) -> bool { return o1.second > o2.second;});

    numToProcess = std::min(catalog->nobj, m_ActiveParameters.initialKeep);
    for (int index = 0; index < numToProcess; index++)
    {
        // Processing detections in the order of the sort above.
        int i = ovals[index].first;

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
        double sum;
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

void InternalSextractorSolver::applyStarFilters(QList<FITSImage::Star> &starList)
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

        if(m_ActiveParameters.resort && m_ActiveParameters.keepNum > 0.0)
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
void InternalSextractorSolver::getFloatBuffer(float * buffer, int x, int y, int w, int h)
{
    auto * rawBuffer = reinterpret_cast<T const *>(m_ImageBuffer);
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
}

void InternalSextractorSolver::downsampleImage(int d)
{
    switch (m_Statistics.dataType)
    {
        case SEP_TBYTE:
            downSampleImageType<uint8_t>(d);
            break;
        case TSHORT:
            downSampleImageType<int16_t>(d);
            break;
        case TUSHORT:
            downSampleImageType<uint16_t>(d);
            break;
        case TLONG:
            downSampleImageType<int32_t>(d);
            break;
        case TULONG:
            downSampleImageType<uint32_t>(d);
            break;
        case TFLOAT:
            downSampleImageType<float>(d);
            break;
        case TDOUBLE:
            downSampleImageType<double>(d);
            break;
        default:
            return;
    }

}

template <typename T>
void InternalSextractorSolver::downSampleImageType(int d)
{
    int w = m_Statistics.width;
    int h = m_Statistics.height;

    int numChannels;
    if (m_Statistics.ndim < 3)
        numChannels = 1;
    else
        numChannels = 3;
    int oldBufferSize = m_Statistics.samples_per_channel * numChannels * m_Statistics.bytesPerPixel;
    int newBufferSize = oldBufferSize / (d * d); //It is d times smaller in width and height
    downSampledBuffer = new uint8_t[newBufferSize];
    auto * sourceBuffer = reinterpret_cast<T const *>(m_ImageBuffer);
    auto * destinationBuffer = reinterpret_cast<T *>(downSampledBuffer);

    //The G pixels are after all the R pixels, Same for the B pixels
    auto * rSource = sourceBuffer;
    auto * gSource = sourceBuffer + (w * h);
    auto * bSource = sourceBuffer + (w * h * 2);

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

                //Shifting the R, G, and B Pointers to the sample location
                auto *rSample = rSource + currentLine + x;
                auto *gSample = gSource + currentLine + x;
                auto *bSample = bSource + currentLine + x;
                for(int x2 = 0; x2 < d; x2++)
                {
                    //This iterates the sample x2 spots to the right,
                    total += *rSample++;
                    //This only samples frome the G and B spots if it is an RGB image
                    if(numChannels == 3)
                    {
                        total += *gSample++;
                        total += *bSample++;
                    }
                }
            }
            //This calculates the average pixel value and puts it in the new downsampled image.
            int pixel = (x / d) + (y / d) * (w / d);
            destinationBuffer[pixel] = total / (d * d) / numChannels;
        }
        //Shifts each pointer by a whole line, d times
        rSource += w * d;
        gSource += w * d;
        bSource += w * d;
    }

    m_ImageBuffer = downSampledBuffer;
    m_Statistics.width /= d;
    m_Statistics.height /= d;
    if(scaleunit == ARCSEC_PER_PIX)
    {
        scalelo *= d;
        scalehi *= d;
    }
    usingDownsampledImage = true;
}

//This method prepares the job file.  It is based upon the methods parse_job_from_qfits_header and engine_read_job_file in engine.c of astrometry.net
//as well as the part of the method augment_xylist in augment_xylist.c where it handles xyls files
bool InternalSextractorSolver::prepare_job()
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

    blind_set_cancel_file(bp, cancelfn.toLatin1().constData());
    blind_set_solved_file(bp, solvedfn.toLatin1().constData());

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
int InternalSextractorSolver::runInternalSolver()
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
        {
            logFile = fopen(m_LogFileName.toLatin1().constData(), "w");
        }
        else
        {
#ifndef _WIN32 //Windows does not support FIFO files
            m_LogFileName = m_BasePath + "/" + m_BaseName + ".logFIFO.txt";
            int mkFifoSuccess = 0; //Note if the return value of the command is 0 it succeeded, -1 means it failed.
            if ((mkFifoSuccess = mkfifo(m_LogFileName.toLatin1(), S_IRUSR | S_IWUSR) == 0))
            {
                startLogMonitor();
                logFile = fopen(m_LogFileName.toLatin1().constData(), "r+");
            }
            else
            {
                emit logOutput("Error making FIFO file: " + m_LogFileName);
                m_AstrometryLogLevel = SSolver::LOG_NONE; //No need to completely fail the solve just because of a fifo error
            }
#endif
        }
        if(logFile)
            log_to(logFile);
    }

    //gslutils_use_error_system();

    //These set the folders in which Astrometry.net will look for index files, based on the folers set before the solver was started.
    for(auto onePath : indexFolderPaths)
    {
        engine_add_search_path(engine, onePath.toLatin1().constData());
    }

    //This actually adds the index files in the directories above.
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
        return -1;
    }

    prepare_job();
    engine->cancelfn = cancelfn.toLatin1().data();
    engine->solvedfn = solvedfn.toLatin1().data();

    blind_t* bp = &(job->bp);

    //This will set up the field file to solve as an xylist
    double *xArray = new double[m_ExtractedStars.size()];
    double *yArray = new double[m_ExtractedStars.size()];

    int i = 0;
    for(auto oneStar : m_ExtractedStars)
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
            parse_depth_string(engine->default_depths,
                               "10 20 30 40 50 60 70 80 90 100 "
                               "110 120 130 140 150 160 170 180 190 200");
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

    //Needs to be done whether FIFO or regular file
    if(m_AstrometryLogLevel != SSolver::LOG_NONE && logFile)
        fclose(logFile);

    //These things need to be done if it is running off the FIFO file
#ifndef _WIN32
    if(!m_LogToFile && m_AstrometryLogLevel != SSolver::LOG_NONE)
    {
        if(logMonitor)
            logMonitor->quit();

        //Wait up to 10 seconds for the log to finish writing
        int maxLogTime = 10000;
        int time = 0;
        int inc = 10;
        while(logMonitorRunning && time < maxLogTime)
        {
            msleep(inc);
            time += inc;
        }
    }
#endif

    //This deletes or frees the items that are no longer needed.
    engine_free(engine);
    bl_free(job->scales);
    dl_free(job->depths);
    free(fieldToSolve);
    delete[] xArray;
    delete[] yArray;

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
        QString parity;
        char* fieldunits;

        // print info about the field.

        double orient;
        sip_get_radec_center(&wcs, &ra, &dec);
        sip_get_radec_center_hms_string(&wcs, rastr, decstr);
        sip_get_field_size(&wcs, &fieldw, &fieldh, &fieldunits);
        orient = sip_get_orientation(&wcs);

        // Note, negative determinant = positive parity.
        double det = sip_det_cd(&wcs);
        parity = (det < 0 ? "pos" : "neg");
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

        emit logOutput("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
        emit logOutput(QString("Solve Log Odds:  %1").arg(bp->solver.best_logodds));
        emit logOutput(QString("Number of Matches:  %1").arg(match.nmatch));
        emit logOutput(QString("Solved with index:  %1").arg(match.indexid));
        emit logOutput(QString("Field center: (RA,Dec) = (%1, %2) deg.").arg( ra).arg( dec));
        emit logOutput(QString("Field center: (RA H:M:S, Dec D:M:S) = (%1, %2).").arg( rastr).arg( decstr));
        if(m_UsePosition)
            emit logOutput(QString("Field is: (%1, %2) deg from search coords.").arg( raErr).arg( decErr));
        emit logOutput(QString("Field size: %1 x %2 %3").arg( fieldw).arg( fieldh).arg( fieldunits));
        emit logOutput(QString("Pixel Scale: %1\"").arg( pixscale));
        emit logOutput(QString("Field rotation angle: up is %1 degrees E of N").arg( orient));
        emit logOutput(QString("Field parity: %1\n").arg( parity));

        m_Solution = {fieldw, fieldh, ra, dec, orient, pixscale, parity, raErr, decErr};
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
        verify_free_matchobj(mo);
        blind_free_matchobj(mo);
    }
    bl_remove_all(bp->solutions);
    blind_clear_solutions(bp);
    solver_cleanup(&bp->solver);
    blind_cleanup(bp);

    return returnCode;
}


void InternalSextractorSolver::computeWCSCoord()
{
    if(!m_HasWCS)
    {
        emit logOutput("There is no WCS Data.");
        return;
    }
    //We have to upscale back to the full size image
    int d = m_ActiveParameters.downsample;
    int w = m_Statistics.width * d;
    int h = m_Statistics.height * d;


    wcs_coord = new FITSImage::wcs_point[w * h];
    FITSImage::wcs_point * p = wcs_coord;

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            double ra;
            double dec;
            sip_pixelxy2radec(&wcs, x / d, y / d, &ra, &dec);
            p->ra = ra;
            p->dec = dec;
            p++;
        }
    }
}

bool InternalSextractorSolver::pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint)
{
    if(!hasWCSData())
    {
        emit logOutput("There is no WCS Data.");
        return false;
    }
    int d = m_ActiveParameters.downsample;
    double ra;
    double dec;
    sip_pixelxy2radec(&wcs, pixelPoint.x() / d, pixelPoint.y() / d, &ra, &dec);
    skyPoint.ra = ra;
    skyPoint.dec = dec;
    return true;
}

bool InternalSextractorSolver::wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint)
{
    if(!hasWCSData())
    {
        emit logOutput("There is no WCS Data.");
        return false;
    }
    double x;
    double y;
    anbool error = sip_radec2pixelxy(&wcs, skyPoint.ra, skyPoint.dec, &x, &y);
    if(error != 0)
        return false;
    pixelPoint.setX(x);
    pixelPoint.setY(y);
    return true;
}

bool InternalSextractorSolver::appendStarsRAandDEC(QList<FITSImage::Star> &stars)
{
    if(!m_HasWCS)
    {
        emit logOutput("There is no WCS Data");
        return false;
    }

    int d = m_ActiveParameters.downsample;
    for(auto &oneStar : stars)
    {
        double ra = HUGE_VAL;
        double dec = HUGE_VAL;
        sip_pixelxy2radec(&wcs, oneStar.x / d, oneStar.y / d, &ra, &dec);
        char rastr[32], decstr[32];
        ra2hmsstring(ra, rastr);
        dec2dmsstring(dec, decstr);
        oneStar.ra = ra;
        oneStar.dec = dec;
    }
    return true;
}

//This starts a monitor for the FIFO file if on a Mac or Linux Machine
//That way the output can be logged.
void InternalSextractorSolver::startLogMonitor()
{
    logMonitor = new QThread();
    connect(logMonitor, &QThread::started, [ = ]()
    {
        QString message;
        FILE *FIFOReader = fopen(m_LogFileName.toLatin1().constData(), "r");
        if(FIFOReader)
        {
            char c;
            while((c = getc(FIFOReader)) != EOF)
            {
                if( c != '\n')
                    message += c;
                else
                {
                    emit logOutput(message);
                    message = "";
                    //If we don't do this, it could freeze your program it is so much output
                    if(m_AstrometryLogLevel == SSolver::LOG_ALL)
                        msleep(1);
                }
            }
            fclose(FIFOReader);
            emit logOutput(message);
            logMonitorRunning = false;
        }
    });
    logMonitor->start();
    logMonitorRunning = true;
}
