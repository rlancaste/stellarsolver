#ifndef INTERNALSOLVER_H
#define INTERNALSOLVER_H

//Includes for this project
#include "structuredefinitions.h"

//system includes
#include "math.h"
#include <sys/mman.h>

//QT Includes
#include <QMainWindow>
#include <QObject>
#include <QWidget>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QProcess>
#include <QPointer>
#include <QScrollBar>
#include <QTime>
#include <QThread>

//CFitsio Includes
#include "longnam.h"
#include "fitsio.h"

//KStars related includes
#include "stretch.h"
#include "math.h"
#include "dms.h"
#include "bayer.h"

//Sextractor Includes
#include "sep/sep.h"

//Astrometry.net includes
extern "C"{
#include "tic.h"
#include "os-features.h"
#include "fileutils.h"
#include "ioutils.h"
#include "bl.h"
#include "an-bool.h"
#include "solver.h"
#include "fitsioutils.h"
#include "blindutils.h"
#include "astrometry/blind.h"
#include "log.h"
#include "engine.h"
#include "an-opts.h"
#include "gslutils.h"
#include "augment-xylist.h"
#include "anqfits.h"
#include "ioutils.h"
#include "fitsioutils.h"
#include "permutedsort.h"
#include "fitstable.h"
#include "sip_qfits.h"
#include "sip-utils.h"
#include "tabsort.h"
}

class InternalSolver : public QThread
{
    Q_OBJECT
public:
    explicit InternalSolver(QString file, QString sextractorFile, Statistic imagestats,  uint8_t *imageBuffer, bool sextractOnly, QObject *parent = nullptr);


    //These are the parameters used by the Internal Sextractor and Internal Astrometry Solver
    //These parameters are public so that they can be changed by the program using the solver
    //The values here are the defaults unless they get changed.

    QString basePath = QDir::tempPath();

    //Sextractor Extraction Parameters

    //Sextractor Photometry Parameters
    Shape apertureShape = SHAPE_CIRCLE;
    double kron_fact = 2.5;
    int subpix = 5;
    float r_min = 3.5;
    short inflags;
    float magzero = 20;
    float minarea = 5;
    int deblend_thresh = 32;
    float deblend_contrast = 1;
    int clean = 1;
    double clean_param = 1;
    QVector<float> convFilter= {0.260856, 0.483068, 0.260856,
                                0.483068, 0.894573, 0.483068,
                                0.260856, 0.483068, 0.260856};

    double fwhm = 2;

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

    char* xcol=strdup("X_IMAGE");
    char* ycol=strdup("Y_IMAGE");
    char* sortcol=strdup("MAG_AUTO");
    bool use_scale = false;
    double scalelo = 0;
    double scalehi = 0;
    int scaleunit = 0;
    bool use_position = false;
    double ra_center = HUGE_VAL;
    double dec_center = HUGE_VAL;
    double search_radius = 15;

    bool logToFile = false;
    QString logFile = QDir::tempPath() + "/AstrometryLog.txt";
    int logLevel = 0;

    //LogOdds Settings
    double logratio_tosolve = log(1e9);
    double logratio_tokeep  = log(1e9);
    double logratio_totune  = log(1e6);

    void setSearchScale(double fov_low, double fov_high, QString units);
    void setSearchPosition(double ra, double dec, double rad);
    void setIndexFolderPaths(QStringList paths);
    void clearIndexFolderPaths();
    void addIndexFolderPath(QString pathToAdd);

    void run() override;
    template <typename T>
    void getFloatBuffer(float * buffer, int x, int y, int w, int h);

    //These are for the internal sextractor
    bool writeSextractorTable();
    bool runInnerSextractor();

    //These are for the internal solver
    int runAstrometryEngine();
    bool prepare_job();
    void abort();

    QList<Star> getStarList(){return stars;}

    Solution solution;

    int getNumStarsFound(){return stars.size();};

    void removeTempFile(char * fileName);


private:

    QString fileToSolve;
    QString sextractorFilePath;

    QList<Star> stars;
    Statistic stats;
    bool justSextract;

    // Generic data image buffer
    uint8_t *m_ImageBuffer { nullptr };

    job_t thejob;
    job_t* job = &thejob;

    char* cancelfn;

signals:
    void starsFound();
    void logNeedsUpdating(QString logText);
    void finished(int x);

};

#endif // INTERNALSOLVER_H
