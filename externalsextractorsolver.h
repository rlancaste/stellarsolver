/*  ExternalSextractorSolver for SexySolver Tester Application, developed by Robert Lancaster, 2020

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

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
    void SEPAndSolve();
    void classicSolve();
    void astapSolve();

    void abort();
    bool isRunning();

    bool runExternalSextractor();
    bool runExternalSolver();
    bool runExternalClassicSolver();
    bool runExternalASTAPSolver();
    QStringList getSolverArgsList();
    QStringList getClassicSolverArgsList();
    bool generateAstrometryConfigFile();

    QString fileToProcess;
    bool fileToProcessIsTempFile = false;
    int selectedStar;

    //External Options
    bool cleanupTemporaryFiles = true;
    bool autoGenerateAstroConfig = true;

    //System File Paths
    QString dirPath = QDir::homePath();
    QStringList indexFilePaths;
    QString astapBinaryPath;
    QString sextractorBinaryPath;
    QString confPath;
    QString solverPath;
    QString wcsPath;

    //Methods to Set File Paths Automatically
    void setLinuxDefaultPaths();
    void setLinuxInternalPaths();
    void setMacHomebrewPaths();
    void setMacInternalPaths();
    void setWinANSVRPaths();
    void setWinCygwinPaths();

    // This is the xyls file path that sextractor will be saving for Astrometry.net
    // If it is not set, it will be set to a random temporary file
    QString sextractorFilePath;
    bool sextractorFilePathIsTempFile = false; //This will be set to true if it gets generated

    void logSolver();
    void logSextractor();


    void printSextractorOutput();
    bool writeSextractorTable();
    bool getSextractorTable();
    bool getSolutionInformation();
    bool getASTAPSolutionInformation();
    bool saveAsFITS();
    void cleanupTempFiles();

private:
    QPointer<QProcess> solver;
    QPointer<QProcess> sextractorProcess;
    // This is information about the image
    Statistic stats;

    //These are used for reading and writing the sextractor file
    char* xcol=strdup("X_IMAGE"); //This is the column for the x-coordinates
    char* ycol=strdup("Y_IMAGE"); //This is the column for the y-coordinates
    char* magcol=strdup("MAG_AUTO"); //This is the column for the magnitude
    char* colFormat=strdup("1E"); //This Format means a decimal number
    char* colUnits=strdup("pixels"); //This is the unit for the xy columns in the file
    char* magUnits=strdup("magnitude"); //This is the unit for the magnitude in the file

};

#endif // EXTERNALSEXTRACTORSOLVER_H
