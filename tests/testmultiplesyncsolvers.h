#ifndef TESTMULTIPLESYNCSOLVERS_H
#define TESTMULTIPLESYNCSOLVERS_H

//Qt Includes
#include <QApplication>
#include <QObject>
#include <QtConcurrent>

#include <stdio.h>

//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "ssolverutils/fileio.h"


class TestMultipleSyncSolvers : public QObject
{
    Q_OBJECT
public:
    TestMultipleSyncSolvers();
     uint8_t * loadImageBuffer(FITSImage::Statistic &stats, QString fileName);
    void runSynchronousSolve(const FITSImage::Statistic &stats, const uint8_t *imageBuffer);
    void runSynchronousSEP(const FITSImage::Statistic &stats, const uint8_t *imageBuffer);
public slots:
    void logOutput(QString text);
private:
    int solversRunning;
    int extractorsRunning;
};

#endif // TESTMULTIPLESYNCSOLVERS_H
