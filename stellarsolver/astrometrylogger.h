#ifndef ASTROMETRYLOGGER_H
#define ASTROMETRYLOGGER_H

#ifdef __cplusplus
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

class AstrometryLogger: public QObject
{
    Q_OBJECT
public:
    AstrometryLogger();
    void logFromAstrometry(char* text);
private:
    QString logText;
    QElapsedTimer timeSinceLastOutput;
    QTimer logUpdater;
    void updateLog();
    bool readyToLog();
signals:
    //This signals that there is infomation that should be printed to a log file or log window
    void logOutput(QString logText);
};
#else
typedef struct AstrometryLogger AstrometryLogger;
#endif

#ifdef __cplusplus
    #define EXPORT_C extern "C"
#else
    #define EXPORT_C
#endif

EXPORT_C void logFromAstrometry(AstrometryLogger* astroLogger, char* text);

#endif // ASTROMETRYLOGGER_H
