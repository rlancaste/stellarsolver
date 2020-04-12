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

    //These are the methods that you can use for the External Programs to Sextract or Solve
    void sextract();
    void sextractAndSolve();

    void abort();
    bool isRunning();

    bool runExternalSextractor();
    bool runExternalSolver();
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

    // This is the xyls file path that sextractor will be saving for Astrometry.net
    // If it is not set, it will be set to a random temporary file
    QString sextractorFilePath;
    QString basePath = QDir::tempPath();

    void logSolver();
    void logSextractor();


    void printSextractorOutput();
    bool writeSextractorTable();
    bool getSextractorTable();
    bool getSolutionInformation();

private:
    QPointer<QProcess> solver;
    QPointer<QProcess> sextractorProcess;
    // This is information about the image
    Statistic stats;

    //These are used for reading and writing the sextractor file
    char* xcol=strdup("X_IMAGE"); //This is the column for the x-coordinates
    char* ycol=strdup("Y_IMAGE"); //This is the column for the y-coordinates
    char* colFormat=strdup("1E"); //This Format means a decimal number
    char* colUnits=strdup("pixels"); //This is the unit for the columns in the file

};

#endif // EXTERNALSEXTRACTORSOLVER_H
