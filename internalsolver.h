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

    //Sextractor Extraction Parameters

    //Sextractor Photometry Parameters
    Shape apertureShape = SHAPE_AUTO;
    double kron_fact = 2.5;
    int subpix = 5;
    float r_min = 3.5;
    short inflags;
    float magzero = 20;
    float minarea = 5;
    int deblend_thresh = 32;
    float deblend_contrast = 0.005;
    int clean = 1;
    double clean_param = 1;
    QVector<float> convFilter;
    double fwhm;

    //Astrometry Parameters

    void run() override;
    template <typename T>
    void getFloatBuffer(float * buffer, int x, int y, int w, int h);

    //These are for the internal sextractor
    bool writeSextractorTable();
    bool runInnerSextractor();

    //These are for the internal solver
    bool augmentXYList();
    int runAstrometryEngine();

    QList<Star> getStarList(){return stars;}

    augment_xylist_t* solverParams(){return allaxy;}

    Solution solution;

    int getNumStarsFound(){return stars.size();};


private:

    QString fileToSolve;
    QString sextractorFilePath;

    QList<Star> stars;
    Statistic stats;
    bool justSextract;

    // Generic data image buffer
    uint8_t *m_ImageBuffer { nullptr };
    //The solving arguments list
    augment_xylist_t theallaxy;
    augment_xylist_t* allaxy = &theallaxy;

signals:
    void starsFound();
    void logNeedsUpdating(QString logText);
    void finished(int x);

};

#endif // INTERNALSOLVER_H
