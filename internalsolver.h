#ifndef INTERNALSOLVER_H
#define INTERNALSOLVER_H

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

class InternalSolver : public QThread
{
    Q_OBJECT
public:
    explicit InternalSolver(Statistic imagestats,  uint8_t *imageBuffer, bool sextractOnly, QObject *parent = nullptr);


    //These are the parameters used by the Internal Sextractor and Internal Astrometry Solver
    //These parameters are public so that they can be changed by the program using the solver
    //The values here are the defaults unless they get changed.

    QString basePath = QDir::tempPath();

    // This is the xyls file path that sextractor will be saving for Astrometry.net
    QString sextractorFilePath = basePath + QDir::separator() + "SextractorList.xyls";

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
    //You should use the method below to create the conv filter based on the fwhm
    QVector<float> convFilter= {0.260856, 0.483068, 0.260856,
                                0.483068, 0.894573, 0.483068,
                                0.260856, 0.483068, 0.260856};

    //Star Filter Parameters
    bool resort = true;  //NOTE: This is REQUIRED for the filters
    double maxEllipse = 0;
    double removeBrightest = 0;
    double removeDimmest = 0;

    //Astrometry Config/Engine Parameters
    QStringList indexFolderPaths;
    bool inParallel = true;
    int solverTimeLimit = 600;
    double minwidth = 0.1;
    double maxwidth = 180;

    //These are used for reading and writing the sextractor file
    char* xcol=strdup("X_IMAGE");
    char* ycol=strdup("Y_IMAGE");
    char* colFormat=strdup("1E");
    char* colUnits=strdup("pixels");

    //Astrometry Scale Parameters, it is best to use the methods below to set them
    bool use_scale = false;
    double scalelo = 0;
    double scalehi = 0;
    int scaleunit = 0;

    //Astrometry Position Parameters, it is best to use the methods below to set them
    bool use_position = false;
    double search_ra = HUGE_VAL;
    double search_dec = HUGE_VAL;
    double search_radius = 15;

    //Logging Settings for Astrometry
    bool logToFile = false;
    QString logFile = QDir::tempPath() + "/AstrometryLog.txt";
    int logLevel = 0;

    //LogOdds Settings
    double logratio_tosolve = log(1e9);
    double logratio_tokeep  = log(1e9);
    double logratio_totune  = log(1e6);

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

    QList<Star> getStarList(){return stars;}

    Solution solution;

    int getNumStarsFound(){return stars.size();};

private:

    // This is the cancel file path that astrometry.net monitors.  If it detects this file, it aborts the solve
    QString cancelfn;
    // This is the list of stars that get sextracted from the image, saved to the file, and then solved by astrometry.net
    QList<Star> stars;
    // This is information about the image
    Statistic stats;
    // This boolean determines whether the solver should proceeed to solve the image, or just sextract
    bool justSextract;

    // The generic data buffer containing the image data
    uint8_t *m_ImageBuffer { nullptr };
    //This is the job file that will be created for astrometry.net to solve
    job_t thejob;
    job_t* job = &thejob;

    //These are for the internal sextractor
    bool writeSextractorTable();
    bool runInnerSextractor();

    //These are for the internal solver
    int runAstrometryEngine();
    bool prepare_job();

    //This is used by the sextractor
    template <typename T>
    void getFloatBuffer(float * buffer, int x, int y, int w, int h);

signals:
    //This signals that the sextractor has found stars
    void starsFound();
    //This signals that there is infomation that should be printed to a log file or log window
    void logNeedsUpdating(QString logText);
    //This signals that astrometry.net has finished solving the image or has quit solving
    void finished(int x);

};

#endif // INTERNALSOLVER_H
