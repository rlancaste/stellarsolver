/*  AstrometryLogger, StellarSolver Intenal Library developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/
#ifndef ASTROMETRYLOGGER_H
#define ASTROMETRYLOGGER_H

#ifdef __cplusplus

//Qt Includes
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

class AstrometryLogger: public QObject
{
    Q_OBJECT
public:

    AstrometryLogger();

    /**
     * @brief logFromAstrometry is the C++ method called by astrometry.net to get the text so that the logging can happen
     * @param text is the information to be logged when ready
     */
    void logFromAstrometry(char* text);
private:

    QString logText;                    // The current text waiting to be logged
    QElapsedTimer timeSinceLastOutput;  // A record of how long ago logging last happened so we know if it is ready to log
    QTimer logUpdater;                  // A timer that times out periodically to log any waiting text

    /**
     * @brief updateLog actually emits the signal to the logger to put the text into a file or scrolling log if it is ready.
     */
    void updateLog();
    /**
     * @brief readyToLog keeps track of how long it has been since logging was last done so it doesn't overwhelm the logging file or scrolling log
     * @return true if it is ready
     */
    bool readyToLog();
signals:
    /**
     * @brief logOutput signals that there is infomation that should be printed to a log file or log window
     * @param logText the text to be logged
     */
    void logOutput(const QString &logText);
};

#else
typedef struct AstrometryLogger AstrometryLogger;
#endif

#ifdef __cplusplus
    #define EXPORT_C extern "C"
#else
    #define EXPORT_C
#endif

// This provides a C interface to the C++ method above so that logging can happen from astrometry.net
EXPORT_C void logFromAstrometry(AstrometryLogger* astroLogger, char* text);

#endif // ASTROMETRYLOGGER_H
