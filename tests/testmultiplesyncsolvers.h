#ifndef TESTMULTIPLESYNCSOLVERS_H
#define TESTMULTIPLESYNCSOLVERS_H

#include <stdio.h>
#include <QApplication>
#include <QObject>
//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "ssolverutils/fileio.h"
#include <QtConcurrent>

class TestMultipleSyncSolvers : public QObject
{
    Q_OBJECT
public:
    TestMultipleSyncSolvers();
     uint8_t * loadImageBuffer(FITSImage::Statistic &stats, QString fileName);
    void runSynchronousSolve(FITSImage::Statistic &stats, uint8_t *imageBuffer);
    void runSynchronousSEP(FITSImage::Statistic &stats, uint8_t *imageBuffer);
public slots:
    void logOutput(QString text);
private:
    int solversRunning;
    int extractorsRunning;
};

#endif // TESTMULTIPLESYNCSOLVERS_H
