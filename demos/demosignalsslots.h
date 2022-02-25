#ifndef DEMOSIGNALSSLOTS_H
#define DEMOSIGNALSSLOTS_H

#include <stdio.h>
#include <QApplication>
#include <QObject>

class DemoSignalsSlots : public QObject
{
    Q_OBJECT
public:
    DemoSignalsSlots();
    void setupStellarSolver(QString fileName);
public slots:
    void stellarSolverFinished();
    void logOutput(QString text);
};

#endif // DEMOSIGNALSSLOTS_H
