/*  SexySolver Intenal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#ifndef SEXYSOLVER_H
#define SEXYSOLVER_H

//Includes for this project
#include "structuredefinitions.h"

//QT Includes
#include <QDir>
#include <QThread>
#include <QMap>
#include <QVariant>
#include <QVector>

//CFitsio Includes
#include "fitsio.h"

//Sextractor Includes
#include "sep/sep.h"

//Astrometry.net includes
extern "C"{
#include "blindutils.h"
#include "log.h"
#include "engine.h"
#include "sip-utils.h"
}

typedef enum{DEG_WIDTH,
             ARCMIN_WIDTH,
             ARCSEC_PER_PIX,
             FOCAL_MM
}ScaleUnits;

class SexySolver : public QThread
{
    Q_OBJECT
public:
    //This defines the type of process to perform.
    typedef enum {INT_SEP,
                  INT_SEP_HFR,
                  EXT_SEXTRACTOR,
                  EXT_SEXTRACTOR_HFR,
                  SEXYSOLVER,
                  EXT_SEXTRACTORSOLVER,
                  INT_SEP_EXT_SOLVER,
                  CLASSIC_ASTROMETRY,
                  ASTAP,
                  INT_SEP_EXT_ASTAP,
                  ONLINE_ASTROMETRY_NET,
                  INT_SEP_ONLINE_ASTROMETRY_NET
    }ProcessType;
    ProcessType processType;

    typedef enum {NOT_MULTI,
                  MULTI_SCALES,
                  MULTI_DEPTHS,
                  MULTI_AUTO
    }MultiAlgo;

    //The constructor and destructor fo the SexySolver Object
    explicit SexySolver(ProcessType type, Statistic imagestats,  uint8_t *imageBuffer, QObject *parent = nullptr);
    ~SexySolver();

    //This generates a SexySolver Object of the SexySolver, ExternalSextractorSolver, or OnlineSolver types
    static SexySolver *createSexySolver(ProcessType type, Statistic imagestats,  uint8_t *imageBuffer, QObject *parent = nullptr);

    //This gets the processType as a string explaining the command SexySolver is Running
    QString getCommandString()
    {
        switch(processType)
        {
            case SexySolver::INT_SEP:
                return "Internal SEP";
                break;
            case SexySolver::INT_SEP_HFR:
                return "Internal SEP w/ HFR";
                break;
            case SexySolver::EXT_SEXTRACTOR:
                return "Ext Sextractor";
                break;
            case SexySolver::EXT_SEXTRACTOR_HFR:
                return "Ext Sextractor w/ HFR";
                break;
            case SexySolver::SEXYSOLVER:
                return "SexySolver";
                break;
            case SexySolver::EXT_SEXTRACTORSOLVER:
                return "Ext Sextractor Ext Solver";
                break;
            case SexySolver::INT_SEP_EXT_SOLVER:
                return "Int SEP Ext Solver";
                break;
            case SexySolver::CLASSIC_ASTROMETRY:
                return "Classic Astrometry.net";
                break;
            case SexySolver::ASTAP:
                return "ASTAP Solver";
                break;
            case SexySolver::INT_SEP_EXT_ASTAP:
                return "INT SEP EXT ASTAP Solver";
                break;
            case SexySolver::ONLINE_ASTROMETRY_NET:
                return "Online Astrometry.net";
                break;
            case SexySolver::INT_SEP_ONLINE_ASTROMETRY_NET:
                return "Int SEP Online Astrometry.net";
                break;
            default: return ""; break;
        }
    };

    //This gets the scale unit string for astrometry.net input
    //This should NOT be translated
    QString getScaleUnitString()
    {
        switch(scaleunit)
        {
            case DEG_WIDTH:
                return "degwidth";
                break;
            case ARCMIN_WIDTH:
                return "arcminwidth";
                break;
            case ARCSEC_PER_PIX:
                return "arcsecperpix";
                break;
            case FOCAL_MM:
                return "focalmm";
            break;
            default: return ""; break;
        }
    }

    //This gets a string for the Sextractor setting for calculating Flux using ellipses or circles
    QString getShapeString()
    {
        switch(params.apertureShape)
        {
            case SHAPE_AUTO:
                return "Auto";
            break;

            case SHAPE_CIRCLE:
               return "Circle";
            break;

            case SHAPE_ELLIPSE:
                return "Ellipse";
            break;
            default: return ""; break;
        }
    }

    //This gets a string for which Parallel Solving Algorithm we are using
    QString getMultiAlgoString()
    {
        switch(params.multiAlgorithm)
        {
            case NOT_MULTI:
                return "None";
            break;

            case MULTI_SCALES:
               return "Scales";
            break;

            case MULTI_DEPTHS:
                return "Depths";
            break;
            default: return ""; break;
        }
    }

    QString getLogLevelString()
    {
        switch(logLevel)
        {
            case LOG_NONE:
                return "None";
            break;

            case LOG_ERROR:
               return "Error";
            break;

            case LOG_MSG:
                return "Message";
            break;

            case LOG_VERB:
                return "Verbose";
            break;

            case LOG_ALL:
                return "All";
            break;

            default: return ""; break;
        }
    }

    //SEXYSOLVER PARAMETERS
    //These are the parameters used by the Internal SexySolver Sextractor and Internal SexySolver Astrometry Solver
    //The values here are the defaults unless they get changed.
    //If you are fine with those defaults, you don't need to set any of them.

    struct Parameters
    {
        QString listName = "Default";       //This is the name of this particular profile of options for SexySolver

        //Sextractor Photometry Parameters
        Shape apertureShape = SHAPE_CIRCLE; //Whether to use the SEP_SUM_ELLIPSE method or the SEP_SUM_CIRCLE method
        double kron_fact = 2.5;             //This sets the Kron Factor for use with the kron radius for flux calculations.
        int subpix = 5;                     //The subpix setting.  The instructions say to make it 5
        double r_min = 3.5;                  //The minimum radius for stars for flux calculations.
        short inflags;                      //Note sure if we need them?

        //Sextractor Extraction Parameters
        double magzero = 20;                 //This is the 'zero' magnitude used for settting the magnitude scale for the stars in the image during sextraction.
        double minarea = 5;                  //This is the minimum area in pixels for a star detection, smaller stars are ignored.
        int deblend_thresh = 32;            //The number of thresholds the intensity range is divided up into.
        double deblend_contrast = 0.005;         //The percentage of flux a separate peak must # have to be considered a separate object.
        int clean = 1;                      //Attempts to 'clean' the image to remove artifacts caused by bright objects
        double clean_param = 1;             //The cleaning parameter, not sure what it does.
        double fwhm = 2;                    //A variable to store the fwhm used to generate the conv filter, changing this WILL NOT change the conv filter, you can use the method below to create the conv filter based on the fwhm

        //This is the filter used for convolution. You can create this directly or use the convenience method below.
        QVector<float> convFilter= {0.260856, 0.483068, 0.260856,
                                    0.483068, 0.894573, 0.483068,
                                    0.260856, 0.483068, 0.260856};

        //Star Filter Parameters
        double maxSize = 0;                 //The maximum size of stars to include in the final list in pixels based on semi-major and semi-minor axes
        double maxEllipse = 0;              //The maximum ratio between the semi-major and semi-minor axes for stars to include (a/b)
        double keepNum = 0;                 //The number of brightest stars to keep in the list
        double removeBrightest = 0;         //The percentage of brightest stars to remove from the list
        double removeDimmest = 0;           //The percentage of dimmest stars to remove from the list
        double saturationLimit = 0;         //Remove all stars above a certain threshhold percentage of saturation

        //Astrometry Config/Engine Parameters
        MultiAlgo multiAlgorithm = NOT_MULTI;//Algorithm for running multiple threads on possibly multiple cores to solve faster
        bool inParallel = true;             //Check the indices in parallel? if the indices you are using take less than 2 GB of space, and you have at least as much physical memory as indices, you want this enabled,
        int solverTimeLimit = 600;          //Give up solving after the specified number of seconds of CPU time
        double minwidth = 0.1;              //If no scale estimate is given, this is the limit on the minimum field width in degrees.
        double maxwidth = 180;              //If no scale estimate is given, this is the limit on the maximum field width in degrees.

        //Astrometry Basic Parameters
        bool resort = true;                 //Whether to resort the stars based on magnitude NOTE: This is REQUIRED to be true for the filters above
        int downsample = 1;                 //Factor to use for downsampling the image before SEP for plate solving.  Can speed it up.  Note: This should ONLY be used for SEP used for solving, not for Sextraction
        int search_parity = PARITY_BOTH;    //Only check for matches with positive/negative parity (default: try both)
        double search_radius = 15;          //Only search in indexes within 'radius' of the field center given by RA and DEC

        //LogOdds Settings
        double logratio_tosolve = log(1e9); //Odds ratio at which to consider a field solved (default: 1e9)
        double logratio_tokeep  = log(1e9); //Odds ratio at which to keep a solution (default: 1e9)
        double logratio_totune  = log(1e6); //Odds ratio at which to try tuning up a match that isn't good enough to solve (default: 1e6)

        //This allows you to compare the Parameters to see if they match
        bool operator==(const Parameters& o)
        {
            //We will skip the list name, since we want to see if the contents are the same.
            return  apertureShape == o.apertureShape &&
                    kron_fact == o.kron_fact &&
                    subpix == o.subpix &&
                    r_min == o.r_min &&
                    //skip inflags??  Not sure we even need them

                    magzero == o.magzero &&
                    deblend_thresh == o.deblend_thresh &&
                    deblend_contrast == o.deblend_contrast &&
                    clean == o.clean &&
                    clean_param == o.clean_param &&
                    fwhm == o.fwhm &&
                    //skip conv filter?? This might be hard to compare

                    maxSize == o.maxSize &&
                    maxEllipse == o.maxEllipse &&
                    keepNum == o.keepNum &&
                    removeBrightest == o.removeBrightest &&
                    removeDimmest == o.removeDimmest &&
                    saturationLimit == o.saturationLimit &&

                    multiAlgorithm == o.multiAlgorithm &&
                    inParallel == o.inParallel &&
                    solverTimeLimit == o.solverTimeLimit &&
                    minwidth == o.minwidth &&
                    maxwidth == o.maxwidth &&

                    resort == o.resort &&
                    downsample == o.downsample &&
                    search_parity == o.search_parity &&
                    search_radius == o.search_radius &&
                    //They need to be turned into a qstring because they are sometimes very close but not exactly the same
                    QString::number(logratio_tosolve) == QString::number(o.logratio_tosolve) &&
                    QString::number(logratio_tokeep) == QString::number(o.logratio_tokeep) &&
                    QString::number(logratio_totune) == QString::number(o.logratio_totune);
        }
    } ;

    //Logging Settings for Astrometry
    bool logToFile = false;             //This determines whether or not to save the output from Astrometry.net to a file
    QString logFileName;                //This is the path to the log file that it will save.
    log_level logLevel = LOG_MSG;       //This is the level of logging getting saved to the log file

    //These are for creating temporary files
    QString baseName;                   //This is the base name used for all temporary files.  It uses a random name based on the type of solver/sextractor.
    QString basePath = QDir::tempPath();//This is the path used for saving any temporary files.  They are by default saved to the default temp directory, you can change it if you want to.

    //SEXYSOLVER METHODS
    //The public methods here are for you to start, stop, setup, and get results from the SexySolver

    //These are the most important methods that you can use for the SexySolver
    virtual void executeProcess();                      //This runs the process without threading.
    virtual void startProcess();                        //This starts the process in a separate thread
    virtual void abort();                       //This will abort the solver

    //These set the settings for the SexySolver
    void setParameters(Parameters parameters){params = parameters;};
    void setIndexFolderPaths(QStringList indexPaths){indexFolderPaths = indexPaths;};
    void setSearchScale(double fov_low, double fov_high, QString scaleUnits);
    void setSearchScale(double fov_low, double fov_high, ScaleUnits units); //This sets the scale range for the image to speed up the solver
    void setSearchPositionRaDec(double ra, double dec);                                                    //This sets the search RA/DEC/Radius to speed up the solver
    void setSearchPositionInDegrees(double ra, double dec);
    void setProcessType(ProcessType type){processType = type;};
    void setLogToFile(bool change){logToFile = change;};
    void setLogLevel(log_level level){logLevel = level;};
    void setDepths(int low, int high){depthlo=low; depthhi = high;};

    //These static methods can be used by classes to configure parameters or paths
    static void createConvFilterFromFWHM(Parameters *params, double fwhm);                      //This creates the conv filter from a fwhm
    static QList<SexySolver::Parameters> getOptionsProfiles();
    static QStringList getDefaultIndexFolderPaths();

    //Accessor Method for external classes
    int getNumStarsFound(){return stars.size();};
    QList<Star> getStarList(){return stars;}
    Solution getSolution(){return solution;};

    bool sextractionDone(){return hasSextracted;};
    bool solvingDone(){return hasSolved;};
    bool hasWCSData(){return hasWCS;};
    int getNumThreads(){if(childSolvers.size()==0) return 1; else return childSolvers.size();}

    Parameters getCurrentParameters(){return params;};
    bool isCalculatingHFR(){return processType == INT_SEP_HFR || processType == EXT_SEXTRACTOR_HFR;};
    bool isUsingScale(){return use_scale;};
    bool isUsingPosition(){return use_position;};

    double convertToDegreeWidth(double scale){
        switch(scaleunit)
        {
            case DEG_WIDTH:
                return scale;
                break;
            case ARCMIN_WIDTH:
                return arcmin2deg(scale);
                break;
            case ARCSEC_PER_PIX:
                return arcsec2deg(scale) * (double)stats.width;
                break;
            case FOCAL_MM:
                return rad2deg(atan(36. / (2. * scale)));
            break;
            default: return scale; break;
        }
    }

    virtual wcs_point *getWCSCoord();
    virtual QList<Star> appendStarsRAandDEC(QList<Star> stars);

    static QMap<QString,QVariant> convertToMap(Parameters params);
    static Parameters convertFromMap(QMap<QString,QVariant> settingsMap);

protected:  //Note: These items are not private because they are needed by ExternalSextractorSolver

    Parameters params;           //The currently set parameters for SexySolver
    QStringList indexFolderPaths = getDefaultIndexFolderPaths();       //This is the list of folder paths that the solver will use to search for index files

    //Astrometry Scale Parameters, These are not saved parameters and change for each image, use the methods to set them
    bool use_scale = false;             //Whether or not to use the image scale parameters
    double scalelo = 0;                 //Lower bound of image scale estimate
    double scalehi = 0;                 //Upper bound of image scale estimate
    ScaleUnits scaleunit;               //In what units are the lower and upper bounds?

    //Astrometry Position Parameters, These are not saved parameters and change for each image, use the methods to set them
    bool use_position = false;          //Whether or not to use initial information about the position
    double search_ra = HUGE_VAL;        //RA of field center for search, format: decimal degrees
    double search_dec = HUGE_VAL;       //DEC of field center for search, format: decimal degrees

    //SexySolver Internal settings that are needed by ExternalSextractorSolver as well
    bool calculateHFR = false;          //Whether or not the HFR of the image should be calculated using sep_flux_radius.  Don't do it unless you need HFR
    bool hasSextracted = false;         //This boolean is set when the sextraction is done
    bool hasSolved = false;             //This boolean is set when the solving is done
    Statistic stats;                    //This is information about the image
    uint8_t *m_ImageBuffer { nullptr }; //The generic data buffer containing the image data
    bool usingDownsampledImage = false; //This boolean gets set internally if we are using a downsampled image buffer for SEP

    //The Results
    QList<Star> stars;          //This is the list of stars that get sextracted from the image, saved to the file, and then solved by astrometry.net
    Solution solution;          //This is the solution that comes back from the Solver
    bool runSEPSextractor();    //This is the method that actually runs the internal sextractor
    bool hasWCS = false;        //This boolean gets set if the SexySolver has WCS data to retrieve

    bool wasAborted = false;
    // This is the cancel file path that astrometry.net monitors.  If it detects this file, it aborts the solve
    QString cancelfn;           //Filename whose creation signals the process to stop
    QString solvedfn;           //Filename whose creation tells astrometry.net it already solved the field.

    uint64_t getAvailableRAM(); //This finds out the amount of available RAM on the system
    bool enoughRAMisAvailableFor(QStringList indexFolders);  //This determines if there is enough RAM for the selected index files so that we don't try to load indexes inParallel unless it can handle it.

    //These support parallel threads used for solving.
    QList<SexySolver*> childSolvers;        //These are the solvers that this solver will spawn to run in parallel to speed up solving
    bool isChildSolver = false;              //This identifies that this solver is in fact a child solver.
    void parallelSolve();                   //This method breaks up the job and solves in parallel threads
    virtual SexySolver* spawnChildSolver(); //This creates a child solver with all the parameters of the parent solver
    bool runInParallelAndWaitForFinish();   //This will wait for the parallel solvers to complete and return true if it should run in parallel, and return false if it shouldn't run in parallel
    void finishParallelSolve(int success);  //This slot handles when a parallel solve completes
    virtual void getWCSDataFromChildSolver(SexySolver *solver);
    int parallelFails;                      //This counts the number of threads that have failed to determine overall failure
    int depthlo = -1;                       //This is the low depth of this child solver
    int depthhi = -1;                       //This is the high depth of this child solver

private:

    // The generic data buffer containing the image data
    uint8_t *downSampledBuffer { nullptr };

    //Job File related stuff
    bool prepare_job();         //This prepares the job object for the solver
    job_t thejob;               //This is the job file that will be created for astrometry.net to solve
    job_t* job = &thejob;       //This is a pointer to that job file

    //These are the key internal methods for the internal SexySolver solver
    void run() override;        //This starts the SexySolver in a separate thread.  Note, ExternalSextractorSolver uses QProcess
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

signals:

    //This signals that there is infomation that should be printed to a log file or log window
    void logOutput(QString logText);

    //This signals that the sextraction or image solving is complete, whether they were successful or not
    //A -1 or some positive value should signify failure, where a 0 should signify success.
    void finished(int x);

};

#endif // SEXYSOLVER_H
