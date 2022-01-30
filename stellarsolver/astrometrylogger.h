#ifndef ASTROMETRYLOGGER_H
#define ASTROMETRYLOGGER_H

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

#endif // ASTROMETRYLOGGER_H
