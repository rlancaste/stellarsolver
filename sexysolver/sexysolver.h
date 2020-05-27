/*  SexySolver, SexySolver Intenal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#ifndef SEXYSOLVER_H
#define SEXYSOLVER_H

//Includes for this project
#include "structuredefinitions.h"
#include "sextractorsolver.h"

//QT Includes
#include <QDir>
#include <QThread>
#include <QMap>
#include <QVariant>
#include <QVector>
#include <QRect>

class SexySolver : public QThread
{
    Q_OBJECT
public:
    //This defines the type of process to perform.

    ProcessType processType;

    //The constructor and destructor fo the SexySolver Object
    explicit SexySolver(ProcessType type, Statistic imagestats, uint8_t const *imageBuffer, QObject *parent = nullptr);
    explicit SexySolver(Statistic imagestats,  uint8_t const *imageBuffer, QObject *parent = nullptr);
    ~SexySolver();

    //This gets the processType as a string explaining the command SexySolver is Running
    QString getCommandString()
    {
        switch(processType)
        {
            case INT_SEP:
                return "Int. SEP";
                break;
            case INT_SEP_HFR:
                return "Int. SEP w/ HFR";
                break;
            case EXT_SEXTRACTOR:
                return "Ext. SExtractor";
                break;
            case EXT_SEXTRACTOR_HFR:
                return "Ext. SExtractor w/ HFR";
                break;
            case SEXYSOLVER:
                return "Int. SEP & Int. Solver";
                break;
            case EXT_SEXTRACTORSOLVER:
                return "Ext. SExtractor & Solver";
                break;
            case INT_SEP_EXT_SOLVER:
                return "Int. SEP & Ext. Solver";
                break;
            case CLASSIC_ASTROMETRY:
                return "Classic Astrometry.net";
                break;
            case ASTAP:
                return "ASTAP Solver";
                break;
            case INT_SEP_EXT_ASTAP:
                return "Int. SEP & Ext. ASTAP Solver";
                break;
            case ONLINE_ASTROMETRY_NET:
                return "Online Solver";
                break;
            case INT_SEP_ONLINE_ASTROMETRY_NET:
                return "Int. SEP & Online Solver";
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
    void sextract();
    void sextract(QRect frame);
    void sextractWithHFR();
    void sextractWithHFR(QRect frame);

    void startsextraction();
    void startSextractionWithHFR();
    void solve();
    virtual void executeProcess();                      //This runs the process without threading.
    virtual void startProcess();                        //This starts the process in a separate thread
    virtual void abort();                       //This will abort the solver

    //These set the settings for the SexySolver
    void setParameters(Parameters parameters){params = parameters;};
    void setParameterProfile(ParametersProfile profile);
    void setIndexFolderPaths(QStringList indexPaths){indexFolderPaths = indexPaths;};
    void setUseScale(bool set){use_scale = set;};
    void setSearchScale(double fov_low, double fov_high, QString scaleUnits);
    void setSearchScale(double fov_low, double fov_high, ScaleUnits units); //This sets the scale range for the image to speed up the solver
    void setUsePostion(bool set){use_position = set;};
    void setSearchPositionRaDec(double ra, double dec);                                                    //This sets the search RA/DEC/Radius to speed up the solver
    void setSearchPositionInDegrees(double ra, double dec);
    void setProcessType(ProcessType type){processType = type;};
    void setLogToFile(bool change){logToFile = change;};
    void setLogLevel(log_level level){logLevel = level;};

    //These static methods can be used by classes to configure parameters or paths
    static void createConvFilterFromFWHM(Parameters *params, double fwhm);                      //This creates the conv filter from a fwhm
    static QList<Parameters> getBuiltInProfiles();
    static QStringList getDefaultIndexFolderPaths();


    //Accessor Method for external classes
    int getNumStarsFound(){return numStars;}
    QList<Star> getStarList(){return stars;}
    Background getBackground(){return background;}
    QList<Star> getStarListFromSolve(){return starsFromSolve;}
    Solution getSolution(){return solution;}

    bool sextractionDone(){return hasSextracted;}
    bool solvingDone(){return hasSolved;}
    bool failed(){return hasFailed;}
    void setLoadWCS(bool set){loadWCS = set;}
    bool hasWCSData(){return hasWCS;};
    int getNumThreads(){if(parallelSolvers.size()==0) return 1; else return parallelSolvers.size();}

    Parameters getCurrentParameters(){return params;}
    bool isCalculatingHFR(){return calculateHFR;}
    bool isUsingScale(){return use_scale;}
    bool isUsingPosition(){return use_position;}


    virtual wcs_point *getWCSCoord();
    virtual QList<Star> appendStarsRAandDEC(QList<Star> stars);

    static QMap<QString,QVariant> convertToMap(Parameters params);
    static Parameters convertFromMap(QMap<QString,QVariant> settingsMap);

    //External Options
    QString fileToProcess;
    bool cleanupTemporaryFiles = true;
    bool autoGenerateAstroConfig = true;

    //System File Paths
    QStringList indexFilePaths;
    QString astapBinaryPath;
    QString sextractorBinaryPath;
    QString confPath;
    QString solverPath;
    QString wcsPath;

    //Online Options
    QString astrometryAPIKey;
    QString astrometryAPIURL;

    uint64_t getAvailableRAM(); //This finds out the amount of available RAM on the system
    bool enoughRAMisAvailableFor(QStringList indexFolders);  //This determines if there is enough RAM for the selected index files so that we don't try to load indexes inParallel unless it can handle it.

    void setUseSubframe(QRect frame);
    void clearSubFrame(){useSubframe = false; subframe = QRect(0,0,stats.width,stats.height);};
public slots:
    void processFinished(int code);
    void parallelSolve();
    void finishParallelSolve(int success);

protected:  //Note: These items are not private because they are needed by ExternalSextractorSolver

    bool useSubframe = false;
    QRect subframe;
    QList<SextractorSolver*> parallelSolvers;
    SextractorSolver *sextractorSolver = nullptr;
    SextractorSolver *solverWithWCS = nullptr;
    int parallelFails = 0;
    bool parallelSolversAreRunning();

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
    bool hasFailed = false;
    Statistic stats;                    //This is information about the image
    const uint8_t *m_ImageBuffer { nullptr }; //The generic data buffer containing the image data

    //The Results
    Background background;      //This is a report on the background levels found during sextraction
    QList<Star> stars;          //This is the list of stars that get sextracted from the image, saved to the file, and then solved by astrometry.net
    QList<Star> starsFromSolve; //This is the list of stars that were sextracted for the last successful solve
    int numStars;               //The number of stars found in the last operation
    Solution solution;          //This is the solution that comes back from the Solver
    bool loadWCS = true;
    bool hasWCS = false;        //This boolean gets set if the SexySolver has WCS data to retrieve
    wcs_point * wcs_coord = nullptr;

    bool wasAborted = false;
    // This is the cancel file path that astrometry.net monitors.  If it detects this file, it aborts the solve
    QString cancelfn;           //Filename whose creation signals the process to stop
    QString solvedfn;           //Filename whose creation tells astrometry.net it already solved the field.

private:
    bool checkParameters();
    void run() override;
    SextractorSolver* createSextractorSolver();

signals:

    //This signals that there is infomation that should be printed to a log file or log window
    void logOutput(QString logText);

    //This signals that the sextraction or image solving is complete, whether they were successful or not
    //A -1 or some positive value should signify failure, where a 0 should signify success.
    void finished(int x);

    void wcsDataisReady();

};

#endif // SEXYSOLVER_H
