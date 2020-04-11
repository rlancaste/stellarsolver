#ifndef EXTERNALSEXTRACTORSOLVER_H
#define EXTERNALSEXTRACTORSOLVER_H

#include "sexysolver.h"
#include <QProcess>
#include <QPointer>

class ExternalSextractorSolver : public SexySolver
{
    Q_OBJECT
public:
    explicit ExternalSextractorSolver(Statistic imagestats, uint8_t *imageBuffer, QObject *parent);

    bool sextract(bool justSextract);
    bool solveField();
    QStringList getSolverArgsList();

    QString fileToSolve;
    int selectedStar;

    //System File Paths
    QString dirPath = QDir::homePath();
    QString tempPath = QDir::tempPath();
    QStringList indexFilePaths;
    QString sextractorBinaryPath;
    QString confPath;
    QString solverPath;
    QString wcsPath;

    QString sextractorFilePath;
    QString basePath = QDir::tempPath();

    void logSolver();
    void logSextractor();
    void runExternalSextractorAndSolver();
    void abort();

    void printSextractorOutput();
    bool getSextractorTable();
    bool getSolutionInformation();

private:
    QPointer<QProcess> solver;
    QPointer<QProcess> sextractorProcess;
    // This is information about the image
    Statistic stats;

signals:
    //This signals that the sextractor has found stars
    void starsFound();
    //This signals that there is infomation that should be printed to a log file or log window
    void logNeedsUpdating(QString logText);
    //This signals that the external sextractor is done.
    void sextractorFinished(int x);
    //This signals that astrometry.net has finished solving the image or has quit solving
    void finished(int x);
};

#endif // EXTERNALSEXTRACTORSOLVER_H
