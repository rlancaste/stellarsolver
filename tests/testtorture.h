#ifndef TESTTORTURE_H
#define TESTTORTURE_H

#include <QApplication>
#include <QObject>
#include <QtConcurrent>
#include <QMutex>
#include <QAtomicInt>
#include <stdio.h>

#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "ssolverutils/fileio.h"

class TestTorture : public QObject
{
    Q_OBJECT
public:
    TestTorture();

    void runConcurrentExtractFlood();
    void runConcurrentSolveFlood();
    void runInterleavedSolveExtract();
    void runRapidCreateDestroy();
    void runParallelProfileStress();
    void runExtractDifferentParams();
    void runRepeatedLoadSolve();

private:
    bool loadImages();
    bool establishBaseline();
    StellarSolver *makeSolver(const FITSImage::Statistic &stats,
                              const uint8_t *buf,
                              SSolver::Parameters::ParametersProfile profile =
                                  SSolver::Parameters::SINGLE_THREAD_SOLVING);

    FITSImage::Statistic m_statsRandom;
    FITSImage::Statistic m_statsPleiades;
    uint8_t *m_bufRandom{nullptr};
    uint8_t *m_bufPleiades{nullptr};

    double m_baselineRA{0};
    double m_baselineDec{0};
    int m_baselineRandomStars{0};
    int m_baselinePleiadesStars{0};

    QAtomicInt m_failCount;
};

#endif // TESTTORTURE_H
