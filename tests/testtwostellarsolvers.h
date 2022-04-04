#ifndef TESTTWOSTELLARSOLVERS_H
#define TESTTWOSTELLARSOLVERS_H

#include <stdio.h>
#include <QApplication>
#include <QObject>

class TestTwoStellarSolvers : public QObject
{
    Q_OBJECT
public:
    TestTwoStellarSolvers();
    void setupStellarSolver(QString fileName);
    void setupStellarStarExtraction(QString fileName);
public slots:
    void stellarSolverFinished();
    void stellarExtractorFinished();
    void logOutput(QString text);
private:
    int solversRunning;
    int extractorsRunning;
};

#endif // TESTTWOSTELLARSOLVERS_H
