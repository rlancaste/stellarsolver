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
public slots:
    void stellarSolverFinished();
    void logOutput(QString text);
private:
    int solversRunning;
};

#endif // DEMOTWOSTELLARSOLVERS_H
