#ifndef DEMOTWOSTELLARSOLVERS_H
#define DEMOTWOSTELLARSOLVERS_H

#include <stdio.h>
#include <QApplication>
#include <QObject>

class DemoTwoStellarSolvers : public QObject
{
    Q_OBJECT
public:
    DemoTwoStellarSolvers();
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

#endif // DEMOTWOSTELLARSOLVERS_H
