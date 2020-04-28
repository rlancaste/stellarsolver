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

//CFitsio Includes
#include "fitsio.h"

//KStars related includes
#include "stretch.h"

//Sextractor Includes
#include "sep/sep.h"

//Astrometry.net includes
extern "C"{
#include "blindutils.h"
#include "log.h"
#include "engine.h"
#include "sip-utils.h"
}

//These are from augment-xylist in astrometry.net and are needed for the scale calculation
#define SCALE_UNITS_DEG_WIDTH 0
#define SCALE_UNITS_ARCMIN_WIDTH 1
#define SCALE_UNITS_ARCSEC_PER_PIX 2
#define SCALE_UNITS_FOCAL_MM 3

class SexySolver : public QThread
{
    Q_OBJECT
public:
    explicit SexySolver(Statistic imagestats,  uint8_t *imageBuffer, QObject *parent = nullptr);
    ~SexySolver();

    //SEXYSOLVER PARAMETERS
    //These are the parameters used by the Internal SexySolver Sextractor and Internal SexySolver Astrometry Solver
    //These parameters are public so that they can be changed by the program using the solver
    //The values here are the defaults unless they get changed.
    //If you are fine with those defaults, you don't need to set any of them.

    QString command;                        //This is a string in which to store the name of the command being run for later access

    typedef struct
    {
        QString listName = "Default";       //This is the name of this particular profile of options for SexySolver

        //Sextractor Photometry Parameters
        Shape apertureShape = SHAPE_CIRCLE; //Whether to use the SEP_SUM_ELLIPSE method or the SEP_SUM_CIRCLE method
        double kron_fact = 2.5;             //This sets the Kron Factor for use with the kron radius for flux calculations.
        int subpix = 5;                     //The subpix setting.  The instructions say to make it 5
        float r_min = 3.5;                  //The minimum radius for stars for flux calculations.
        short inflags;                      //Note sure if we need them?

        //Sextractor Extraction Parameters
        bool calculateHFR = false;          //Whether or not the HFR of the image should be calculated using sep_flux_radius.  Don't do it unless you need HFR
        float magzero = 20;                 //This is the 'zero' magnitude used for settting the magnitude scale for the stars in the image during sextraction.
        float minarea = 5;                  //This is the minimum area in pixels for a star detection, smaller stars are ignored.
        int deblend_thresh = 32;            //The number of thresholds the intensity range is divided up into.
        float deblend_contrast = 1;         //The percentage of flux a separate peak must # have to be considered a separate object.
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
        double removeBrightest = 0;         //The percentage of brightest stars to remove from the list
        double removeDimmest = 0;           //The percentage of dimmest stars to remove from the list
        double saturationLimit = 0;         //Remove all stars above a certain threshhold percentage of saturation

        //Astrometry Config/Engine Parameters
        bool inParallel = true;             //Check the indices in parallel? if the indices you are using take less than 2 GB of space, and you have at least as much physical memory as indices, you want this enabled,
        int solverTimeLimit = 600;          //Give up solving after the specified number of seconds of CPU time
        double minwidth = 0.1;              //If no scale estimate is given, this is the limit on the minimum field width in degrees.
        double maxwidth = 180;              //If no scale estimate is given, this is the limit on the maximum field width in degrees.

        //Astrometry Basic Parameters
        bool resort = true;                 //Whether to resort the stars based on magnitude NOTE: This is REQUIRED to be true for the filters above
        int downsample = 1;                 //Factor to use for downsampling the image before SEP for plate solving.  Can speed it up.  Note: This should ONLY be used for SEP used for solving, not for Sextraction
        int search_parity = PARITY_BOTH;    //Only check for matches with positive/negative parity (default: try both)
        double search_radius = 15;          //Only search in indexes within 'radius' of the field center given by RA and DEC

        //Logging Settings for Astrometry
        bool logToFile = false;             //This determines whether or not to save the output from Astrometry.net to a file
        QString logFile;                    //This is the path to the log file that it will save.
        int logLevel = LOG_NONE;            //This is the level of logging getting saved to the log file

        //LogOdds Settings
        double logratio_tosolve = log(1e9); //Odds ratio at which to consider a field solved (default: 1e9)
        double logratio_tokeep  = log(1e9); //Odds ratio at which to keep a solution (default: 1e9)
        double logratio_totune  = log(1e6); //Odds ratio at which to try tuning up a match that isn't good enough to solve (default: 1e6)
    } Parameters;

    //These are for creating temporary files
    QString baseName;                   //This is the base name used for all temporary files.  It uses a random name based on the type of solver/sextractor.
    QString basePath = QDir::tempPath();//This is the path used for saving any temporary files.  They are by default saved to the default temp directory, you can change it if you want to.

    //SEXYSOLVER METHODS
    //The public methods here are for you to start, stop, setup, and get results from the SexySolver

    //These are the most important methods that you can use for the SexySolver
    void sextract();                    //This will run the Internal SexySolver (SEP) Sextractor
    void sextractAndSolve();            //This will run both the Internal SexySolver Sextractor (SEP) and Solver (astrometry.net)
    void abort();                       //This will abort the solver

    //These set the settings for the SexySolver
    void setParameters(Parameters parameters){params = parameters;};
    void setIndexFolderPaths(QStringList indexPaths){indexFolderPaths = indexPaths;};
    void setSearchScale(double fov_low, double fov_high, QString scaleUnits);                              //This sets the scale range for the image to speed up the solver
    void setSearchPosition(double ra, double dec);                                                    //This sets the search RA/DEC/Radius to speed up the solver

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
    Parameters getCurrentParameters(){return params;};
    bool isUsingScale(){return use_scale;};
    bool isUsingPosition(){return use_position;};



protected:  //Note: These items are not private because they are needed by ExternalSextractorSolver

    Parameters params;           //The currently set parameters for SexySolver

    QStringList indexFolderPaths = getDefaultIndexFolderPaths();       //This is the list of folder paths that the solver will use to search for index files

    //Astrometry Scale Parameters, These are not saved parameters and change for each image, use the methods to set them
    bool use_scale = false;             //Whether or not to use the image scale parameters
    double scalelo = 0;                 //Lower bound of image scale estimate
    double scalehi = 0;                 //Upper bound of image scale estimate
    QString units;                      //A String Representation of the Scale units
    int scaleunit = 0;                  //In what units are the lower and upper bounds?

    //Astrometry Position Parameters, These are not saved parameters and change for each image, use the methods to set them
    bool use_position = false;          //Whether or not to use initial information about the position
    double search_ra = HUGE_VAL;        //RA of field center for search, format: decimal degrees
    double search_dec = HUGE_VAL;       //DEC of field center for search, format: decimal degrees

    //SexySolver Internal settings that are needed by ExternalSextractorSolver as well
    bool justSextract = false;          //This boolean determines whether the solver should proceeed to solve the image, or just sextract
    bool hasSextracted = false;         //This boolean is set when the sextraction is done
    bool hasSolved = false;             //This boolean is set when the solving is done
    Statistic stats;                    //This is information about the image
    uint8_t *m_ImageBuffer { nullptr }; //The generic data buffer containing the image data
    bool usingDownsampledImage = false; //This boolean gets set internally if we are using a downsampled image buffer for SEP

    //The Results
    QList<Star> stars;          //This is the list of stars that get sextracted from the image, saved to the file, and then solved by astrometry.net
    Solution solution;          //This is the solution that comes back from the Solver

    bool runSEPSextractor();    //This is the method that actually runs the internal sextractor

private:

    // The generic data buffer containing the image data
    uint8_t *downSampledBuffer { nullptr };

    // This is the cancel file path that astrometry.net monitors.  If it detects this file, it aborts the solve
    QString cancelfn;           //Filename whose creation signals the process to stop
    QString solvedfn;           //Filename whose creation tells astrometry.net it already solved the field.

    //Job File related stuff
    bool prepare_job();         //This prepares the job object for the solver
    job_t thejob;               //This is the job file that will be created for astrometry.net to solve
    job_t* job = &thejob;       //This is a pointer to that job file

    //These are the key internal methods for the internal SexySolver solver
    void run() override;        //This starts the sexysolver in a separate thread.  Note, ExternalSextractorSolver uses QProcess
    int runInternalSolver();    //This is the method that actually runs the internal solver

    //This is used by the sextractor, it gets a new representation of the buffer that SEP can understand
    template <typename T>
    void getFloatBuffer(float * buffer, int x, int y, int w, int h);

    //This can downsample the image by the requested amount.
    void downsampleImage(int d);
    template <typename T>
    void downSampleImageType(int d);

signals:

    //This signals that there is infomation that should be printed to a log file or log window
    void logNeedsUpdating(QString logText);

    //This signals that the sextraction or image solving is complete, whether they were successful or not
    //A -1 or some positive value should signify failure, where a 0 should signify success.
    void finished(int x);

};

#endif // SEXYSOLVER_H
