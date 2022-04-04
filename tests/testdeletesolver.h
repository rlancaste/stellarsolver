#ifndef TESTDELETESOLVER_H
#define TESTDELETESOLVER_H

#include <stdio.h>
#include <QApplication>
#include <QObject>

class TestDeleteSolver : public QObject
{
public:
    TestDeleteSolver();
    void runCrashTest(QString fileName);
    void runCrashTestStarExtract(QString fileName);
};

#endif // TESTDELETESOLVER_H
