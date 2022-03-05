/*  InternalExtractorSolver, StellarSolver Intenal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#pragma once

#include "extractorsolver.h"
#include "astrometrylogger.h"

//SEP Includes
#include "sep/sep.h"

using namespace SSolver;

class InternalExtractorSolver: public ExtractorSolver
{
    public:
        explicit InternalExtractorSolver(ProcessType pType, ExtractorType eType, SolverType sType,
                                          FITSImage::Statistic imagestats,  uint8_t const *imageBuffer, QObject *parent = nullptr);
        ~InternalExtractorSolver();

        // This struct contains information about the image used by SEP
        typedef struct
        {
            float *data;
            uint32_t width;
            uint32_t height;
            uint32_t subX;
            uint32_t subY;
            uint32_t subW;
            uint32_t subH;
            uint32_t keep;
            FITSImage::Background *background;
        } ImageParams;

        /**
         * @brief extract for the InternalExtractor does star extraction with SEP
         * @return whether or not it was successful, 0 means success
         */
        int extract() override;

        /**
         * @brief abort will stop the InternalExtractor by setting a cancel variable and using the quit method.
         */
        void abort() override;

        /**
         * @brief appendStarsRAandDEC attaches the RA and DEC information to a star list
         * @param stars is the star list to process
         * @return true if it was successful
         */
        bool appendStarsRAandDEC(QList<FITSImage::Star> &stars) override;

        /**
         * @brief spawnChildSolver is a method used by Internal ExtractorSolver to make the child solvers from this solver
         * @param n is not actually used by the internal solvers, just the external ones
         * @return the spawned child solver
         */
        ExtractorSolver* spawnChildSolver(int n) override;

        /**
         * @brief cleanupTempFiles is a method that is not used by the InternalExtractorSolver since there no longer any temp files
         */
        void cleanupTempFiles() override;

        /**
         * @brief pixelToWCS converts the image X, Y Pixel coordinates to RA, DEC sky coordinates using the WCS data
         * For the InternalSextractorSolver, it directly uses the SIP object obtained from the internal astrometry.net build
         * @param pixelPoint The X, Y coordinate in pixels
         * @param skyPoint The RA, DEC coordinates
         * @return A boolean to say whether it succeeded, true means it did
         */
        bool pixelToWCS(const QPointF &pixelPoint, FITSImage::wcs_point &skyPoint) override;

        /**
         * @brief wcsToPixel converts the RA, DEC sky coordinates to image X, Y Pixel coordinates using the WCS data
         * For the InternalSextractorSolver, it directly uses the SIP object obtained from the internal astrometry.net build
         * @param skyPoint The RA, DEC coordinates
         * @param pixelPoint The X, Y coordinate in pixels
         * @return A boolean to say whether it succeeded, true means it did
         */
        bool wcsToPixel(const FITSImage::wcs_point &skyPoint, QPointF &pixelPoint) override;



    protected:

        //This boolean gets set internally if we are using a downsampled image buffer for SEP
        bool usingDownsampledImage = false;

        /**
         * @brief runSEPExtractor is the method that actually runs internal SEP
         * @return
         */
        int runSEPExtractor();

        /**
         * @brief applyStarFilters filters the stars list so that the list can be reduced for faster solving
         * @param starList
         */
        void applyStarFilters(QList<FITSImage::Star> &starList);

        /**
         * @brief extractPartition actually performs star extraction in separate threads for different parts of the image
         * @param parameters The details about the image partition
         * @return A QList containing Stars with all the details found during the operation
         */
        QList<FITSImage::Star> extractPartition(const ImageParams &parameters);

        /**
         * @brief allocateDataBuffer allocates the space needed for the image buffer object used by SEP
         * @param data is the image buffer used by SEP being allocated
         * @param x is the starting x coordinate of the partition to be processed
         * @param y is the starting y coordinate of the partition to be processed
         * @param w is the width of the partition to be processed
         * @param h is the height of the partition to be processed
         */
        void allocateDataBuffer(float *data, uint32_t x, uint32_t y, uint32_t w, uint32_t h);


    private:

        // The generic data buffer containing the image data
        uint8_t *downSampledBuffer { nullptr };

        // This is the number of threads used for star extraction with SEP
        uint32_t m_PartitionThreads = {16};

        // Job File related
        job_t thejob;                   //This is the job file that will be created for astrometry.net to solve
        job_t* job = &thejob;           //This is a pointer to that job file

        // Solution related
        MatchObj match;                 //This is where the match object gets stored once the solving is done.
        sip_t wcs;                      //This is where the WCS data gets saved once the solving is done

        // Logging related
        FILE *logFile = nullptr;        // This is the name of the log file used
        AstrometryLogger astroLogger;  // This is an object that lets C based astrometry report to C++ based code

    // InternalExtractorSolver Methods

        /**
         * @brief prepare_job prepares the job object used by the internal astrometry solver
         * @return true if successful
         */
        bool prepare_job();

        /**
         * @brief run starts the InternalExtractorSolver to do SEP or solving in a separate thread.
         */
        void run() override;

        /**
         * @brief runInternalSolver is the method that actually runs the internal solver
         * @return 0 if it is successful
         */
        int runInternalSolver();

        /**
         * @brief getFloatBuffer gets a float buffer from the image buffer for SEP to perform star extraction
         * @param buffer is a pointer to the created image buffer
         * @param x is the starting x coordinate of the buffer
         * @param y is the starting y coordinate of the buffer
         * @param w is the width of the buffer
         * @param h is the height of the buffer
         */
        template <typename T> void getFloatBuffer(float * buffer, int x, int y, int w, int h);

        /**
         * @brief downsampleImage downsamples the image by the requested factor
         * @param d The factor to downsample by in both dimensions
         */
        void downsampleImage(int d);

        /**
         * @brief downSampleImageType allows the downsampling code to handle various data types
         * @param d The factor to downsample by in both dimensions
         */
        template <typename T> void downSampleImageType(int d);


};

