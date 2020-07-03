/*  InternalSextractorSolver, StellarSolver Intenal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#ifndef INTERNALSEXTRACTORSOLVER_H
#define INTERNALSEXTRACTORSOLVER_H

#include "sextractorsolver.h"

//Sextractor Includes
#include "sep/sep.h"

using namespace SSolver;

class InternalSextractorSolver: public SextractorSolver
{
public:
    explicit InternalSextractorSolver(ProcessType type, SextractorType sexType, SolverType solType, FITSImage::Statistic imagestats,  uint8_t const *imageBuffer, QObject *parent = nullptr);
    ~InternalSextractorSolver();

    int sextract() override;
    //void solve() override;
    void abort() override;
    FITSImage::wcs_point *getWCSCoord() override;
    QList<FITSImage::Star> appendStarsRAandDEC(QList<FITSImage::Star> stars) override;
    SextractorSolver* spawnChildSolver(int n) override;

protected:
    int runSEPSextractor();    //This is the method that actually runs the internal sextractor
    void applyStarFilters();    //This applies the star filter to the stars list.
    bool usingDownsampledImage = false; //This boolean gets set internally if we are using a downsampled image buffer for SEP


private:

    // The generic data buffer containing the image data
    uint8_t *downSampledBuffer { nullptr };

    //Job File related stuff
    bool prepare_job();         //This prepares the job object for the solver
    job_t thejob;               //This is the job file that will be created for astrometry.net to solve
    job_t* job = &thejob;       //This is a pointer to that job file

    //These are the key internal methods for the internal StellarSolver solver
    void run() override;        //This starts the StellarSolver in a separate thread.  Note, ExternalSextractorSolver uses QProcess
    int runInternalSolver();    //This is the method that actually runs the internal solver

    //This is used by the sextractor, it gets a new representation of the buffer that SEP can understand
    template <typename T>
    void getFloatBuffer(float * buffer, int x, int y, int w, int h);

    MatchObj match;             //This is where the match object gets stored once the solving is done.
    sip_t wcs;                  //This is where the WCS data gets saved once the solving is done

    //This can downsample the image by the requested amount.
    void downsampleImage(int d);
    template <typename T>
    void downSampleImageType(int d);

    void startLogMonitor();
    QThread* logMonitor = nullptr;
    bool logMonitorRunning = false;
    FILE *logFile = nullptr;
};

#endif // INTERNALSEXTRACTORSOLVER_H
