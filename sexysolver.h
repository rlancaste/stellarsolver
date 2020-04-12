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

//These are from augment xylist and are needed for the scale calculation
#define SCALE_UNITS_DEG_WIDTH 0
#define SCALE_UNITS_ARCMIN_WIDTH 1
#define SCALE_UNITS_ARCSEC_PER_PIX 2
#define SCALE_UNITS_FOCAL_MM 3

class SexySolver : public QThread
{
    Q_OBJECT
public:
    explicit SexySolver(Statistic imagestats,  uint8_t *imageBuffer, QObject *parent = nullptr);


    //These are the parameters used by the Internal SexySolver Sextractor and Internal SexySolver Astrometry Solver
    //These parameters are public so that they can be changed by the program using the solver
    //The values here are the defaults unless they get changed.

    // This boolean determines whether the solver should proceeed to solve the image, or just sextract
    bool justSextract = false;

    //These are the methods that you can use for the SexySolver to Sextract or Solve
    void sextract();
    void sextractAndSolve();

    //Sextractor Photometry Parameters
    Shape apertureShape = SHAPE_CIRCLE;
    double kron_fact = 2.5;
    int subpix = 5;
    float r_min = 3.5;
    short inflags;

    //Sextractor Extraction Parameters
    float magzero = 20;
    float minarea = 5;
    int deblend_thresh = 32;
    float deblend_contrast = 1;
    int clean = 1;
    double clean_param = 1;
    double fwhm = 2;  //A variable to store the fwhm used to generate the conv filter, changing this WILL NOT change the conv filter
    //You can use the method below to create the conv filter based on the fwhm
    QVector<float> convFilter= {0.260856, 0.483068, 0.260856,
                                0.483068, 0.894573, 0.483068,
                                0.260856, 0.483068, 0.260856};

    //Star Filter Parameters
    bool resort = true;  //Whether to resort the stars based on magnitude. NOTE: This is REQUIRED to be true for the filters
    double maxEllipse = 0; //The maximum ratio between the semi-major and semi-minor axes for stars to include (a/b)
    double removeBrightest = 0; //The percentage of brightest stars to remove from the list
    double removeDimmest = 0; //The percentage of dimmest stars to remove from the list

    //Astrometry Config/Engine Parameters
    QStringList indexFolderPaths; //This is the list of folder paths that the solver will use to search for index files
    bool inParallel = true; //Check the indices in parallel? if the indices you are using take less than 2 GB of space, and you have at least as much physical memory as indices, you want this enabled,
    int solverTimeLimit = 600; //Give up solving after the specified number of seconds of CPU time
    double minwidth = 0.1; //If no scale estimate is given, this is the limit on the minimum field width in degrees.
    double maxwidth = 180; //If no scale estimate is given, this is the limit on the maximum field width in degrees.

    //Astrometry Scale Parameters, you can use the convenience methods to set these
    bool use_scale = false; //Whether or not to use the image scale parameters
    double scalelo = 0; //Lower bound of image scale estimate
    double scalehi = 0; //Upper bound of image scale estimate
    QString units; // A String Representation of the Scale units
    int scaleunit = 0; //In what units are the lower and upper bounds?

    //Astrometry Position Parameters, you can use the convenience methods to set these
    bool use_position = false; //Whether or not to use initial information about the position
    double search_ra = HUGE_VAL; //RA of field center for search, format: decimal degrees
    double search_dec = HUGE_VAL; //DEC of field center for search, format: decimal degrees
    double search_radius = 15; //Only search in indexes within 'radius' of the field center given by RA and DEC

    int search_parity = PARITY_BOTH; //Only check for matches with positive/negative parity (default: try both)


    //Logging Settings for Astrometry
    bool logToFile = false; //This determines whether or not to save the output from Astrometry.net to a file
    QString logFile = QDir::tempPath() + "/AstrometryLog.txt"; //This is the path to the log file that it will save.
    int logLevel = LOG_NONE; //This is the level of logging getting saved to the log file

    //LogOdds Settings
    double logratio_tosolve = log(1e9); //Odds ratio at which to consider a field solved (default: 1e9)
    double logratio_tokeep  = log(1e9);
    double logratio_totune  = log(1e6); //Odds ratio at which to try tuning up a match that isn't good enough to solve (default: 1e6)

    //This starts the sextractor/solver, but DON'T call it.  use start() instead since that will spawn a new thread
    void run() override;
    //This will abort the solver
    void abort();
    //This sets the scale range for the image to speed up the solver
    void setSearchScale(double fov_low, double fov_high, QString units);
    //This sets the search RA/DEC/Radius to speed up the solver
    void setSearchPosition(double ra, double dec, double rad);
    //This creates the conv filter from a fwhm
    void createConvFilterFromFWHM(double fwhm);

    //Methods to set the index file folders for Astrometry
    void setIndexFolderPaths(QStringList paths);
    void clearIndexFolderPaths();
    void addIndexFolderPath(QString pathToAdd);

    // This is the list of stars that get sextracted from the image, saved to the file, and then solved by astrometry.net
    QList<Star> stars;
    int getNumStarsFound(){return stars.size();};
    QList<Star> getStarList(){return stars;}
    //This is the solution that comes back from the Solver
    Solution solution;

private:

    // This is the cancel file path that astrometry.net monitors.  If it detects this file, it aborts the solve
    QString cancelfn; //Filename whose creation signals the process to stop
    // This is information about the image
    Statistic stats;

    // The generic data buffer containing the image data
    uint8_t *m_ImageBuffer { nullptr };

    //This is the job file that will be created for astrometry.net to solve
    job_t thejob;
    job_t* job = &thejob;

    //These are for the internal SexySolver sextractor
    bool runSEPSextractor();

    //These are for the internal SexySolver solver
    int runInternalSolver();
    bool prepare_job();

    //This is used by the sextractor
    template <typename T>
    void getFloatBuffer(float * buffer, int x, int y, int w, int h);

signals:

    //This signals that there is infomation that should be printed to a log file or log window
    void logNeedsUpdating(QString logText);

    //This signals that the sextraction or image solving is complete, whether they were successful or not
    //A -1 should signify failure, where a 0 should signify success.
    void finished(int x);

};

#endif // SEXYSOLVER_H
