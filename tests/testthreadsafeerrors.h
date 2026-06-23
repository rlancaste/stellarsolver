#ifndef TESTTHREADSAFEERRORS_H
#define TESTTHREADSAFEERRORS_H

#include <stdio.h>
#include <QApplication>
#include <QObject>
#include <QtConcurrent>
#include <QMutex>
#include <set>
#include <string>

class TestThreadSafeErrors : public QObject
{
    Q_OBJECT
public:
    TestThreadSafeErrors();
    void runConcurrentErrors();
    void runConcurrentSolverNum();
    void runConcurrentKDTree();

private:
    QMutex seenNamesMutex;
    std::set<std::string> seenNames;
};

#endif // TESTTHREADSAFEERRORS_H
