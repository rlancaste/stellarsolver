#ifndef FILEIO_H
#define FILEIO_H

#include <QObject>
#include <QImageReader>
#include <QFile>

//CFitsio Includes
#include "longnam.h"
#include "fitsio.h"

//KStars related includes
#include "stretch.h"
#include "math.h"
#include "dms.h"
#include "bayer.h"

#include "parameters.h"
#include "structuredefinitions.h"
#include "stellarsolver/sep/sep.h"

class fileio : public QObject
{
    Q_OBJECT
public:
    fileio();
    bool logToSignal = false;
    bool loadImage(QString fileName);
    bool loadFits(QString fileName);
    bool saveAsFITS(QString fileName, FITSImage::Statistic &imageStats, uint8_t *m_ImageBuffer, FITSImage::Solution solution, bool hasSolution);
    bool loadOtherFormat(QString fileName);
    bool checkDebayer();
    bool debayer();
    bool debayer_8bit();
    bool debayer_16bit();
    bool getSolverOptionsFromFITS();

    bool position_given = false;
    double ra;
    double dec;

    bool scale_given = false;
    double scale_low;
    double scale_high;
    SSolver::ScaleUnits scale_units;

    uint8_t *getImageBuffer(){
        return m_ImageBuffer;
    }

    FITSImage::Statistic getStats(){
        return stats;
    }
private:
    QString file;
    fitsfile *fptr { nullptr };
    FITSImage::Statistic stats;
    /// Generic data image buffer
    uint8_t *m_ImageBuffer { nullptr };
    /// Above buffer size in bytes
    uint32_t m_ImageBufferSize { 0 };
    StretchParams stretchParams;
    BayerParams debayerParams;
    void logIssue(QString messsage);

signals:
    void logOutput(QString logText);
};

#endif // FILEIO_H
