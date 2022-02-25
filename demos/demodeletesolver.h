#ifndef DEMODELETESOLVER_H
#define DEMODELETESOLVER_H

#include <stdio.h>
#include <QApplication>
#include <QObject>

class DemoDeleteSolver : public QObject
{
public:
    DemoDeleteSolver();
    void runCrashTest(QString fileName);
};

#endif // DEMODELETESOLVER_H
